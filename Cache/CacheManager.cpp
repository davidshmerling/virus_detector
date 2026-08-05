#include "Cache/CacheManager.h"

#include <system_error>
#include <utility>

namespace fs = std::filesystem;

CacheManager::CacheManager(
    std::unique_ptr<CacheRepository> repository,
    Logger& logger,
    PerformanceProfiler& profiler,
    std::size_t flush_interval)
    : repository_(std::move(repository)),
      logger_(logger),
      profiler_(profiler),
      flush_interval_(flush_interval == 0 ? 1 : flush_interval)
{
}

bool CacheManager::initialize()
{
    std::unique_lock lock(mutex_);

    if (!repository_) {
        logger_.error("Cache repository is missing");
        return false;
    }

    if (!repository_->initialize()) {
        logger_.error("Could not initialize cache repository");
        return false;
    }

    if (!repository_->load(entries_)) {
        logger_.error("Could not load cache");
        return false;
    }

    dirty_paths_.clear();
    removed_paths_.clear();

    logger_.info(
        "Cache loaded. Entries: " +
        std::to_string(entries_.size()));

    return true;
}

std::size_t CacheManager::pendingChangesLocked() const
{
    return dirty_paths_.size() + removed_paths_.size();
}

std::optional<FileVerdict> CacheManager::getValidVerdict(
    const fs::path& file_path,
    std::int64_t signatures_last_modified) const
{
    ScopedPerformanceTimer timer(
        profiler_,
        PerformanceSection::CacheValidation);

    const std::string key = pathKey(file_path);

    CacheEntry entry;
    bool found = false;

    {
        std::shared_lock lock(mutex_);

        const auto iterator = entries_.find(key);
        if (iterator == entries_.end()) {
            return std::nullopt;
        }

        entry = iterator->second;
        found = true;
    }

    if (!found) {
        return std::nullopt;
    }

    // Filesystem metadata outside the cache lock.
    std::int64_t current_file_time = 0;
    std::uintmax_t current_file_size = 0;
    if (!getFileIdentity(file_path, current_file_time, current_file_size)) {
        return std::nullopt;
    }

    if (entry.file_last_modified != current_file_time) {
        return std::nullopt;
    }

    if (entry.file_size != current_file_size) {
        return std::nullopt;
    }

    if (entry.signatures_last_modified != signatures_last_modified) {
        return std::nullopt;
    }

    return entry.verdict;
}

bool CacheManager::update(
    const fs::path& file_path,
    std::int64_t signatures_last_modified,
    FileVerdict verdict)
{
    if (verdict != FileVerdict::Clean) {
        return true;
    }

    // Filesystem work outside the cache lock.
    std::int64_t file_last_modified = 0;
    std::uintmax_t file_size = 0;
    if (!getFileIdentity(file_path, file_last_modified, file_size)) {
        logger_.warning(
            "Could not read file identity: " +
            file_path.string());
        return false;
    }

    const std::string key = pathKey(file_path);

    CacheEntry entry;
    entry.file_last_modified = file_last_modified;
    entry.file_size = file_size;
    entry.signatures_last_modified = signatures_last_modified;
    entry.verdict = verdict;

    bool should_flush = false;

    {
        ScopedPerformanceTimer timer(
            profiler_,
            PerformanceSection::CacheUpdate);

        std::unique_lock lock(mutex_);

        entries_[key] = entry;
        dirty_paths_.insert(key);
        removed_paths_.erase(key);

        should_flush = pendingChangesLocked() >= flush_interval_;
    }

    if (should_flush) {
        return flush();
    }

    return true;
}

bool CacheManager::remove(const fs::path& file_path)
{
    const std::string key = pathKey(file_path);
    bool should_flush = false;

    {
        std::unique_lock lock(mutex_);

        const std::size_t removed = entries_.erase(key);
        if (removed == 0) {
            return true;
        }

        dirty_paths_.erase(key);
        removed_paths_.insert(key);

        should_flush = pendingChangesLocked() >= flush_interval_;
    }

    if (should_flush) {
        return flush();
    }

    return true;
}

bool CacheManager::flush()
{
    CacheMap snapshot;
    CacheMap dirty_entries;
    std::unordered_set<std::string> removed_snapshot;
    std::size_t total_entries = 0;

    {
        std::unique_lock lock(mutex_);

        if (pendingChangesLocked() == 0) {
            return true;
        }

        dirty_entries.reserve(dirty_paths_.size());

        for (const std::string& path : dirty_paths_) {
            const auto iterator = entries_.find(path);
            if (iterator != entries_.end()) {
                dirty_entries.emplace(path, iterator->second);
            }
        }

        // SQLite uses dirty/removed only. Avoid copying the full map under lock.
        // JSON backend would need a full snapshot — swap carefully if reused.
        removed_snapshot = std::move(removed_paths_);
        total_entries = entries_.size();

        dirty_paths_.clear();
        removed_paths_.clear();
    }

    ScopedPerformanceTimer timer(
        profiler_,
        PerformanceSection::CacheJsonSave);

    if (!repository_->save(snapshot, dirty_entries, removed_snapshot)) {
        std::unique_lock lock(mutex_);

        for (const auto& [path, entry] : dirty_entries) {
            (void)entry;
            dirty_paths_.insert(path);
        }

        for (const std::string& path : removed_snapshot) {
            removed_paths_.insert(path);
        }

        logger_.error("Could not save cache");
        return false;
    }

    logger_.info(
        "Cache saved. Entries: " +
        std::to_string(total_entries) +
        ", dirty: " +
        std::to_string(dirty_entries.size()) +
        ", removed: " +
        std::to_string(removed_snapshot.size()));

    return true;
}

std::size_t CacheManager::size() const
{
    std::shared_lock lock(mutex_);
    return entries_.size();
}

std::string CacheManager::pathKey(const fs::path& path)
{
    std::error_code error;

    fs::path normalized = fs::weakly_canonical(path, error);
    if (error) {
        normalized = fs::absolute(path, error);
        if (error) {
            normalized = path.lexically_normal();
        }
    }

    return normalized.generic_string();
}

bool CacheManager::getFileIdentity(
    const fs::path& path,
    std::int64_t& last_modified,
    std::uintmax_t& file_size)
{
    std::error_code error;

    const fs::file_time_type file_time =
        fs::last_write_time(path, error);

    if (error) {
        return false;
    }

    const std::uintmax_t size = fs::file_size(path, error);
    if (error) {
        return false;
    }

    last_modified = static_cast<std::int64_t>(
        file_time.time_since_epoch().count());
    file_size = size;

    return true;
}

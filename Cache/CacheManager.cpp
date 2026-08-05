#include "Cache/CacheManager.h"

#include <system_error>
#include <utility>

namespace fs = std::filesystem;

CacheManager::CacheManager(
    std::unique_ptr<CacheRepository> repository,
    Logger& logger,
    std::size_t flush_interval)
    : repository_(std::move(repository)),
      logger_(logger),
      flush_interval_(flush_interval == 0 ? 1 : flush_interval)
{
}

bool CacheManager::initialize()
{
    std::scoped_lock lock(mutex_);

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

    logger_.info(
        "Cache loaded. Entries: " +
        std::to_string(entries_.size()));

    return true;
}

std::optional<FileVerdict> CacheManager::getValidVerdict(
    const fs::path& file_path,
    std::int64_t signatures_last_modified) const
{
    std::scoped_lock lock(mutex_);

    const auto iterator = entries_.find(pathKey(file_path));
    if (iterator == entries_.end()) {
        return std::nullopt;
    }

    std::int64_t current_file_time = 0;
    std::uintmax_t current_file_size = 0;
    if (!getFileIdentity(file_path, current_file_time, current_file_size)) {
        return std::nullopt;
    }

    const CacheEntry& entry = iterator->second;

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
    std::scoped_lock lock(mutex_);

    if (verdict != FileVerdict::Clean) {
        return true;
    }

    std::int64_t file_last_modified = 0;
    std::uintmax_t file_size = 0;
    if (!getFileIdentity(file_path, file_last_modified, file_size)) {
        logger_.warning(
            "Could not read file identity: " +
            file_path.string());
        return false;
    }

    CacheEntry entry;
    entry.file_last_modified = file_last_modified;
    entry.file_size = file_size;
    entry.signatures_last_modified = signatures_last_modified;
    entry.verdict = verdict;

    entries_[pathKey(file_path)] = entry;
    ++dirty_count_;

    if (dirty_count_ >= flush_interval_) {
        return flushUnlocked();
    }

    return true;
}

bool CacheManager::remove(const fs::path& file_path)
{
    std::scoped_lock lock(mutex_);

    const std::size_t removed = entries_.erase(pathKey(file_path));
    if (removed == 0) {
        return true;
    }

    ++dirty_count_;

    if (dirty_count_ >= flush_interval_) {
        return flushUnlocked();
    }

    return true;
}

bool CacheManager::flush()
{
    std::scoped_lock lock(mutex_);
    return flushUnlocked();
}

bool CacheManager::flushUnlocked()
{
    if (dirty_count_ == 0) {
        return true;
    }

    if (!repository_->save(entries_)) {
        logger_.error("Could not save cache");
        return false;
    }

    logger_.info(
        "Cache saved. Entries: " +
        std::to_string(entries_.size()));

    dirty_count_ = 0;
    return true;
}

std::size_t CacheManager::size() const
{
    std::scoped_lock lock(mutex_);
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

#include "Cache/CacheManager.h"

#include <chrono>
#include <system_error>
#include <utility>

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

namespace fs = std::filesystem;

CacheManager::CacheManager(
    std::unique_ptr<CacheRepository> repository,
    Logger& logger,
    PerformanceProfiler& profiler,
    std::size_t flush_interval,
    std::size_t submit_batch_size)
    : repository_(std::move(repository)),
      logger_(logger),
      profiler_(profiler),
      flush_interval_(flush_interval == 0 ? 1 : flush_interval),
      submit_batch_size_(submit_batch_size == 0 ? 1 : submit_batch_size)
{
}

CacheManager::~CacheManager()
{
    {
        std::scoped_lock lock(queue_mutex_);
        writer_stop_ = true;
        flush_requested_ = true;
    }

    queue_cv_.notify_all();

    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
}

void CacheManager::setOnDurableComplete(DurableCompleteCallback callback)
{
    on_durable_complete_ = std::move(callback);
}

void CacheManager::notifyCompleted(
    const std::vector<fs::path>& paths)
{
    if (!on_durable_complete_) {
        return;
    }

    for (const fs::path& progress_path : paths) {
        if (progress_path.empty()) {
            continue;
        }

        if (!on_durable_complete_(progress_path)) {
            logger_.error(
                "Could not mark durable progress complete: " +
                progress_path.string());
        }
    }
}

bool CacheManager::initialize()
{
    {
        std::unique_lock lock(map_mutex_);

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
    }

    if (!writer_started_) {
        writer_stop_ = false;
        flush_requested_ = false;
        writer_thread_ = std::thread([this]() { writerLoop(); });
        writer_started_ = true;
    }

    return true;
}

void CacheManager::enqueueBatch(std::vector<CacheUpsert> batch)
{
    if (batch.empty()) {
        return;
    }

    {
        const auto lock_wait_start = std::chrono::steady_clock::now();
        std::scoped_lock lock(queue_mutex_);
        const auto lock_acquired = std::chrono::steady_clock::now();

        profiler_.addMeasurement(
            PerformanceSection::CacheUpdateLockWait,
            lock_acquired - lock_wait_start);

        upsert_queue_.push_back(std::move(batch));
    }

    queue_cv_.notify_one();
}

void CacheManager::flushPendingBatches()
{
    std::vector<std::vector<CacheUpsert>> batches;

    {
        std::scoped_lock lock(pending_mutex_);

        for (auto& [thread_id, batch] : pending_batches_) {
            (void)thread_id;
            if (!batch.empty()) {
                batches.push_back(std::move(batch));
            }
        }

        pending_batches_.clear();
    }

    for (auto& batch : batches) {
        enqueueBatch(std::move(batch));
    }
}

std::optional<FileVerdict> CacheManager::getValidVerdict(
    const fs::path& file_path,
    std::int64_t signatures_last_modified) const
{
    const std::string key = pathKey(file_path);

    CacheEntry entry;
    bool found = false;
    std::chrono::nanoseconds work_time{0};

    {
        const auto lock_wait_start = std::chrono::steady_clock::now();
        std::shared_lock lock(map_mutex_);
        const auto lock_acquired = std::chrono::steady_clock::now();

        profiler_.addMeasurement(
            PerformanceSection::CacheLookupLockWait,
            lock_acquired - lock_wait_start);

        const auto iterator = entries_.find(key);
        if (iterator != entries_.end()) {
            entry = iterator->second;
            found = true;
        }

        work_time += std::chrono::steady_clock::now() - lock_acquired;
    }

    if (!found) {
        profiler_.addMeasurement(
            PerformanceSection::CacheLookupWork,
            work_time);
        return std::nullopt;
    }

    const auto identity_start = std::chrono::steady_clock::now();

    std::int64_t current_file_time = 0;
    std::uintmax_t current_file_size = 0;
    if (!getFileIdentity(file_path, current_file_time, current_file_size)) {
        work_time += std::chrono::steady_clock::now() - identity_start;
        profiler_.addMeasurement(
            PerformanceSection::CacheLookupWork,
            work_time);
        return std::nullopt;
    }

    const bool valid =
        entry.file_last_modified == current_file_time &&
        entry.file_size == current_file_size &&
        entry.signatures_last_modified == signatures_last_modified;

    work_time += std::chrono::steady_clock::now() - identity_start;
    profiler_.addMeasurement(
        PerformanceSection::CacheLookupWork,
        work_time);

    if (!valid) {
        return std::nullopt;
    }

    return entry.verdict;
}

bool CacheManager::update(
    const fs::path& file_path,
    const fs::path& progress_path,
    std::int64_t signatures_last_modified,
    FileVerdict verdict)
{
    if (verdict != FileVerdict::Clean) {
        return true;
    }

    const auto work_start = std::chrono::steady_clock::now();

    std::int64_t file_last_modified = 0;
    std::uintmax_t file_size = 0;
    if (!getFileIdentity(file_path, file_last_modified, file_size)) {
        logger_.warning(
            "Could not read file identity: " +
            file_path.string());
        profiler_.addMeasurement(
            PerformanceSection::CacheUpdateWork,
            std::chrono::steady_clock::now() - work_start);
        return false;
    }

    CacheUpsert upsert;
    upsert.path = pathKey(file_path);
    upsert.entry.file_last_modified = file_last_modified;
    upsert.entry.file_size = file_size;
    upsert.entry.signatures_last_modified = signatures_last_modified;
    upsert.entry.verdict = verdict;
    upsert.progress_path = progress_path;

    std::vector<CacheUpsert> ready_batch;

    {
        const auto lock_wait_start = std::chrono::steady_clock::now();
        std::scoped_lock lock(pending_mutex_);
        const auto lock_acquired = std::chrono::steady_clock::now();

        profiler_.addMeasurement(
            PerformanceSection::CacheUpdateLockWait,
            lock_acquired - lock_wait_start);

        auto& batch = pending_batches_[std::this_thread::get_id()];
        batch.push_back(std::move(upsert));

        if (batch.size() >= submit_batch_size_) {
            ready_batch = std::move(batch);
            batch.clear();
        }

        profiler_.addMeasurement(
            PerformanceSection::CacheUpdateWork,
            (lock_acquired - work_start) +
                (std::chrono::steady_clock::now() - lock_acquired));
    }

    if (!ready_batch.empty()) {
        enqueueBatch(std::move(ready_batch));
    }

    return true;
}

bool CacheManager::notifyDurableComplete(const fs::path& progress_path)
{
    if (progress_path.empty()) {
        return true;
    }

    {
        std::scoped_lock lock(queue_mutex_);
        complete_queue_.push_back(progress_path);
    }

    queue_cv_.notify_one();
    return true;
}

bool CacheManager::remove(const std::filesystem::path& file_path)
{
    const std::string key = pathKey(file_path);

    {
        std::scoped_lock lock(queue_mutex_);
        remove_queue_.push_back({key});
    }

    queue_cv_.notify_one();
    return true;
}

void CacheManager::applyBatchToMap(const std::vector<CacheUpsert>& batch)
{
    std::unique_lock lock(map_mutex_);

    for (const CacheUpsert& upsert : batch) {
        entries_[upsert.path] = upsert.entry;
    }
}

void CacheManager::applyRemovesToMap(const std::vector<std::string>& paths)
{
    if (paths.empty()) {
        return;
    }

    std::unique_lock lock(map_mutex_);

    for (const std::string& path : paths) {
        entries_.erase(path);
    }
}

void CacheManager::writerLoop()
{
    CacheMap dirty_entries;
    std::unordered_map<std::string, fs::path> dirty_progress;
    std::unordered_set<std::string> removed_paths;

    while (true) {
        std::vector<std::vector<CacheUpsert>> upsert_batches;
        std::vector<std::vector<std::string>> remove_batches;
        std::vector<fs::path> complete_now;
        bool stop = false;
        bool flush_now = false;

        {
            std::unique_lock lock(queue_mutex_);

            queue_cv_.wait(lock, [this]() {
                return writer_stop_ ||
                       flush_requested_ ||
                       !upsert_queue_.empty() ||
                       !remove_queue_.empty() ||
                       !complete_queue_.empty();
            });

            stop = writer_stop_;
            flush_now = flush_requested_;

            while (!upsert_queue_.empty()) {
                upsert_batches.push_back(std::move(upsert_queue_.front()));
                upsert_queue_.pop_front();
            }

            while (!remove_queue_.empty()) {
                remove_batches.push_back(std::move(remove_queue_.front()));
                remove_queue_.pop_front();
            }

            while (!complete_queue_.empty()) {
                complete_now.push_back(std::move(complete_queue_.front()));
                complete_queue_.pop_front();
            }

            writer_busy_ = true;
        }

        for (const auto& batch : upsert_batches) {
            applyBatchToMap(batch);

            for (const CacheUpsert& upsert : batch) {
                dirty_entries[upsert.path] = upsert.entry;
                dirty_progress[upsert.path] = upsert.progress_path;
                removed_paths.erase(upsert.path);
            }
        }

        for (const auto& batch : remove_batches) {
            applyRemovesToMap(batch);

            for (const std::string& path : batch) {
                dirty_entries.erase(path);
                dirty_progress.erase(path);
                removed_paths.insert(path);
            }
        }

        // Outcomes that are already durable (no new SQLite write).
        if (!complete_now.empty()) {
            notifyCompleted(complete_now);
        }

        const bool should_persist =
            dirty_entries.size() + removed_paths.size() >= flush_interval_ ||
            flush_now ||
            stop;

        if (should_persist &&
            (!dirty_entries.empty() || !removed_paths.empty())) {
            ScopedPerformanceTimer timer(
                profiler_,
                PerformanceSection::CachePersistence);

            CacheMap snapshot;
            const std::size_t dirty_count = dirty_entries.size();
            const std::size_t removed_count = removed_paths.size();

            if (!repository_->save(snapshot, dirty_entries, removed_paths)) {
                logger_.error("Could not save cache");
            } else {
                logger_.info(
                    "Cache saved. Entries: " +
                    std::to_string(size()) +
                    ", dirty: " +
                    std::to_string(dirty_count) +
                    ", removed: " +
                    std::to_string(removed_count));

                std::vector<fs::path> completed;
                completed.reserve(dirty_progress.size());

                for (const auto& [cache_key, progress_path] : dirty_progress) {
                    (void)cache_key;
                    if (!progress_path.empty()) {
                        completed.push_back(progress_path);
                    }
                }

                dirty_entries.clear();
                dirty_progress.clear();
                removed_paths.clear();

                // Checkpoint advances only after a successful COMMIT.
                notifyCompleted(completed);
            }
        }

        {
            std::scoped_lock lock(queue_mutex_);
            writer_busy_ = false;

            if (flush_now &&
                upsert_queue_.empty() &&
                remove_queue_.empty() &&
                complete_queue_.empty()) {
                flush_requested_ = false;
            }

            drain_cv_.notify_all();
        }

        if (stop &&
            upsert_batches.empty() &&
            remove_batches.empty() &&
            complete_now.empty()) {
            if (!dirty_entries.empty() || !removed_paths.empty()) {
                CacheMap snapshot;
                if (repository_->save(snapshot, dirty_entries, removed_paths)) {
                    std::vector<fs::path> completed;
                    for (const auto& [cache_key, progress_path] :
                         dirty_progress) {
                        (void)cache_key;
                        if (!progress_path.empty()) {
                            completed.push_back(progress_path);
                        }
                    }
                    notifyCompleted(completed);
                }
            }
            return;
        }
    }
}

bool CacheManager::flush()
{
    flushPendingBatches();

    {
        std::scoped_lock lock(queue_mutex_);
        flush_requested_ = true;
    }

    queue_cv_.notify_one();

    std::unique_lock lock(queue_mutex_);
    drain_cv_.wait(lock, [this]() {
        return !flush_requested_ &&
               !writer_busy_ &&
               upsert_queue_.empty() &&
               remove_queue_.empty() &&
               complete_queue_.empty();
    });

    return true;
}

std::size_t CacheManager::size() const
{
    std::shared_lock lock(map_mutex_);
    return entries_.size();
}

std::string CacheManager::pathKey(const fs::path& path)
{
    // Avoid weakly_canonical() — it hits the filesystem on every lookup.
    // Enumerator skips symlinks; absolute + lexical normalize is enough.
    std::error_code error;
    fs::path normalized = fs::absolute(path, error);
    if (error) {
        return path.lexically_normal().generic_string();
    }

    return normalized.lexically_normal().generic_string();
}

bool CacheManager::getFileIdentity(
    const fs::path& path,
    std::int64_t& last_modified,
    std::uintmax_t& file_size)
{
#if !defined(_WIN32)
    // One syscall instead of separate last_write_time + file_size.
    struct stat status {};
    if (::stat(path.c_str(), &status) != 0) {
        return false;
    }

    last_modified =
        static_cast<std::int64_t>(status.st_mtim.tv_sec) * 1'000'000'000LL +
        static_cast<std::int64_t>(status.st_mtim.tv_nsec);
    file_size = static_cast<std::uintmax_t>(status.st_size);
    return true;
#else
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
#endif
}

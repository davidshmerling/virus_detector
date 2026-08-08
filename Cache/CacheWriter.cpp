#include "Cache/CacheWriter.h"

#include <utility>

CacheWriter::CacheWriter(SqliteCacheManager& storage, std::size_t flush_threshold)
    : storage_(storage),
      flush_threshold_(flush_threshold == 0 ? 1 : flush_threshold),
      thread_([this]() { run(); })
{
}

CacheWriter::~CacheWriter()
{
    {
        std::scoped_lock lock(mutex_);
        stop_ = true;
        flush_requested_ = true;
    }
    cv_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void CacheWriter::submit(CacheEntry entry)
{
    {
        std::scoped_lock lock(mutex_);
        upsert_queue_.push_back(std::move(entry));
    }
    cv_.notify_one();
}

void CacheWriter::remove(std::string path)
{
    {
        std::scoped_lock lock(mutex_);
        remove_queue_.push_back(std::move(path));
    }
    cv_.notify_one();
}

void CacheWriter::flush()
{
    {
        std::scoped_lock lock(mutex_);
        flush_requested_ = true;
    }
    cv_.notify_one();

    std::unique_lock lock(mutex_);
    drain_cv_.wait(lock, [this]() {
        return !flush_requested_ &&
               !busy_ &&
               upsert_queue_.empty() &&
               remove_queue_.empty();
    });
}

void CacheWriter::persistLocked(
    std::unordered_map<std::string, CacheEntry>& dirty,
    std::vector<std::string>& removals)
{
    if (!removals.empty()) {
        (void)storage_.removeBatch(removals);
        removals.clear();
    }

    if (!dirty.empty()) {
        std::vector<CacheEntry> batch;
        batch.reserve(dirty.size());
        for (auto& [path, entry] : dirty) {
            (void)path;
            batch.push_back(std::move(entry));
        }
        dirty.clear();
        (void)storage_.upsertBatch(batch);
    }
}

void CacheWriter::run()
{
    std::unordered_map<std::string, CacheEntry> dirty;
    std::vector<std::string> removals;

    while (true) {
        std::deque<CacheEntry> upserts;
        std::deque<std::string> removes;
        bool stop = false;
        bool flush_now = false;

        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this]() {
                return stop_ ||
                       flush_requested_ ||
                       !upsert_queue_.empty() ||
                       !remove_queue_.empty();
            });

            stop = stop_;
            flush_now = flush_requested_;
            upserts.swap(upsert_queue_);
            removes.swap(remove_queue_);
            busy_ = true;
        }

        for (CacheEntry& entry : upserts) {
            dirty[entry.path] = std::move(entry);
        }
        for (std::string& path : removes) {
            dirty.erase(path);
            removals.push_back(std::move(path));
        }

        const bool should_persist =
            dirty.size() + removals.size() >= flush_threshold_ ||
            flush_now ||
            stop;

        if (should_persist) {
            persistLocked(dirty, removals);
        }

        {
            std::scoped_lock lock(mutex_);
            busy_ = false;
            if (flush_now && upsert_queue_.empty() && remove_queue_.empty()) {
                flush_requested_ = false;
            }
            drain_cv_.notify_all();
        }

        if (stop && upserts.empty() && removes.empty()) {
            persistLocked(dirty, removals);
            return;
        }
    }
}

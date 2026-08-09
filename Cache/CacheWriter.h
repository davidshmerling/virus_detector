#pragma once

#include "Cache/CacheEntry.h"
#include "Cache/SqliteCacheManager.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Async SQLite writer — queue + single thread only.
class CacheWriter {
public:
    explicit CacheWriter(SqliteCacheManager& storage);
    ~CacheWriter();

    CacheWriter(const CacheWriter&) = delete;
    CacheWriter& operator=(const CacheWriter&) = delete;

    void submit(CacheEntry entry);
    void remove(std::string path);
    void flush();

private:
    void run();
    void persistLocked(
        std::unordered_map<std::string, CacheEntry>& dirty,
        std::vector<std::string>& removals);

    SqliteCacheManager& storage_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable drain_cv_;

    std::deque<CacheEntry> upsert_queue_;
    std::deque<std::string> remove_queue_;

    bool stop_ = false;
    bool flush_requested_ = false;
    bool busy_ = false;
    std::thread thread_;
};

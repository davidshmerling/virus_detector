#pragma once

#include "Cache/CacheEntry.h"
#include "Cache/SqliteCacheManager.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <thread>

// Async SQLite writer — upsert queue + single background thread.
// Writes when the queue reaches a batch of 100, or when stopped at end of scan.
// Stale-entry deletion is not its job; CacheManager does that after scan-all.
class CacheWriter {
public:
    explicit CacheWriter(SqliteCacheManager& storage);
    ~CacheWriter();

    CacheWriter(const CacheWriter&) = delete;
    CacheWriter& operator=(const CacheWriter&) = delete;

    void submit(CacheEntry entry);

    // Drains any remaining upserts to SQLite and joins the writer thread.
    // Call once at end of scan (before committing the generation).
    void finish();

private:
    void run();

    SqliteCacheManager& storage_;

    std::mutex mutex_;
    std::condition_variable cv_;

    std::deque<CacheEntry> upsert_queue_;

    bool stop_ = false;
    std::thread thread_;
};

#pragma once

#include "Cache/CacheEntry.h"
#include "Cache/SqliteCacheManager.h"

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <thread>

// Asynchronous SQLite writer: an upsert queue plus a single background thread.
// Writes when the queue reaches a batch of 100, or when stopped at end of scan.
// Stale-entry deletion is not its job; CacheManager does that after scan-all.
class CacheWriter {
public:
    explicit CacheWriter(SqliteCacheManager& storage);
    ~CacheWriter();

    CacheWriter(const CacheWriter&) = delete;
    CacheWriter& operator=(const CacheWriter&) = delete;

    // Enqueues `entry` for a later batch upsert.
    void submit(CacheEntry entry);

    // Drains any remaining upserts to SQLite and joins the writer thread.
    // Call once at end of scan (before committing the generation).
    void finish();

private:
    // Background loop: waits for a full batch or stop, then upserts.
    void run();

    SqliteCacheManager& storage_;

    std::mutex mutex_;
    std::condition_variable cv_;

    std::deque<CacheEntry> upsert_queue_;

    bool stop_ = false;
    std::thread thread_;
};

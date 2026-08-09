#include "Cache/CacheWriter.h"

#include <utility>
#include <vector>

namespace {

// Write to SQLite once this many pending upserts pile up. Anything left under
// the batch size is written when finish() stops the writer at end of scan.
constexpr std::size_t kBatchSize = 100;

}  // namespace

CacheWriter::CacheWriter(SqliteCacheManager& storage)
    : storage_(storage),
      thread_([this]() { run(); })
{
}

CacheWriter::~CacheWriter()
{
    // Safety net if the scan never reached finish() (e.g. early abort).
    finish();
}

void CacheWriter::submit(CacheEntry entry)
{
    bool should_wake = false;
    {
        std::scoped_lock lock(mutex_);
        if (stop_) {
            return;
        }
        upsert_queue_.push_back(std::move(entry));
        if (upsert_queue_.size() >= kBatchSize) {
            should_wake = true;
        }
    }
    if (should_wake) {
        cv_.notify_one();
    }
}

void CacheWriter::finish()
{
    {
        std::scoped_lock lock(mutex_);
        stop_ = true;
    }
    cv_.notify_one();
    if (thread_.joinable()) {
        thread_.join();
    }
}

void CacheWriter::run()
{
    while (true) {
        std::deque<CacheEntry> upserts;
        bool stop = false;

        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this]() {
                return stop_ || upsert_queue_.size() >= kBatchSize;
            });

            stop = stop_;
            upserts.swap(upsert_queue_);
        }

        // One update per path per scan — no dedup map needed.
        if (!upserts.empty()) {
            std::vector<CacheEntry> batch(
                std::make_move_iterator(upserts.begin()),
                std::make_move_iterator(upserts.end()));
            (void)storage_.upsertBatch(batch);
        }

        if (stop) {
            return;
        }
    }
}

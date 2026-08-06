#pragma once

#include "Cache/CacheRepository.h"
#include "Common/FileVerdict.h"
#include "Logger/Logger.h"
#include "Performance/PerformanceProfiler.h"

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct CacheUpsert {
    std::string path;
    CacheEntry entry;
    // Relative scan path; completed in ProgressTracker only after SQLite COMMIT.
    std::filesystem::path progress_path;
};

class CacheManager {
public:
    using DurableCompleteCallback =
        std::function<bool(const std::filesystem::path& progress_path)>;

    CacheManager(
        std::unique_ptr<CacheRepository> repository,
        Logger& logger,
        PerformanceProfiler& profiler,
        std::size_t flush_interval = 100,
        std::size_t submit_batch_size = 10);

    ~CacheManager();

    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;

    void setOnDurableComplete(DurableCompleteCallback callback);

    bool initialize();

    std::optional<FileVerdict> getValidVerdict(
        const std::filesystem::path& file_path,
        std::int64_t signatures_last_modified) const;

    // Workers only enqueue upserts (batched). Map + SQLite are updated
    // by the dedicated writer thread. Progress advances after COMMIT.
    bool update(
        const std::filesystem::path& file_path,
        const std::filesystem::path& progress_path,
        std::int64_t signatures_last_modified,
        FileVerdict verdict);

    // Already-durable outcomes (cache hit, quarantine, error): advance
    // ProgressTracker via the writer thread, without a cache write.
    bool notifyDurableComplete(const std::filesystem::path& progress_path);

    bool remove(const std::filesystem::path& file_path);

    // Wait until pending batches are applied and dirty entries are persisted.
    bool flush();

    std::size_t size() const;

private:
    static std::string pathKey(const std::filesystem::path& path);

    static bool getFileIdentity(
        const std::filesystem::path& path,
        std::int64_t& last_modified,
        std::uintmax_t& file_size);

    void enqueueBatch(std::vector<CacheUpsert> batch);
    void flushPendingBatches();
    void writerLoop();
    void applyBatchToMap(const std::vector<CacheUpsert>& batch);
    void applyRemovesToMap(const std::vector<std::string>& paths);
    void notifyCompleted(const std::vector<std::filesystem::path>& paths);

    std::unique_ptr<CacheRepository> repository_;
    Logger& logger_;
    PerformanceProfiler& profiler_;
    DurableCompleteCallback on_durable_complete_;

    // Shared for lookups; exclusive only for the writer thread.
    mutable std::shared_mutex map_mutex_;
    CacheMap entries_;

    // Per-worker submit batches (keyed by thread id). Brief lock only.
    std::mutex pending_mutex_;
    std::unordered_map<std::thread::id, std::vector<CacheUpsert>> pending_batches_;

    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::condition_variable drain_cv_;
    std::deque<std::vector<CacheUpsert>> upsert_queue_;
    std::deque<std::vector<std::string>> remove_queue_;
    std::deque<std::filesystem::path> complete_queue_;

    bool writer_stop_ = false;
    bool flush_requested_ = false;
    bool writer_busy_ = false;
    bool writer_started_ = false;

    std::thread writer_thread_;

    std::size_t flush_interval_;
    std::size_t submit_batch_size_;
};

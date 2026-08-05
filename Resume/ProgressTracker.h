#pragma once

#include "Logger/Logger.h"
#include "Resume/JsonCheckpointRepository.h"
#include "Resume/ScanCheckpoint.h"

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <set>
#include <string>

class ProgressTracker {
public:
    ProgressTracker(
        JsonCheckpointRepository& repository,
        Logger& logger,
        std::size_t flush_interval = 10);

    bool startNewScan(const std::filesystem::path& root);
    bool resumeScan(const ScanCheckpoint& checkpoint);

    bool registerTask(const std::filesystem::path& relative_path);
    bool cancelTask(const std::filesystem::path& relative_path);
    bool markCompleted(const std::filesystem::path& relative_path);
    bool markEnumerationFinished();

    // Flush any pending checkpoint updates to disk.
    bool flush();

    const ScanCheckpoint& checkpoint() const;

private:
    static std::string pathKey(const std::filesystem::path& relative_path);

    void refreshNextPathLocked();
    bool saveLocked(bool force);

    std::set<std::string> unfinished_paths_;
    std::mutex mutex_;

    ScanCheckpoint checkpoint_;
    JsonCheckpointRepository& repository_;
    Logger& logger_;

    std::size_t flush_interval_;
    std::size_t dirty_count_ = 0;

    bool enumeration_finished_ = false;
};

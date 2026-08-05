#pragma once

#include "Logger/Logger.h"
#include "Resume/JsonCheckpointRepository.h"
#include "Resume/ScanCheckpoint.h"

#include <filesystem>
#include <mutex>
#include <set>
#include <string>

class ProgressTracker {
public:
    ProgressTracker(
        JsonCheckpointRepository& repository,
        Logger& logger);

    bool startNewScan(const std::filesystem::path& root);
    bool resumeScan(const ScanCheckpoint& checkpoint);

    bool registerTask(const std::filesystem::path& relative_path);
    bool cancelTask(const std::filesystem::path& relative_path);
    bool markCompleted(const std::filesystem::path& relative_path);
    bool markEnumerationFinished();

    bool flush();

    const ScanCheckpoint& checkpoint() const;

private:
    static std::string pathKey(const std::filesystem::path& relative_path);

    void updateNextUnfinished();
    bool saveLocked();

    std::set<std::string> unfinished_paths_;
    std::mutex mutex_;

    ScanCheckpoint checkpoint_;
    JsonCheckpointRepository& repository_;
    Logger& logger_;

    bool enumeration_finished_ = false;
};

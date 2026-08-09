#pragma once

#include "Logger/Logger.h"

#include <filesystem>
#include <mutex>
#include <set>
#include <string>

// Remembers which file a scan should resume from if it is interrupted.
//
// The checkpoint file has three lines:
//   line 1 = scan root
//   line 2 = running / completed
//   line 3 = next file to resume from (smallest unfinished path)
//
// While a scan runs, the set of unfinished files is kept in memory and the
// smallest one is written to disk as the safe resume point.
class ResumeManager {
public:
    ResumeManager(
        Logger& logger,
        std::filesystem::path checkpoint_file);

    // Start a new scan, or report that we are resuming a previous one.
    bool begin(
        const std::filesystem::path& scan_root,
        bool& resumed);

    // A discovered file was handed to the ThreadPool; it is now unfinished.
    bool addFile(const std::filesystem::path& file);

    // A worker finished processing this file.
    bool fileCompleted(const std::filesystem::path& file);

    // The FileTreeWalker finished discovering files.
    bool discoveryFinished();

    const std::filesystem::path& nextFile() const;

private:
    bool load(
        std::filesystem::path& root,
        std::string& status,
        std::filesystem::path& next);

    bool save(const std::string& status);

    // Writes a checkpoint only once every kCheckpointInterval calls, so we do
    // not hit the disk for every single file. State transitions still call
    // save() directly to guarantee they are persisted.
    bool maybeSave(const std::string& status);

    static constexpr int kCheckpointInterval = 200;
    int since_last_save_ = 0;

    Logger& logger_;

    std::filesystem::path checkpoint_file_;
    std::filesystem::path root_;
    std::filesystem::path next_file_;

    std::set<std::string> unfinished_files_;
    bool discovery_finished_ = false;

    std::mutex mutex_;
};

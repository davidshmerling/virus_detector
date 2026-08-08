#pragma once

#include "Logger/Logger.h"
#include "Quarantine/QuarantineEntry.h"
#include "Quarantine/QuarantineFileOperations.h"
#include "Quarantine/QuarantineRepository.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

// Drives the quarantine process. It owns the lock and decides the order of
// steps, delegating the actual work to two helpers:
//
//   QuarantineRepository      - holds and searches the records (metadata.json)
//   QuarantineFileOperations  - moves / restores / deletes the files
//
// The manager itself parses no JSON and moves no files; it only coordinates.
class QuarantineManager {
public:
    QuarantineManager(
        Logger& logger,
        std::filesystem::path quarantine_directory);

    // Prepare the folders and read existing metadata into memory.
    bool load();

    // Move a file into quarantine and record it.
    bool quarantine(const std::filesystem::path& file);

    // Put a quarantined file back at its original path.
    bool restore(const std::string& id);
    bool restoreAll();

    // Delete a quarantined file for good.
    bool remove(const std::string& id);
    bool removeAll();

    std::vector<QuarantineEntry> list() const;

private:
    // These *One helpers assume mutex_ is held and do not persist; the public
    // methods lock, call them, then let the repository save once.
    bool restoreOne(const std::string& id);
    bool removeOne(const std::string& id);

    static std::string currentTime();

    Logger& logger_;

    QuarantineRepository repository_;
    QuarantineFileOperations file_ops_;

    mutable std::mutex mutex_;
};

#pragma once

#include "Logger/Logger.h"
#include "Quarantine/QuarantineEntry.h"
#include "Quarantine/QuarantineFileOperations.h"
#include "Quarantine/QuarantineRepository.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

// Drives the quarantine process. Owns the lock and decides the order of steps,
// delegating the actual work to two helpers:
//
//   QuarantineRepository      — holds and searches the records (metadata.json)
//   QuarantineFileOperations  — moves, restores, and deletes the files
//
// The manager itself parses no JSON and moves no files; it only coordinates.
class QuarantineManager {
public:
    QuarantineManager(
        Logger& logger,
        std::filesystem::path quarantine_directory);

    // Prepares the folders and reads existing metadata into memory.
    bool load();

    // Moves `file` into quarantine and records it, together with the malware
    // signatures that matched during the scan.
    bool quarantine(
        const std::filesystem::path& file,
        const std::vector<std::string>& signatures);

    // Puts a quarantined file back at its original path.
    bool restore(const std::string& id);

    // Restores every quarantined file. Continues after individual failures;
    // returns false if any restore failed or metadata could not be saved.
    bool restoreAll();

    // Deletes a quarantined file permanently.
    bool remove(const std::string& id);

    // Returns a snapshot of all quarantine records.
    std::vector<QuarantineEntry> list() const;

private:
    // These *One helpers assume mutex_ is held and do not persist; the public
    // methods lock, call them, then let the repository save once.
    // Restores a single entry by `id` (move back + drop the record).
    bool restoreOne(const std::string& id);
    // Deletes a single quarantined file and drops its record.
    bool removeOne(const std::string& id);

    // Returns the current wall-clock timestamp as a readable string.
    static std::string currentTime();

    Logger& logger_;

    QuarantineRepository repository_;
    QuarantineFileOperations file_ops_;

    mutable std::mutex mutex_;
};

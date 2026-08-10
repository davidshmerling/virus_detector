#pragma once

#include "Quarantine/QuarantineEntry.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Holds the quarantine records in memory and persists them to metadata.json.
// Pure bookkeeping: looks up, adds, and removes records, but never touches the
// quarantined files themselves.
class QuarantineRepository {
public:
    explicit QuarantineRepository(std::filesystem::path metadata_file);

    // Reads the records from disk into memory. A missing file is treated as
    // empty and valid.
    bool load();

    // Writes the current records to disk atomically.
    bool save() const;

    // Appends `entry` to the in-memory list (does not persist by itself).
    void add(const QuarantineEntry& entry);

    // Removes the record with `id` from memory. Returns false if not found.
    bool removeById(const std::string& id);

    // Returns the record with `id`, or std::nullopt if missing.
    std::optional<QuarantineEntry> find(const std::string& id) const;

    // Returns a copy of all records.
    std::vector<QuarantineEntry> all() const;

private:
    std::filesystem::path metadata_file_;
    std::vector<QuarantineEntry> entries_;
};

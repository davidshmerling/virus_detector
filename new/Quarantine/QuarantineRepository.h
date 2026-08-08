#pragma once

#include "Quarantine/QuarantineEntry.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// Holds the quarantine records in memory and persists them to metadata.json.
// Pure bookkeeping: it looks up, adds and removes records, but never touches
// the quarantined files themselves.
class QuarantineRepository {
public:
    explicit QuarantineRepository(std::filesystem::path metadata_file);

    // Read the records from disk into memory (missing file = empty, valid).
    bool load();

    // Write the current records to disk atomically.
    bool save() const;

    void add(const QuarantineEntry& entry);
    bool removeById(const std::string& id);

    std::optional<QuarantineEntry> find(const std::string& id) const;
    std::vector<QuarantineEntry> all() const;

private:
    std::filesystem::path metadata_file_;
    std::vector<QuarantineEntry> entries_;
};

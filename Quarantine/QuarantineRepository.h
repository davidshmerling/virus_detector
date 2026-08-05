#pragma once

#include "Quarantine/QuarantineEntry.h"

#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class QuarantineRepository {
public:
    explicit QuarantineRepository(std::filesystem::path metadata_file);

    bool initialize();

    bool load(std::vector<QuarantineEntry>& entries) const;
    bool save(const std::vector<QuarantineEntry>& entries) const;

    std::optional<QuarantineEntry> findById(const std::string& id) const;
    bool add(const QuarantineEntry& entry);
    bool remove(const std::string& id);

    std::vector<QuarantineEntry> list() const;

private:
    std::filesystem::path metadata_file_;
    std::vector<QuarantineEntry> entries_;
    mutable std::mutex mutex_;
};

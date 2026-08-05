#pragma once

#include "Common/OperationResult.h"

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

// Loads user-excluded paths from a text file and answers isExcluded().
// System paths (/proc, /sys, /dev, /run, /tmp) are always excluded.
class ExcludeManager {
public:
    explicit ExcludeManager(std::string file_path);

    OperationResult load();

    bool isExcluded(const std::filesystem::path& path) const;

    const std::vector<std::filesystem::path>& getExcludedPaths() const;

private:
    static std::filesystem::path normalize(const std::filesystem::path& path);
    static std::string trim(const std::string& text);

    std::filesystem::path file_path_;
    std::vector<std::filesystem::path> excluded_paths_;
    std::unordered_set<std::string> excluded_lookup_;
};

#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>

// Owns the set of paths a scan must not touch: built-in defaults (system dirs
// and the scanner's own install/dev trees) plus the user's config-file
// exclusions. This class only owns the set and knows how to build it. Deciding
// whether a given path is excluded belongs to PathFilter (per DFS node) and
// ScanRootValidator (the scan root, once).
class ExcludeSet {
public:
    // Constructs a set that will also load user exclusions from `file_path`
    // (default: config/exclude.txt).
    explicit ExcludeSet(
        std::filesystem::path file_path = "config/exclude.txt");

    // Loads the built-in defaults plus the user's config file. Returns false
    // if the config file cannot be opened (defaults are still applied).
    bool load();

    // Returns the normalized excluded paths for the filters to query.
    const std::unordered_set<std::string>& excludedPaths() const
    {
        return excluded_paths_;
    }

    // Returns the canonical string form used both when storing a path and when
    // querying one, so the two always compare the same way.
    static std::string normalize(const std::filesystem::path& path);

private:
    std::filesystem::path file_path_;
    std::unordered_set<std::string> excluded_paths_;
};

#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>

// The set of paths a scan must not touch: built-in defaults (system dirs and
// the scanner's own install/dev trees) plus the user's config-file exclusions.
// This class only *owns* the set and knows how to build it. Deciding whether a
// given path is excluded is not its job — that belongs to PathFilter (per DFS
// node) and ScanRootGuard (the scan root, once).
class ExcludeSet {
public:
    explicit ExcludeSet(
        std::filesystem::path file_path = "config/exclude.txt");

    // Loads the built-in defaults plus the user's config file.
    bool load();

    // The normalized excluded paths, for the filters to query.
    const std::unordered_set<std::string>& excludedPaths() const
    {
        return excluded_paths_;
    }

    // Canonical string form used both when storing a path and when querying one,
    // so the two always compare the same way.
    static std::string normalize(const std::filesystem::path& path);

private:
    std::filesystem::path file_path_;
    std::unordered_set<std::string> excluded_paths_;
};

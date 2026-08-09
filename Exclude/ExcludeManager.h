#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>

// The set of paths a scan must not touch: system directories, user-listed
// exclusions from the config file, and the scanner's own project tree. Answers
// two questions for the walker — should this entry be skipped, and is the scan
// root itself inside an excluded path.
class ExcludeManager {
public:
    explicit ExcludeManager(
        std::filesystem::path file_path = "config/exclude.txt");

    // Loads the built-in system excludes plus the user's config file.
    bool load();

    // Excludes the scanner's own project tree (the directory holding .git,
    // falling back to the working directory) so a full-system scan never scans
    // or quarantines its own config, runtime data, or binaries.
    void excludeProjectRoot();

    // Per-node decision for the DFS: true for a symbolic link (never followed —
    // it may point outside the tree or form a cycle), an unreadable path, or an
    // explicitly excluded path.
    bool shouldSkip(const std::filesystem::path& path) const;

    // One-time ancestry check for the scan root before the DFS starts: true if
    // the root is itself an excluded path or lives inside one.
    bool isRootInsideExcludedPath(const std::filesystem::path& root) const;

private:
    void addExcludedPath(const std::filesystem::path& path, bool internal);
    void clearInternalExcludedPaths();

    std::filesystem::path file_path_;
    std::unordered_set<std::string> excluded_;
    std::unordered_set<std::string> internal_;
};

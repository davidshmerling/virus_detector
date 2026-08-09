#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

// Exclude set for DFS:
// - contains()            — O(1) exact match at every DFS node
// - isScanRootExcluded()  — one-time ancestry check for the scan root only
class ExcludeManager {
public:
    explicit ExcludeManager(
        std::filesystem::path file_path = "config/exclude.txt");

    bool load();

    // Excludes the scanner's own project tree (the directory holding .git,
    // falling back to the working directory) so a full-system scan never scans
    // or quarantines its own config, runtime data, or binaries.
    void excludeProjectRoot();

    void clearInternalExcludedPaths();
    void addInternalExcludedPath(const std::filesystem::path& path);

    // Exact lookup. Use during DFS — excluded dirs are never entered.
    bool contains(const std::filesystem::path& path) const;

    // Per-node skip decision for the DFS: true for a symbolic link (never
    // followed — it may point outside the tree or form a cycle), an unreadable
    // path, or an explicitly excluded path. Combines the symlink test with
    // contains() so the walker does not need to know either rule.
    bool shouldSkip(const std::filesystem::path& path) const;

    // Prefix/ancestry check. Call once on the scan root before DFS starts.
    // Not for per-node use during the walk.
    bool isScanRootExcluded(const std::filesystem::path& root) const;

private:
    static std::filesystem::path normalize(const std::filesystem::path& path);
    static std::string trim(const std::string& text);
    static bool isUnderPrefix(
        std::string_view path,
        std::string_view prefix);

    void addExact(const std::filesystem::path& path, bool internal);

    std::filesystem::path file_path_;
    std::unordered_set<std::string> exact_;
    std::unordered_set<std::string> internal_;
};

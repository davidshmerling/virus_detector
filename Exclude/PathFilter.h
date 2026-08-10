#pragma once

#include <filesystem>

class ExcludeSet;

// Answers the per-node question the DFS asks for every entry it meets: should
// this path be skipped? Returns true for a symbolic link (never followed — it
// may leave the tree or form a cycle), an unreadable path, or a path in the
// exclude set.
class PathFilter {
public:
    explicit PathFilter(const ExcludeSet& exclude_set);

    // Returns true if `path` should be skipped by the DFS.
    bool shouldSkip(const std::filesystem::path& path) const;

private:
    // Returns true if `path` is in the exclude set (exact match after
    // normalization).
    bool isExcluded(const std::filesystem::path& path) const;

    const ExcludeSet& exclude_set_;
};

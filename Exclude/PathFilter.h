#pragma once

#include <filesystem>

class ExcludeSet;

// The per-node decision the DFS asks for every entry it meets: should this be
// skipped? True for a symbolic link (never followed — it may leave the tree or
// form a cycle), an unreadable path, or a path in the exclude set.
class PathFilter {
public:
    explicit PathFilter(const ExcludeSet& exclude_set);

    bool shouldSkip(const std::filesystem::path& path) const;

private:
    // True if path is in the exclude set (exact match after normalization).
    bool isExcluded(const std::filesystem::path& path) const;

    const ExcludeSet& exclude_set_;
};

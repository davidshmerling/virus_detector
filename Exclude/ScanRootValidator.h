#pragma once

#include <filesystem>

class ExcludeSet;

// Performs a one-time check before the DFS starts: whether the requested scan
// root is itself an excluded path, or lives inside one. Prevents a full walk
// from ever beginning inside an excluded tree.
class ScanRootValidator {
public:
    explicit ScanRootValidator(const ExcludeSet& exclude_set);

    // Returns true if `root` is excluded or is a descendant of an excluded path.
    bool isRootInsideExcludedPath(const std::filesystem::path& root) const;

private:
    const ExcludeSet& exclude_set_;
};

#pragma once

#include <filesystem>

class ExcludeSet;

// The one-time check done before the DFS starts: is the requested scan root
// itself an excluded path, or does it live inside one? Keeps a full walk from
// ever beginning inside an excluded tree.
class ScanRootGuard {
public:
    explicit ScanRootGuard(const ExcludeSet& exclude_set);

    bool isRootInsideExcludedPath(const std::filesystem::path& root) const;

private:
    const ExcludeSet& exclude_set_;
};

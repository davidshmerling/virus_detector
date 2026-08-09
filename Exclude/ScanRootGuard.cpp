#include "Exclude/ScanRootGuard.h"

#include "Exclude/ExcludeSet.h"

#include <string>
#include <string_view>

namespace fs = std::filesystem;

namespace {

// True when path is prefix itself or a descendant of it (prefix/...).
bool isSameOrDescendant(std::string_view path, std::string_view prefix)
{
    if (path == prefix) {
        return true;
    }
    return path.size() > prefix.size() &&
           path.starts_with(prefix) &&
           path[prefix.size()] == '/';
}

}  // namespace

ScanRootGuard::ScanRootGuard(const ExcludeSet& exclude_set)
    : exclude_set_(exclude_set)
{
}

bool ScanRootGuard::isRootInsideExcludedPath(const fs::path& root) const
{
    const std::string normalized_root = ExcludeSet::normalize(root);

    for (const std::string& excluded : exclude_set_.excludedPaths()) {
        if (isSameOrDescendant(normalized_root, excluded)) {
            return true;
        }
    }

    return false;
}

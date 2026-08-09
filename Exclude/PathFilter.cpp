#include "Exclude/PathFilter.h"

#include "Exclude/ExcludeSet.h"

#include <system_error>

namespace fs = std::filesystem;

PathFilter::PathFilter(const ExcludeSet& exclude_set)
    : exclude_set_(exclude_set)
{
}

bool PathFilter::shouldSkip(const fs::path& path) const
{
    // Read the file status once, then skip if the path is unreadable, a
    // symbolic link, or explicitly excluded.
    std::error_code error;
    const fs::file_status status = fs::symlink_status(path, error);

    return error ||
           status.type() == fs::file_type::symlink ||
           isExcluded(path);
}

bool PathFilter::isExcluded(const fs::path& path) const
{
    // Exact match only — parents already passed DFS exclusion.
    return exclude_set_.excludedPaths().contains(ExcludeSet::normalize(path));
}

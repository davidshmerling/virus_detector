#include "Scanner/FileEnumerator/SortedDirectoryReader.h"

#include <algorithm>
#include <format>
#include <system_error>

namespace fs = std::filesystem;

SortedDirectoryReader::SortedDirectoryReader(Logger& logger)
    : logger_(logger)
{
}

bool SortedDirectoryReader::read(
    const fs::path& directory,
    std::vector<fs::directory_entry>& children) const
{
    children.clear();

    std::error_code error;
    fs::directory_iterator iterator(
        directory,
        fs::directory_options::skip_permission_denied,
        error);

    if (error) {
        logger_.warning(
            std::format(
                "Open directory failed for {}: {}",
                directory.string(),
                error.message()));
        return false;
    }

    const fs::directory_iterator end;

    while (iterator != end) {
        const fs::path entry_path = iterator->path();
        children.push_back(*iterator);

        iterator.increment(error);
        if (error) {
            logger_.warning(
                std::format(
                    "Read directory entry failed for {}: {}",
                    entry_path.string(),
                    error.message()));
            error.clear();
        }
    }

    std::ranges::sort(
        children,
        [](const fs::directory_entry& left,
           const fs::directory_entry& right) {
            return left.path().filename().generic_string()
                 < right.path().filename().generic_string();
        });

    return true;
}

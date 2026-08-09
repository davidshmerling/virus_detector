#include "Scan/Traversal/FileInfo.h"

#include <cstdint>
#include <system_error>

namespace fs = std::filesystem;

FileInfoBuilder::FileInfoBuilder(Logger& logger)
    : logger_(logger)
{
}

bool FileInfoBuilder::build(
    const fs::path& root,
    const fs::directory_entry& entry,
    FileInfo& info) const
{
    std::error_code error;

    // Reuses the stat the directory_entry already cached during the DFS, so
    // this issues no new filesystem calls in the common case.
    const std::uintmax_t size = entry.file_size(error);
    if (error) {
        logger_.warning("Could not read file size: " + entry.path().string());
        return false;
    }

    const fs::file_time_type write_time = entry.last_write_time(error);
    if (error) {
        logger_.warning("Could not read file time: " + entry.path().string());
        return false;
    }

    info.path = entry.path();
    info.relative_path = entry.path().lexically_relative(root);
    info.size = size;
    info.last_modified =
        static_cast<std::int64_t>(write_time.time_since_epoch().count());
    return true;
}

#pragma once

#include <cstdint>
#include <filesystem>

// Everything a worker needs about a discovered file, collected once by the
// FileTreeWalker while it is already at the file. Passing this to the worker
// avoids a second round of filesystem calls (file_size, last_write_time,
// relative) before the cache lookup.
struct FileInfo {
    std::filesystem::path path;
    std::filesystem::path relative_path;
    std::uintmax_t size = 0;
    std::int64_t last_modified = 0;
};

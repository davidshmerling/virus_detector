#pragma once

#include "Logger/Logger.h"

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

// Builds a FileInfo from a directory_entry the DFS has already visited, reusing
// the stat the entry cached (size + last-modified time) and computing the path
// relative to the scan root. This keeps the "what do we know about this file"
// concern out of the walker, which only has to walk.
class FileInfoBuilder {
public:
    explicit FileInfoBuilder(Logger& logger);

    // Fills info from entry. Returns false if the metadata cannot be read
    // (e.g. the file vanished mid-scan), so the caller can skip the file.
    bool build(const std::filesystem::path& root,
               const std::filesystem::directory_entry& entry,
               FileInfo& info) const;

private:
    Logger& logger_;
};

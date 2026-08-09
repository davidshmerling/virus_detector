#pragma once

#include "Logger/Logger.h"
#include "Scan/FileInfo.h"

#include <cstdint>
#include <filesystem>
#include <system_error>

// Builds a FileInfo from a directory_entry the DFS has already visited, reusing
// the stat the entry cached (size + last-modified time) and computing the path
// relative to the scan root. This keeps the "what do we know about this file"
// concern out of the walker, which only has to walk.
//
// Small enough to be header-only: the single build() method lives here inline.
class FileInfoBuilder {
public:
    explicit FileInfoBuilder(Logger& logger)
        : logger_(logger)
    {
    }

    // Fills info from entry. Returns false if the metadata cannot be read
    // (e.g. the file vanished mid-scan), so the caller can skip the file.
    bool build(const std::filesystem::path& root,
               const std::filesystem::directory_entry& entry,
               FileInfo& info) const
    {
        std::error_code error;

        // Reuses the stat the directory_entry already cached during the DFS,
        // so this issues no new filesystem calls in the common case.
        const std::uintmax_t size = entry.file_size(error);
        if (error) {
            logger_.warning(
                "Could not read file size: " + entry.path().string());
            return false;
        }

        const std::filesystem::file_time_type write_time =
            entry.last_write_time(error);
        if (error) {
            logger_.warning(
                "Could not read file time: " + entry.path().string());
            return false;
        }

        info.path = entry.path();
        info.relative_path = entry.path().lexically_relative(root);
        info.size = size;
        info.last_modified =
            static_cast<std::int64_t>(write_time.time_since_epoch().count());
        return true;
    }

private:
    Logger& logger_;
};

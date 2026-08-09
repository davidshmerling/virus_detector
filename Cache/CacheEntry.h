#pragma once

#include "Common/FileVerdict.h"

#include <cstdint>
#include <string>

// Current file identity used for cache lookup.
struct FileMetadata {
    std::int64_t last_modified = 0;
    std::uintmax_t size = 0;
    std::int64_t signatures_last_modified = 0;
};

inline bool operator==(const FileMetadata& left, const FileMetadata& right)
{
    return left.last_modified == right.last_modified &&
           left.size == right.size &&
           left.signatures_last_modified == right.signatures_last_modified;
}

// One persisted cache record.
struct CacheEntry {
    std::string path;
    FileMetadata metadata;
    FileVerdict verdict = FileVerdict::Clean;

    // The scan generation that last saw this file. Every scan runs under one
    // monotonically increasing generation; an entry left behind at an older
    // generation means the file was not seen by the last completed scan and can
    // be dropped. Not part of file identity, so it is excluded from ==.
    std::uint64_t generation = 0;
};

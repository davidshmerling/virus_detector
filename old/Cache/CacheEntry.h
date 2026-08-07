#pragma once

#include "Common/FileVerdict.h"

#include <cstdint>

struct CacheEntry {
    std::int64_t file_last_modified = 0;
    std::uintmax_t file_size = 0;
    std::int64_t signatures_last_modified = 0;
    FileVerdict verdict = FileVerdict::Clean;
};

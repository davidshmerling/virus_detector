#pragma once

#include "Cache/CacheEntry.h"

#include <string>
#include <unordered_map>
#include <unordered_set>

using CacheMap = std::unordered_map<std::string, CacheEntry>;

class CacheRepository {
public:
    virtual ~CacheRepository() = default;

    virtual bool initialize() = 0;
    virtual bool load(CacheMap& entries) const = 0;

    // snapshot  — full in-memory cache (used by JSON replace).
    // dirty     — entries changed since last flush (used by SQLite UPSERT).
    // removed   — paths deleted since last flush (used by SQLite DELETE).
    virtual bool save(
        const CacheMap& snapshot,
        const CacheMap& dirty_entries,
        const std::unordered_set<std::string>& removed_paths) const = 0;
};

#pragma once

#include "Cache/CacheEntry.h"
#include "Cache/CacheWriter.h"
#include "Cache/SqliteCacheManager.h"
#include "Common/FileVerdict.h"
#include "Logger/Logger.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

// In-memory cache facade for path → verdict lookups. Persistence goes through
// CacheWriter; stale entries are pruned after a completed full-system scan via
// generation stamps.
class CacheManager {
public:
    explicit CacheManager(
        Logger& logger,
        std::filesystem::path database_path = "runtime/cache/cache.db");

    // Opens storage, loads entries into memory, and sets the current scan
    // generation. Returns false if the database cannot be opened.
    bool load();

    // Returns the cached verdict when a valid entry exists (same file
    // metadata and same signatures). std::nullopt means a cache miss.
    std::optional<FileVerdict> cachedVerdict(
        const std::string& path,
        FileMetadata metadata) const;

    // Records a fresh verdict. The entry is stamped with the current scan
    // generation, marking the file as seen by this scan.
    void update(CacheEntry entry);

    // After a successful scan: drains leftover writes; if this was a
    // scan-all, drops every entry not stamped with the current generation;
    // then persists the generation. Partial path scans never prune (other
    // trees must keep their cache entries).
    void commitGeneration(bool full_system_scan);

private:
    // Reads the last successfully completed generation from disk (0 if missing).
    std::uint64_t loadLastCompletedGeneration() const;

    // Persists `generation` as the last successfully completed generation.
    bool saveLastCompletedGeneration(std::uint64_t generation) const;

    Logger& logger_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, CacheEntry> cache_entries_;

    // Generation stamped onto every entry seen during the current scan,
    // always last_completed + 1. Set once by load() before scanning starts.
    std::uint64_t current_generation_ = 0;

    // One-line file next to the SQLite DB holding the last successfully
    // completed generation (for example, runtime/cache/generation.txt).
    std::filesystem::path generation_file_;

    SqliteCacheManager storage_;
    CacheWriter writer_;
};

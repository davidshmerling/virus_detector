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

// In-memory cache facade. Persistence goes through CacheWriter.
class CacheManager {
public:
    explicit CacheManager(
        Logger& logger,
        std::filesystem::path database_path = "runtime/cache/cache.db");

    bool load();

    // Returns the cached verdict when a valid entry exists (same file
    // metadata and same signatures). std::nullopt means a cache miss.
    std::optional<FileVerdict> cachedVerdict(
        const std::string& path,
        FileMetadata metadata) const;

    // Records a fresh verdict. The entry is stamped with the current scan
    // generation, marking the file as seen by this scan.
    void update(CacheEntry entry);

    // After a successful scan: drain leftover writes, and if this was a
    // scan-all drop every entry not stamped with the current generation, then
    // persist the generation. Partial path scans never prune (other trees must
    // keep their cache entries).
    void commitGeneration(bool full_system_scan);

private:
    std::uint64_t loadLastCompletedGeneration() const;
    bool saveLastCompletedGeneration(std::uint64_t generation) const;

    Logger& logger_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, CacheEntry> cache_entries_;

    // The generation stamped onto every entry seen during the current scan,
    // always last_completed + 1. Set once by load() before any scanning starts.
    std::uint64_t current_generation_ = 0;

    // One-line file next to the SQLite DB: the last successfully completed
    // generation (e.g. runtime/cache/generation.txt).
    std::filesystem::path generation_file_;

    SqliteCacheManager storage_;
    CacheWriter writer_;
};

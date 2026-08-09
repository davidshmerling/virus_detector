#pragma once

#include "Cache/CacheCleaner.h"
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
    void remove(const std::string& path);
    void flush();

    // Promotes the in-progress generation to "last completed". Call only after a
    // scan finishes successfully; anything older than this becomes reclaimable on
    // the next run. Must run after flush() so every stamped entry is durable
    // before the generation is declared complete.
    void commitGeneration();

private:
    Logger& logger_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, CacheEntry> cache_entries_;

    // The generation stamped onto every entry seen during the current scan,
    // always last_completed + 1. Set once by load() before any scanning starts.
    std::uint64_t current_generation_ = 0;

    SqliteCacheManager storage_;
    CacheCleaner cleaner_;
    CacheWriter writer_;
};

#pragma once

#include "Cache/CacheEntry.h"
#include "Cache/CacheWriter.h"
#include "Cache/SqliteCacheManager.h"
#include "Common/FileVerdict.h"
#include "Logger/Logger.h"

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

    void update(CacheEntry entry);
    void remove(const std::string& path);
    void flush();

private:
    Logger& logger_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, CacheEntry> entries_;
    SqliteCacheManager storage_;
    CacheWriter writer_;
};

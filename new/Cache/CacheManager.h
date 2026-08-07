#pragma once

#include "Cache/CacheEntry.h"
#include "Cache/CacheWriter.h"
#include "Cache/SqliteCache.h"
#include "Logger/Logger.h"

#include <filesystem>
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

    bool containsValid(
        const std::string& path,
        FileMetadata metadata) const;

    void update(CacheEntry entry);
    void remove(const std::string& path);
    void flush();

private:
    Logger& logger_;
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, CacheEntry> entries_;
    SqliteCache storage_;
    CacheWriter writer_;
};

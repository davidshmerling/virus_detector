#pragma once

#include "Cache/CacheEntry.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

struct sqlite3;

// SQLite persistence only — no threads, no validity logic.
class SqliteCacheManager {
public:
    explicit SqliteCacheManager(std::filesystem::path database_path);
    ~SqliteCacheManager();

    SqliteCacheManager(const SqliteCacheManager&) = delete;
    SqliteCacheManager& operator=(const SqliteCacheManager&) = delete;

    bool open();

    // Loads every row at generation >= min_generation straight into a path-keyed
    // map (reserved up front from the row count so it never rehashes during the
    // load).
    std::unordered_map<std::string, CacheEntry> loadAll(
        std::uint64_t min_generation);

    bool upsertBatch(const std::vector<CacheEntry>& entries);
    bool removeBatch(const std::vector<std::string>& paths);

    // Generation bookkeeping. The last completed generation persists across runs
    // so the next scan knows which entries are stale.
    std::uint64_t loadLastCompletedGeneration();
    bool saveLastCompletedGeneration(std::uint64_t generation);

    // Drops every entry left behind at a generation older than the given one.
    bool deleteOlderThan(std::uint64_t generation);

    // Wipes all entries and generation state (used on the theoretical counter
    // overflow, to restart cleanly from generation 0).
    bool clearAll();

private:
    bool exec(const char* sql) const;

    std::filesystem::path database_path_;
    sqlite3* database_ = nullptr;
};

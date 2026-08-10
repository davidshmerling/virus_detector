#pragma once

#include "Cache/CacheEntry.h"

#include <SQLiteCpp/Database.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// SQLite persistence only — no threads, no validity logic.
class SqliteCacheManager {
public:
    explicit SqliteCacheManager(std::filesystem::path database_path);
    ~SqliteCacheManager();

    SqliteCacheManager(const SqliteCacheManager&) = delete;
    SqliteCacheManager& operator=(const SqliteCacheManager&) = delete;

    // Opens (or creates) the database and ensures the schema exists.
    bool open();

    // Loads every row into a path-keyed map (reserved up front from the row
    // count so it never rehashes during the load).
    std::unordered_map<std::string, CacheEntry> loadAll();

    // Upserts a batch of entries in one transaction.
    bool upsertBatch(const std::vector<CacheEntry>& entries);

    // After a successful scan-all: drops every entry not stamped with the just
    // finished generation (files that scan never saw).
    bool cleanOldGenerations(std::uint64_t current_generation);

    // Wipes all cache entries (used on theoretical counter overflow, so the
    // next scan can restart cleanly from generation 0).
    bool clearAll();

private:
    std::filesystem::path database_path_;
    std::unique_ptr<SQLite::Database> database_;
};

#pragma once

#include "Cache/CacheEntry.h"

#include <filesystem>
#include <string>
#include <vector>

struct sqlite3;

// SQLite persistence only — no threads, no validity logic.
class SqliteCache {
public:
    explicit SqliteCache(std::filesystem::path database_path);
    ~SqliteCache();

    SqliteCache(const SqliteCache&) = delete;
    SqliteCache& operator=(const SqliteCache&) = delete;

    bool open();
    std::vector<CacheEntry> loadAll();
    bool upsertBatch(const std::vector<CacheEntry>& entries);
    bool removeBatch(const std::vector<std::string>& paths);

private:
    bool exec(const char* sql) const;

    std::filesystem::path database_path_;
    sqlite3* database_ = nullptr;
};

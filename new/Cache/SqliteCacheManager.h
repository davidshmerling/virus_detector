#pragma once

#include "Cache/CacheEntry.h"

#include <filesystem>
#include <string>
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
    std::vector<CacheEntry> loadAll();
    bool upsertBatch(const std::vector<CacheEntry>& entries);
    bool removeBatch(const std::vector<std::string>& paths);

private:
    bool exec(const char* sql) const;

    std::filesystem::path database_path_;
    sqlite3* database_ = nullptr;
};

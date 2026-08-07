#pragma once

#include "Cache/CacheRepository.h"

#include <filesystem>
#include <mutex>

struct sqlite3;
struct sqlite3_stmt;

// Persists the clean-file cache in SQLite.
// Lookup stays in CacheManager's in-memory map; this class only loads/saves.
class SqliteCacheRepository : public CacheRepository {
public:
    explicit SqliteCacheRepository(std::filesystem::path database_path);
    ~SqliteCacheRepository() override;

    SqliteCacheRepository(const SqliteCacheRepository&) = delete;
    SqliteCacheRepository& operator=(const SqliteCacheRepository&) = delete;

    bool initialize() override;
    bool load(CacheMap& entries) const override;
    bool save(const CacheMap& dirty_entries) const override;

private:
    bool exec(const char* sql) const;
    bool prepareUpsertStatement(sqlite3_stmt*& statement) const;
    bool upsertEntry(
        sqlite3_stmt* statement,
        const std::string& path,
        const CacheEntry& entry) const;

    static int verdictToInt(FileVerdict verdict);
    static FileVerdict verdictFromInt(int value);

    std::filesystem::path database_path_;
    mutable std::mutex mutex_;
    mutable sqlite3* database_ = nullptr;
};

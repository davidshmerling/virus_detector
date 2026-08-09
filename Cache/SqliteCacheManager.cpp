#include "Cache/SqliteCacheManager.h"

#include <system_error>

#include <sqlite3.h>

namespace fs = std::filesystem;

namespace {

constexpr const char* kCreateTableSql = R"(
CREATE TABLE IF NOT EXISTS cache_entries (
    path TEXT PRIMARY KEY,
    file_last_modified INTEGER NOT NULL,
    file_size INTEGER NOT NULL,
    signatures_last_modified INTEGER NOT NULL,
    verdict INTEGER NOT NULL,
    generation INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS cache_meta (
    key TEXT PRIMARY KEY,
    value INTEGER NOT NULL
);
)";

constexpr const char* kCountAllSql = R"(
SELECT COUNT(*) FROM cache_entries;
)";

constexpr const char* kSelectAllSql = R"(
SELECT path, file_last_modified, file_size, signatures_last_modified, verdict,
       generation
FROM cache_entries
WHERE generation >= ?;
)";

constexpr const char* kUpsertSql = R"(
INSERT INTO cache_entries (
    path, file_last_modified, file_size, signatures_last_modified, verdict,
    generation
) VALUES (?, ?, ?, ?, ?, ?)
ON CONFLICT(path) DO UPDATE SET
    file_last_modified = excluded.file_last_modified,
    file_size = excluded.file_size,
    signatures_last_modified = excluded.signatures_last_modified,
    verdict = excluded.verdict,
    generation = excluded.generation;
)";

constexpr const char* kDeleteSql = R"(
DELETE FROM cache_entries WHERE path = ?;
)";

constexpr const char* kDeleteOlderSql = R"(
DELETE FROM cache_entries WHERE generation < ?;
)";

constexpr const char* kSelectGenerationSql = R"(
SELECT value FROM cache_meta WHERE key = 'last_completed_generation';
)";

constexpr const char* kUpsertGenerationSql = R"(
INSERT INTO cache_meta (key, value)
VALUES ('last_completed_generation', ?)
ON CONFLICT(key) DO UPDATE SET value = excluded.value;
)";

constexpr const char* kClearAllSql = R"(
DELETE FROM cache_entries;
DELETE FROM cache_meta;
)";

// Number of rows in cache_entries, so the map can be reserved before loading.
std::size_t countRows(sqlite3* database)
{
    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database, kCountAllSql, -1, &statement, nullptr) !=
        SQLITE_OK) {
        return 0;
    }

    std::size_t count = 0;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        count = static_cast<std::size_t>(sqlite3_column_int64(statement, 0));
    }

    sqlite3_finalize(statement);
    return count;
}

}  // namespace

SqliteCacheManager::SqliteCacheManager(fs::path database_path)
    : database_path_(std::move(database_path))
{
}

SqliteCacheManager::~SqliteCacheManager()
{
    if (database_ != nullptr) {
        sqlite3_close(database_);
        database_ = nullptr;
    }
}

bool SqliteCacheManager::exec(const char* sql) const
{
    char* error_message = nullptr;
    const int result =
        sqlite3_exec(database_, sql, nullptr, nullptr, &error_message);
    if (result != SQLITE_OK) {
        sqlite3_free(error_message);
        return false;
    }
    return true;
}

bool SqliteCacheManager::open()
{
    std::error_code error;
    const fs::path parent = database_path_.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, error);
        if (error) {
            return false;
        }
    }

    if (database_ != nullptr) {
        sqlite3_close(database_);
        database_ = nullptr;
    }

    if (sqlite3_open(database_path_.string().c_str(), &database_) != SQLITE_OK) {
        if (database_ != nullptr) {
            sqlite3_close(database_);
            database_ = nullptr;
        }
        return false;
    }

    return exec("PRAGMA journal_mode=WAL;") &&
           exec("PRAGMA synchronous=NORMAL;") &&
           exec(kCreateTableSql);
}

std::unordered_map<std::string, CacheEntry> SqliteCacheManager::loadAll(
    std::uint64_t min_generation)
{
    std::unordered_map<std::string, CacheEntry> cache;
    if (database_ == nullptr) {
        return cache;
    }

    // Reserve from the row count so inserting every row never rehashes. Stale
    // rows are deleted before this call, so the count matches what we load.
    cache.reserve(countRows(database_));

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, kSelectAllSql, -1, &statement, nullptr) !=
        SQLITE_OK) {
        return cache;
    }

    sqlite3_bind_int64(
        statement, 1, static_cast<sqlite3_int64>(min_generation));

    while (sqlite3_step(statement) == SQLITE_ROW) {
        const unsigned char* path_text = sqlite3_column_text(statement, 0);
        if (path_text == nullptr) {
            continue;
        }

        CacheEntry entry;
        entry.path = reinterpret_cast<const char*>(path_text);
        entry.metadata.last_modified = sqlite3_column_int64(statement, 1);
        entry.metadata.size =
            static_cast<std::uintmax_t>(sqlite3_column_int64(statement, 2));
        entry.metadata.signatures_last_modified =
            sqlite3_column_int64(statement, 3);
        entry.verdict = static_cast<FileVerdict>(sqlite3_column_int(statement, 4));
        entry.generation =
            static_cast<std::uint64_t>(sqlite3_column_int64(statement, 5));

        std::string path = entry.path;
        cache.emplace(std::move(path), std::move(entry));
    }

    sqlite3_finalize(statement);
    return cache;
}

std::uint64_t SqliteCacheManager::loadLastCompletedGeneration()
{
    if (database_ == nullptr) {
        return 0;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            database_, kSelectGenerationSql, -1, &statement, nullptr) !=
        SQLITE_OK) {
        return 0;
    }

    std::uint64_t generation = 0;
    if (sqlite3_step(statement) == SQLITE_ROW) {
        generation =
            static_cast<std::uint64_t>(sqlite3_column_int64(statement, 0));
    }

    sqlite3_finalize(statement);
    return generation;
}

bool SqliteCacheManager::saveLastCompletedGeneration(std::uint64_t generation)
{
    if (database_ == nullptr) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(
            database_, kUpsertGenerationSql, -1, &statement, nullptr) !=
        SQLITE_OK) {
        return false;
    }

    const bool ok =
        sqlite3_bind_int64(
            statement, 1, static_cast<sqlite3_int64>(generation)) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_DONE;

    sqlite3_finalize(statement);
    return ok;
}

bool SqliteCacheManager::deleteOlderThan(std::uint64_t generation)
{
    if (database_ == nullptr) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, kDeleteOlderSql, -1, &statement, nullptr) !=
        SQLITE_OK) {
        return false;
    }

    const bool ok =
        sqlite3_bind_int64(
            statement, 1, static_cast<sqlite3_int64>(generation)) == SQLITE_OK &&
        sqlite3_step(statement) == SQLITE_DONE;

    sqlite3_finalize(statement);
    return ok;
}

bool SqliteCacheManager::clearAll()
{
    if (database_ == nullptr) {
        return false;
    }
    return exec(kClearAllSql);
}

bool SqliteCacheManager::upsertBatch(const std::vector<CacheEntry>& entries)
{
    if (database_ == nullptr || entries.empty()) {
        return database_ != nullptr;
    }

    if (!exec("BEGIN IMMEDIATE;")) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, kUpsertSql, -1, &statement, nullptr) !=
        SQLITE_OK) {
        exec("ROLLBACK;");
        return false;
    }

    bool ok = true;
    for (const CacheEntry& entry : entries) {
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);

        if (sqlite3_bind_text(statement, 1, entry.path.c_str(), -1, SQLITE_TRANSIENT) !=
                SQLITE_OK ||
            sqlite3_bind_int64(statement, 2, entry.metadata.last_modified) !=
                SQLITE_OK ||
            sqlite3_bind_int64(
                statement, 3, static_cast<sqlite3_int64>(entry.metadata.size)) !=
                SQLITE_OK ||
            sqlite3_bind_int64(
                statement, 4, entry.metadata.signatures_last_modified) !=
                SQLITE_OK ||
            sqlite3_bind_int(statement, 5, static_cast<int>(entry.verdict)) !=
                SQLITE_OK ||
            sqlite3_bind_int64(
                statement, 6, static_cast<sqlite3_int64>(entry.generation)) !=
                SQLITE_OK ||
            sqlite3_step(statement) != SQLITE_DONE) {
            ok = false;
            break;
        }
    }

    sqlite3_finalize(statement);
    if (!ok) {
        exec("ROLLBACK;");
        return false;
    }
    return exec("COMMIT;");
}

bool SqliteCacheManager::removeBatch(const std::vector<std::string>& paths)
{
    if (database_ == nullptr || paths.empty()) {
        return database_ != nullptr;
    }

    if (!exec("BEGIN IMMEDIATE;")) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, kDeleteSql, -1, &statement, nullptr) !=
        SQLITE_OK) {
        exec("ROLLBACK;");
        return false;
    }

    bool ok = true;
    for (const std::string& path : paths) {
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);

        if (sqlite3_bind_text(statement, 1, path.c_str(), -1, SQLITE_TRANSIENT) !=
                SQLITE_OK ||
            sqlite3_step(statement) != SQLITE_DONE) {
            ok = false;
            break;
        }
    }

    sqlite3_finalize(statement);
    if (!ok) {
        exec("ROLLBACK;");
        return false;
    }
    return exec("COMMIT;");
}

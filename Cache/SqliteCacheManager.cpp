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
    verdict INTEGER NOT NULL
);
)";

constexpr const char* kSelectAllSql = R"(
SELECT path, file_last_modified, file_size, signatures_last_modified, verdict
FROM cache_entries;
)";

constexpr const char* kUpsertSql = R"(
INSERT INTO cache_entries (
    path, file_last_modified, file_size, signatures_last_modified, verdict
) VALUES (?, ?, ?, ?, ?)
ON CONFLICT(path) DO UPDATE SET
    file_last_modified = excluded.file_last_modified,
    file_size = excluded.file_size,
    signatures_last_modified = excluded.signatures_last_modified,
    verdict = excluded.verdict;
)";

constexpr const char* kDeleteSql = R"(
DELETE FROM cache_entries WHERE path = ?;
)";

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

std::vector<CacheEntry> SqliteCacheManager::loadAll()
{
    std::vector<CacheEntry> entries;
    if (database_ == nullptr) {
        return entries;
    }

    sqlite3_stmt* statement = nullptr;
    if (sqlite3_prepare_v2(database_, kSelectAllSql, -1, &statement, nullptr) !=
        SQLITE_OK) {
        return entries;
    }

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
        entries.push_back(std::move(entry));
    }

    sqlite3_finalize(statement);
    return entries;
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

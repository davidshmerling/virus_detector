#include "Cache/SqliteCacheRepository.h"

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
SELECT path,
       file_last_modified,
       file_size,
       signatures_last_modified,
       verdict
FROM cache_entries;
)";

constexpr const char* kUpsertSql = R"(
INSERT INTO cache_entries (
    path,
    file_last_modified,
    file_size,
    signatures_last_modified,
    verdict
)
VALUES (?, ?, ?, ?, ?)
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

SqliteCacheRepository::SqliteCacheRepository(fs::path database_path)
    : database_path_(std::move(database_path))
{
}

SqliteCacheRepository::~SqliteCacheRepository()
{
    std::scoped_lock lock(mutex_);

    if (database_ != nullptr) {
        sqlite3_close(database_);
        database_ = nullptr;
    }
}

bool SqliteCacheRepository::exec(const char* sql) const
{
    char* error_message = nullptr;

    const int result = sqlite3_exec(
        database_,
        sql,
        nullptr,
        nullptr,
        &error_message);

    if (result != SQLITE_OK) {
        sqlite3_free(error_message);
        return false;
    }

    return true;
}

int SqliteCacheRepository::verdictToInt(FileVerdict verdict)
{
    return static_cast<int>(verdict);
}

FileVerdict SqliteCacheRepository::verdictFromInt(int value)
{
    switch (value) {
        case static_cast<int>(FileVerdict::Clean):
            return FileVerdict::Clean;

        case static_cast<int>(FileVerdict::Malicious):
            return FileVerdict::Malicious;

        case static_cast<int>(FileVerdict::Error):
            return FileVerdict::Error;

        default:
            return FileVerdict::Error;
    }
}

bool SqliteCacheRepository::initialize()
{
    std::scoped_lock lock(mutex_);

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

    const int open_result = sqlite3_open(
        database_path_.string().c_str(),
        &database_);

    if (open_result != SQLITE_OK) {
        if (database_ != nullptr) {
            sqlite3_close(database_);
            database_ = nullptr;
        }
        return false;
    }

    if (!exec("PRAGMA journal_mode=WAL;")) {
        return false;
    }

    if (!exec("PRAGMA synchronous=NORMAL;")) {
        return false;
    }

    return exec(kCreateTableSql);
}

bool SqliteCacheRepository::load(CacheMap& entries) const
{
    std::scoped_lock lock(mutex_);
    entries.clear();

    if (database_ == nullptr) {
        return false;
    }

    sqlite3_stmt* statement = nullptr;

    if (sqlite3_prepare_v2(
            database_,
            kSelectAllSql,
            -1,
            &statement,
            nullptr) != SQLITE_OK) {
        return false;
    }

    bool ok = true;

    while (true) {
        const int step = sqlite3_step(statement);

        if (step == SQLITE_DONE) {
            break;
        }

        if (step != SQLITE_ROW) {
            ok = false;
            break;
        }

        const unsigned char* path_text = sqlite3_column_text(statement, 0);
        if (path_text == nullptr) {
            continue;
        }

        CacheEntry entry;
        entry.file_last_modified = sqlite3_column_int64(statement, 1);
        entry.file_size =
            static_cast<std::uintmax_t>(sqlite3_column_int64(statement, 2));
        entry.signatures_last_modified = sqlite3_column_int64(statement, 3);
        entry.verdict = verdictFromInt(sqlite3_column_int(statement, 4));

        entries.emplace(
            reinterpret_cast<const char*>(path_text),
            entry);
    }

    sqlite3_finalize(statement);

    if (!ok) {
        entries.clear();
    }

    return ok;
}

bool SqliteCacheRepository::prepareUpsertStatement(
    sqlite3_stmt*& statement) const
{
    return sqlite3_prepare_v2(
               database_,
               kUpsertSql,
               -1,
               &statement,
               nullptr) == SQLITE_OK;
}

bool SqliteCacheRepository::prepareDeleteStatement(
    sqlite3_stmt*& statement) const
{
    return sqlite3_prepare_v2(
               database_,
               kDeleteSql,
               -1,
               &statement,
               nullptr) == SQLITE_OK;
}

bool SqliteCacheRepository::upsertEntry(
    sqlite3_stmt* statement,
    const std::string& path,
    const CacheEntry& entry) const
{
    sqlite3_reset(statement);
    sqlite3_clear_bindings(statement);

    if (sqlite3_bind_text(
            statement,
            1,
            path.c_str(),
            -1,
            SQLITE_TRANSIENT) != SQLITE_OK) {
        return false;
    }

    if (sqlite3_bind_int64(
            statement,
            2,
            entry.file_last_modified) != SQLITE_OK) {
        return false;
    }

    if (sqlite3_bind_int64(
            statement,
            3,
            static_cast<sqlite3_int64>(entry.file_size)) != SQLITE_OK) {
        return false;
    }

    if (sqlite3_bind_int64(
            statement,
            4,
            entry.signatures_last_modified) != SQLITE_OK) {
        return false;
    }

    if (sqlite3_bind_int(
            statement,
            5,
            verdictToInt(entry.verdict)) != SQLITE_OK) {
        return false;
    }

    return sqlite3_step(statement) == SQLITE_DONE;
}

bool SqliteCacheRepository::deleteEntry(
    sqlite3_stmt* statement,
    const std::string& path) const
{
    sqlite3_reset(statement);
    sqlite3_clear_bindings(statement);

    if (sqlite3_bind_text(
            statement,
            1,
            path.c_str(),
            -1,
            SQLITE_TRANSIENT) != SQLITE_OK) {
        return false;
    }

    return sqlite3_step(statement) == SQLITE_DONE;
}

bool SqliteCacheRepository::save(
    const CacheMap& /*snapshot*/,
    const CacheMap& dirty_entries,
    const std::unordered_set<std::string>& removed_paths) const
{
    std::scoped_lock lock(mutex_);

    if (database_ == nullptr) {
        return false;
    }

    if (dirty_entries.empty() && removed_paths.empty()) {
        return true;
    }

    // One transaction for the whole flush — only dirty rows.
    if (!exec("BEGIN IMMEDIATE;")) {
        return false;
    }

    sqlite3_stmt* upsert_statement = nullptr;
    sqlite3_stmt* delete_statement = nullptr;

    if (!dirty_entries.empty() && !prepareUpsertStatement(upsert_statement)) {
        exec("ROLLBACK;");
        return false;
    }

    if (!removed_paths.empty() && !prepareDeleteStatement(delete_statement)) {
        if (upsert_statement != nullptr) {
            sqlite3_finalize(upsert_statement);
        }
        exec("ROLLBACK;");
        return false;
    }

    bool ok = true;

    for (const auto& [path, entry] : dirty_entries) {
        if (!upsertEntry(upsert_statement, path, entry)) {
            ok = false;
            break;
        }
    }

    if (ok) {
        for (const std::string& path : removed_paths) {
            if (!deleteEntry(delete_statement, path)) {
                ok = false;
                break;
            }
        }
    }

    if (upsert_statement != nullptr) {
        sqlite3_finalize(upsert_statement);
    }

    if (delete_statement != nullptr) {
        sqlite3_finalize(delete_statement);
    }

    if (!ok) {
        exec("ROLLBACK;");
        return false;
    }

    return exec("COMMIT;");
}

#include "Cache/SqliteCacheManager.h"

#include <SQLiteCpp/Statement.h>
#include <SQLiteCpp/Transaction.h>

#include <system_error>

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

}  // namespace

SqliteCacheManager::SqliteCacheManager(fs::path database_path)
    : database_path_(std::move(database_path))
{
}

SqliteCacheManager::~SqliteCacheManager() = default;

bool SqliteCacheManager::open()
{
    try {
        std::error_code error;
        const fs::path parent = database_path_.parent_path();
        if (!parent.empty()) {
            fs::create_directories(parent, error);
            if (error) {
                return false;
            }
        }

        database_ = std::make_unique<SQLite::Database>(
            database_path_.string(),
            SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE);

        database_->exec("PRAGMA journal_mode=WAL;");
        database_->exec("PRAGMA synchronous=NORMAL;");
        database_->exec(kCreateTableSql);
        return true;
    } catch (const SQLite::Exception&) {
        database_.reset();
        return false;
    }
}

std::unordered_map<std::string, CacheEntry> SqliteCacheManager::loadAll()
{
    std::unordered_map<std::string, CacheEntry> cache;
    if (!database_) {
        return cache;
    }

    try {
        cache.reserve(static_cast<std::size_t>(
            database_->execAndGet("SELECT COUNT(*) FROM cache_entries")
                .getInt64()));

        SQLite::Statement query(
            *database_,
            "SELECT path, file_last_modified, file_size, "
            "signatures_last_modified, verdict, generation "
            "FROM cache_entries");

        while (query.executeStep()) {
            CacheEntry entry;
            entry.path = query.getColumn(0).getString();
            entry.metadata.last_modified = query.getColumn(1).getInt64();
            entry.metadata.size =
                static_cast<std::uintmax_t>(query.getColumn(2).getInt64());
            entry.metadata.signatures_last_modified =
                query.getColumn(3).getInt64();
            entry.verdict =
                static_cast<FileVerdict>(query.getColumn(4).getInt());
            entry.generation =
                static_cast<std::uint64_t>(query.getColumn(5).getInt64());

            std::string path = entry.path;
            cache.emplace(std::move(path), std::move(entry));
        }
    } catch (const SQLite::Exception&) {
        return {};
    }

    return cache;
}

bool SqliteCacheManager::cleanOldGenerations(std::uint64_t current_generation)
{
    if (!database_) {
        return false;
    }

    try {
        SQLite::Statement query(
            *database_,
            "DELETE FROM cache_entries WHERE generation < ?");
        query.bind(1, static_cast<std::int64_t>(current_generation));
        query.exec();
        return true;
    } catch (const SQLite::Exception&) {
        return false;
    }
}

bool SqliteCacheManager::clearAll()
{
    if (!database_) {
        return false;
    }

    try {
        database_->exec("DELETE FROM cache_entries;");
        return true;
    } catch (const SQLite::Exception&) {
        return false;
    }
}

bool SqliteCacheManager::upsertBatch(const std::vector<CacheEntry>& entries)
{
    if (!database_) {
        return false;
    }
    if (entries.empty()) {
        return true;
    }

    try {
        SQLite::Transaction transaction(
            *database_, SQLite::TransactionBehavior::IMMEDIATE);
        SQLite::Statement query(*database_, kUpsertSql);

        for (const CacheEntry& entry : entries) {
            query.reset();
            query.bind(1, entry.path);
            query.bind(2, entry.metadata.last_modified);
            query.bind(3, static_cast<std::int64_t>(entry.metadata.size));
            query.bind(4, entry.metadata.signatures_last_modified);
            query.bind(5, static_cast<int>(entry.verdict));
            query.bind(6, static_cast<std::int64_t>(entry.generation));
            query.exec();
        }

        transaction.commit();
        return true;
    } catch (const SQLite::Exception&) {
        return false;
    }
}

#pragma once

#include <cstdint>

class SqliteCacheManager;

// The "what can we throw away?" side of the cache. Every entry is tagged with
// the generation of the scan that last saw it; an entry left at a generation
// older than the last completed scan means its file was not seen and is stale.
// This prunes such entries from the store before the next scan loads.
class CacheCleaner {
public:
    explicit CacheCleaner(SqliteCacheManager& storage);

    // Prepares the store for the next scan and returns the generation its
    // entries should be loaded from. Normally drops everything older than
    // last_completed_generation and returns it unchanged. On the (theoretical)
    // counter overflow it wipes the store and restarts from generation 0.
    std::uint64_t pruneStale(std::uint64_t last_completed_generation);

private:
    SqliteCacheManager& storage_;
};

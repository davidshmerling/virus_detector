#include "Cache/CacheCleaner.h"

#include "Cache/SqliteCacheManager.h"

#include <limits>

CacheCleaner::CacheCleaner(SqliteCacheManager& storage)
    : storage_(storage)
{
}

std::uint64_t CacheCleaner::pruneStale(std::uint64_t last_completed_generation)
{
    // Counter exhausted: wipe and restart cleanly rather than wrap around.
    if (last_completed_generation ==
        std::numeric_limits<std::uint64_t>::max()) {
        storage_.clearAll();
        return 0;
    }

    // Files not seen by the last completed scan are left at an older generation.
    storage_.deleteOlderThan(last_completed_generation);
    return last_completed_generation;
}

#include "Cache/CacheManager.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

CacheManager::CacheManager(Logger& logger, std::filesystem::path database_path)
    : logger_(logger),
      storage_(std::move(database_path)),
      cleaner_(storage_),
      writer_(storage_)
{
}

bool CacheManager::load()
{
    if (!storage_.open()) {
        logger_.error("Could not open cache database");
        return false;
    }

    const std::uint64_t last_completed = storage_.loadLastCompletedGeneration();

    // Drop stale entries first (and handle the overflow reset); the returned
    // generation is what remains to load from.
    const std::uint64_t load_from = cleaner_.pruneStale(last_completed);

    // Keep the last completed generation and any partial in-progress one (a scan
    // that crashed mid-run), so a resumed scan still benefits from its cache.
    std::unordered_map<std::string, CacheEntry> loaded =
        storage_.loadAll(load_from);

    std::unique_lock lock(mutex_);
    cache_entries_ = std::move(loaded);
    current_generation_ = load_from + 1;

    logger_.info(
        "Cache loaded. Entries: " + std::to_string(cache_entries_.size()) +
        ", generation: " + std::to_string(current_generation_));
    return true;
}

std::optional<FileVerdict> CacheManager::cachedVerdict(
    const std::string& path,
    FileMetadata metadata) const
{
    std::shared_lock lock(mutex_);

    const auto iterator = cache_entries_.find(path);
    if (iterator == cache_entries_.end()) {
        return std::nullopt;
    }

    const CacheEntry& entry = iterator->second;
    if (entry.metadata == metadata) {
        return entry.verdict;
    }
    return std::nullopt;
}

void CacheManager::update(CacheEntry entry)
{
    // Stamp every write with the current generation so this file counts as seen
    // by the current scan (both fresh scans and re-affirmed cache hits).
    entry.generation = current_generation_;
    {
        std::unique_lock lock(mutex_);
        cache_entries_[entry.path] = entry;
    }
    writer_.submit(std::move(entry));
}

void CacheManager::remove(const std::string& path)
{
    {
        std::unique_lock lock(mutex_);
        cache_entries_.erase(path);
    }
    writer_.remove(path);
}

void CacheManager::flush()
{
    writer_.flush();
}

void CacheManager::commitGeneration()
{
    // Every entry from this scan is durable (flush() ran first); declaring the
    // generation complete lets the next run treat older entries as stale.
    storage_.saveLastCompletedGeneration(current_generation_);
}

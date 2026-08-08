#include "Cache/CacheManager.h"

#include <string>
#include <utility>
#include <vector>

CacheManager::CacheManager(Logger& logger, std::filesystem::path database_path)
    : logger_(logger),
      storage_(std::move(database_path)),
      writer_(storage_)
{
}

bool CacheManager::load()
{
    if (!storage_.open()) {
        logger_.error("Could not open cache database");
        return false;
    }

    std::vector<CacheEntry> loaded = storage_.loadAll();

    std::unique_lock lock(mutex_);
    entries_.clear();
    entries_.reserve(loaded.size());
    for (CacheEntry& entry : loaded) {
        std::string path = entry.path;
        entries_.emplace(std::move(path), std::move(entry));
    }

    logger_.info(
        "Cache loaded. Entries: " + std::to_string(entries_.size()));
    return true;
}

std::optional<FileVerdict> CacheManager::cachedVerdict(
    const std::string& path,
    FileMetadata metadata) const
{
    std::shared_lock lock(mutex_);

    const auto iterator = entries_.find(path);
    if (iterator == entries_.end()) {
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
    {
        std::unique_lock lock(mutex_);
        entries_[entry.path] = entry;
    }
    writer_.submit(std::move(entry));
}

void CacheManager::remove(const std::string& path)
{
    {
        std::unique_lock lock(mutex_);
        entries_.erase(path);
    }
    writer_.remove(path);
}

void CacheManager::flush()
{
    writer_.flush();
}

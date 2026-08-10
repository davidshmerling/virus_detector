#include "Cache/CacheManager.h"

#include <cstdint>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace fs = std::filesystem;

CacheManager::CacheManager(Logger& logger, fs::path database_path)
    : logger_(logger),
      generation_file_(database_path.parent_path() / "generation.txt"),
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

    std::uint64_t last_completed = loadLastCompletedGeneration();

    // Theoretical overflow guard: wipe and restart from 0 rather than wrap.
    if (last_completed == std::numeric_limits<std::uint64_t>::max()) {
        storage_.clearAll();
        last_completed = 0;
        saveLastCompletedGeneration(0);
    }

    std::unordered_map<std::string, CacheEntry> loaded = storage_.loadAll();

    std::unique_lock lock(mutex_);
    cache_entries_ = std::move(loaded);
    current_generation_ = last_completed + 1;

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

void CacheManager::commitGeneration(bool full_system_scan)
{
    // Drain leftover upserts so every stamp is durable.
    writer_.finish();

    // Only a completed scan-all may prune: anything still at an older
    // generation was not seen anywhere on the machine.
    if (full_system_scan) {
        storage_.cleanOldGenerations(current_generation_);
    }

    // Record this generation as the last completed.
    saveLastCompletedGeneration(current_generation_);
}

std::uint64_t CacheManager::loadLastCompletedGeneration() const
{
    std::ifstream file(generation_file_);
    std::uint64_t generation = 0;
    if (!(file >> generation)) {
        return 0;
    }
    return generation;
}

bool CacheManager::saveLastCompletedGeneration(std::uint64_t generation) const
{
    const fs::path directory = generation_file_.parent_path();
    if (!directory.empty()) {
        std::error_code error;
        fs::create_directories(directory, error);
    }

    // Atomic write via temp + rename, same pattern as the resume checkpoint.
    const fs::path temp_file = generation_file_.string() + ".tmp";
    {
        std::ofstream file(temp_file, std::ios::trunc);
        if (!file) {
            logger_.error("Could not save cache generation");
            return false;
        }
        file << generation << '\n';
    }

    std::error_code error;
    fs::rename(temp_file, generation_file_, error);
    if (error) {
        fs::remove(temp_file, error);
        logger_.error("Could not save cache generation");
        return false;
    }
    return true;
}

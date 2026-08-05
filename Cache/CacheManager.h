#pragma once

#include "Cache/CacheRepository.h"
#include "Common/FileVerdict.h"
#include "Logger/Logger.h"
#include "Performance/PerformanceProfiler.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_set>

class CacheManager {
public:
    CacheManager(
        std::unique_ptr<CacheRepository> repository,
        Logger& logger,
        PerformanceProfiler& profiler,
        std::size_t flush_interval = 100);

    bool initialize();

    std::optional<FileVerdict> getValidVerdict(
        const std::filesystem::path& file_path,
        std::int64_t signatures_last_modified) const;

    bool update(
        const std::filesystem::path& file_path,
        std::int64_t signatures_last_modified,
        FileVerdict verdict);

    bool remove(const std::filesystem::path& file_path);
    bool flush();

    std::size_t size() const;

private:
    static std::string pathKey(const std::filesystem::path& path);

    static bool getFileIdentity(
        const std::filesystem::path& path,
        std::int64_t& last_modified,
        std::uintmax_t& file_size);

    std::size_t pendingChangesLocked() const;

    std::unique_ptr<CacheRepository> repository_;
    Logger& logger_;
    PerformanceProfiler& profiler_;

    mutable std::shared_mutex mutex_;
    CacheMap entries_;
    std::unordered_set<std::string> dirty_paths_;
    std::unordered_set<std::string> removed_paths_;
    std::size_t flush_interval_;
};

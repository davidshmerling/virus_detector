#pragma once

#include "Cache/CacheRepository.h"
#include "Common/FileVerdict.h"
#include "Logger/Logger.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>

class CacheManager {
public:
    CacheManager(
        std::unique_ptr<CacheRepository> repository,
        Logger& logger,
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

    static bool getLastModified(
        const std::filesystem::path& path,
        std::int64_t& result);

    bool flushUnlocked();

    std::unique_ptr<CacheRepository> repository_;
    Logger& logger_;

    mutable std::mutex mutex_;
    CacheMap entries_;
    std::size_t flush_interval_;
    std::size_t dirty_count_ = 0;
};

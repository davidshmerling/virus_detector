#pragma once

#include "Logger/Logger.h"

#include <cstddef>
#include <filesystem>
#include <mutex>
#include <set>
#include <string>

// Tiny text checkpoint: root / status / next (one field per line).
class ResumeManager {
public:
    explicit ResumeManager(
        Logger& logger,
        std::filesystem::path checkpoint_file =
            "runtime/resume/checkpoint.txt");

    // Matching running checkpoint → resumed=true.
    // Otherwise starts fresh → resumed=false. False only on I/O failure.
    bool begin(const std::filesystem::path& root, bool& resumed);

    const std::filesystem::path& root() const;
    const std::filesystem::path& next() const;

    bool registerTask(const std::filesystem::path& path);
    bool markCompleted(const std::filesystem::path& path);
    bool markEnumerationFinished();
    bool flush();

private:
    static std::string pathKey(const std::filesystem::path& path);
    static std::filesystem::path normalizeRoot(const std::filesystem::path& root);

    bool updateFrontierLocked();
    bool saveLocked();
    bool load(std::filesystem::path& root,
              std::string& status,
              std::filesystem::path& next) const;

    Logger& logger_;
    std::filesystem::path checkpoint_file_;

    std::mutex mutex_;
    std::set<std::string> unfinished_;

    std::filesystem::path root_;
    std::filesystem::path next_;
    std::string status_ = "running";
    bool enumeration_finished_ = false;

    std::size_t dirty_ = 0;
    std::size_t flush_interval_ = 100;
};

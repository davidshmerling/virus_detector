#pragma once

#include "Logger/Logger.h"
#include "Quarantine/QuarantineEntry.h"
#include "Quarantine/QuarantineRepository.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

// Moves infected files to quarantine and supports restore / delete.
class QuarantineManager {
public:
    QuarantineManager(
        std::filesystem::path quarantine_directory,
        Logger& logger);

    bool initialize();

    bool quarantine(
        const std::filesystem::path& file_path,
        const std::string& signature);

    bool restore(const std::string& id);
    bool remove(const std::string& id);

    std::vector<QuarantineEntry> list() const;

private:
    std::string generateId() const;
    std::string currentTime() const;

    bool moveFile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination);

    std::filesystem::path quarantine_directory_;
    std::filesystem::path files_directory_;

    Logger& logger_;
    QuarantineRepository repository_;
    mutable std::mutex mutex_;
};

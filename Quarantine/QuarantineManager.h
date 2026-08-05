#pragma once

#include "Logger/Logger.h"
#include "Quarantine/FileMover.h"
#include "Quarantine/QuarantineEntry.h"
#include "Quarantine/QuarantineRepository.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

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
    struct UniqueDestination {
        std::string id;
        std::filesystem::path path;
    };

    std::string generateId() const;
    std::string currentTime() const;

    UniqueDestination createUniqueDestination(
        const std::filesystem::path& original_path) const;

    std::filesystem::path quarantine_directory_;
    std::filesystem::path files_directory_;

    Logger& logger_;
    QuarantineRepository repository_;
    FileMover file_mover_;
    mutable std::mutex mutex_;
};

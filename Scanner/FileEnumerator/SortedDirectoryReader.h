#pragma once

#include "Logger/Logger.h"

#include <filesystem>
#include <vector>

class SortedDirectoryReader {
public:
    explicit SortedDirectoryReader(Logger& logger);

    bool read(
        const std::filesystem::path& directory,
        std::vector<std::filesystem::directory_entry>& children) const;

private:
    Logger& logger_;
};

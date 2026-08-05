#pragma once

#include "Logger/Logger.h"
#include "Performance/PerformanceProfiler.h"

#include <filesystem>
#include <vector>

class SortedDirectoryReader {
public:
    SortedDirectoryReader(
        Logger& logger,
        PerformanceProfiler& profiler);

    bool read(
        const std::filesystem::path& directory,
        std::vector<std::filesystem::directory_entry>& children) const;

private:
    Logger& logger_;
    PerformanceProfiler& profiler_;
};

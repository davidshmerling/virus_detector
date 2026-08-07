#pragma once

#include "CLI/CommandParser.h"
#include "Logger/Logger.h"
#include "Performance/PerformanceProfiler.h"
#include "Quarantine/QuarantineManager.h"
#include "Scanner/Scanner.h"

#include <filesystem>
#include <memory>
#include <string>

class Application {
public:
    int run(int argc, char* argv[]);

private:
    int executeCommand(const Command& command);

    int runScan(const std::filesystem::path& root);
    int restoreFile(const std::string& id);
    int deleteFile(const std::string& id);
    int listQuarantine();

    std::unique_ptr<Logger> logger_;
    std::unique_ptr<PerformanceProfiler> profiler_;
    std::unique_ptr<QuarantineManager> quarantine_manager_;
    std::unique_ptr<Scanner> scanner_;
};

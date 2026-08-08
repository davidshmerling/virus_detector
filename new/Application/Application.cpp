#include "Application/Application.h"

#include "Console/ConsolePrinter.h"
#include "Logger/Logger.h"
#include "Performance/PerformanceMonitor.h"
#include "Quarantine/QuarantineManager.h"
#include "Scan/ScanPipeline.h"

#include <filesystem>

int Application::run(int argc, char* argv[])
{
    // Times the whole run: the monitor reports (log + console) when it goes out
    // of scope, after dispatch() has finished.
    Logger logger("runtime/logs");
    PerformanceMonitor monitor(logger, "Total run time");

    const Command command = parser_.parse(argc, argv);
    return dispatch(command);
}

int Application::dispatch(const Command& command)
{
    switch (command.type) {
        case CommandType::Help:
            parser_.printHelp();
            return 0;

        case CommandType::ScanAll:
        case CommandType::ScanPath:
            return runScan(command);

        case CommandType::Restore:
        case CommandType::RestoreAll:
        case CommandType::Delete:
        case CommandType::ListQuarantine:
            return runQuarantine(command);

        case CommandType::Unknown:
        default:
            ConsolePrinter::printError("Unknown or invalid command");
            parser_.printHelp();
            return 1;
    }
}

int Application::runScan(const Command& command)
{
    const std::filesystem::path root =
        command.type == CommandType::ScanAll ? "/" : command.argument;

    ScanPipeline().scan(root);
    return 0;
}

int Application::runQuarantine(const Command& command)
{
    Logger logger("runtime/logs");
    QuarantineManager quarantine(logger, "runtime/quarantine");

    if (!quarantine.load()) {
        ConsolePrinter::printError("Could not open quarantine");
        return 1;
    }

    switch (command.type) {
        case CommandType::Restore:
            return quarantine.restore(command.argument) ? 0 : 1;

        case CommandType::RestoreAll:
            return quarantine.restoreAll() ? 0 : 1;

        case CommandType::Delete:
            return quarantine.remove(command.argument) ? 0 : 1;

        case CommandType::ListQuarantine:
            ConsolePrinter::printQuarantineList(quarantine.list());
            return 0;

        default:
            return 1;
    }
}

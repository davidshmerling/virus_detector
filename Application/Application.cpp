#include "Application/Application.h"

#include "CLI/ConsolePrinter.h"
#include "Scanner/ScanSummary.h"

#include <chrono>
#include <string>

int Application::run(int argc, char* argv[])
{
    using clock = std::chrono::steady_clock;
    const auto start_time = clock::now();

    logger_ = std::make_unique<Logger>("runtime/logs");
    if (!logger_->isOpen()) {
        ConsolePrinter::printError("Could not initialize logger");
        return 1;
    }

    ConsolePrinter::printMessage("Log file: " + logger_->filePath());

    quarantine_manager_ = std::make_unique<QuarantineManager>(
        "runtime/quarantine",
        *logger_);

    scanner_ = std::make_unique<Scanner>(
        "config/signatures.txt",
        "config/exclude.txt",
        *quarantine_manager_,
        *logger_);

    CommandParser command_parser;
    const Command command = command_parser.parse(argc, argv);

    const int exit_code = executeCommand(command);

    const auto end_time = clock::now();
    const double seconds =
        std::chrono::duration<double>(end_time - start_time).count();
    ConsolePrinter::printMessage(
        "Elapsed time: " + std::to_string(seconds) + " seconds");

    return exit_code;
}

int Application::executeCommand(const Command& command)
{
    switch (command.type) {
        case CommandType::Help: {
            CommandParser().printHelp();
            return 0;
        }

        case CommandType::ScanAll:
            return runScan("/");

        case CommandType::ScanPath:
            return runScan(command.argument);

        case CommandType::Restore:
            return restoreFile(command.argument);

        case CommandType::Delete:
            return deleteFile(command.argument);

        case CommandType::ListQuarantine:
            return listQuarantine();

        case CommandType::Unknown:
        default:
            ConsolePrinter::printError("Unknown or invalid command");
            CommandParser().printHelp();
            return 1;
    }
}

int Application::runScan(const std::filesystem::path& root)
{
    if (!scanner_->initialize()) {
        return 1;
    }

    const ScanSummary summary = scanner_->scanRoot(root);
    ConsolePrinter::printScanSummary(summary);
    return summary.failed.load() > 0 ? 1 : 0;
}

int Application::restoreFile(const std::string& id)
{
    if (!quarantine_manager_->initialize()) {
        ConsolePrinter::printError("Could not initialize quarantine");
        return 1;
    }

    if (!quarantine_manager_->restore(id)) {
        ConsolePrinter::printError(
            "Could not restore file: id = " + id);
        return 1;
    }

    ConsolePrinter::printMessage("File restored: id = " + id);
    logger_->info("Restored from quarantine: " + id);
    return 0;
}

int Application::deleteFile(const std::string& id)
{
    if (!quarantine_manager_->initialize()) {
        ConsolePrinter::printError("Could not initialize quarantine");
        return 1;
    }

    if (!quarantine_manager_->remove(id)) {
        ConsolePrinter::printError(
            "Could not delete file: id = " + id);
        return 1;
    }

    ConsolePrinter::printMessage("File deleted: id = " + id);
    logger_->info("Deleted from quarantine: " + id);
    return 0;
}

int Application::listQuarantine()
{
    if (!quarantine_manager_->initialize()) {
        ConsolePrinter::printError("Could not initialize quarantine");
        return 1;
    }

    ConsolePrinter::printQuarantineList(quarantine_manager_->list());
    return 0;
}

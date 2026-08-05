#include "CLI/CommandParser.h"
#include "CLI/ConsolePrinter.h"
#include "Logger/Logger.h"
#include "Quarantine/QuarantineManager.h"
#include "Scanner/Scanner.h"
#include "Scanner/ScanSummary.h"

#include <chrono>
#include <exception>
#include <iostream>
#include <string>

namespace {

int runApplication(int argc, char* argv[])
{
    using clock = std::chrono::steady_clock;
    const auto start_time = clock::now();

    Logger logger("runtime/logs");
    if (!logger.isOpen()) {
        ConsolePrinter::printError("Could not initialize logger");
        return 1;
    }

    ConsolePrinter::printMessage("Log file: " + logger.filePath());

    CommandParser command_parser;
    const Command command = command_parser.parse(argc, argv);

    Scanner scanner(
        "config/signatures.txt",
        "config/exclude.txt",
        "runtime/quarantine",
        logger);

    QuarantineManager quarantine_manager("runtime/quarantine", logger);

    const bool is_scan_command =
        command.type == CommandType::ScanAll ||
        command.type == CommandType::ScanPath;

    const bool is_quarantine_command =
        command.type == CommandType::Restore ||
        command.type == CommandType::Delete ||
        command.type == CommandType::ListQuarantine;

    if (is_scan_command && !scanner.initialize()) {
        return 1;
    }

    if (is_quarantine_command && !quarantine_manager.initialize()) {
        ConsolePrinter::printError("Could not initialize quarantine");
        return 1;
    }

    int exit_code = 0;

    switch (command.type) {
        case CommandType::Help:
            command_parser.printHelp();
            break;

        case CommandType::ScanAll: {
            const ScanSummary summary = scanner.scanRoot("/");
            ConsolePrinter::printScanSummary(summary);
            exit_code = summary.failed.load() > 0 ? 1 : 0;
            break;
        }

        case CommandType::ScanPath: {
            const ScanSummary summary =
                scanner.scanRoot(command.argument);
            ConsolePrinter::printScanSummary(summary);
            exit_code = summary.failed.load() > 0 ? 1 : 0;
            break;
        }

        case CommandType::Restore:
            if (quarantine_manager.restore(command.argument)) {
                ConsolePrinter::printMessage(
                    "File restored: id = " + command.argument);
                logger.info("Restored from quarantine: " + command.argument);
            } else {
                ConsolePrinter::printError(
                    "Could not restore file: id = " + command.argument);
                exit_code = 1;
            }
            break;

        case CommandType::Delete:
            if (quarantine_manager.remove(command.argument)) {
                ConsolePrinter::printMessage(
                    "File deleted: id = " + command.argument);
                logger.info("Deleted from quarantine: " + command.argument);
            } else {
                ConsolePrinter::printError(
                    "Could not delete file: id = " + command.argument);
                exit_code = 1;
            }
            break;

        case CommandType::ListQuarantine:
            ConsolePrinter::printQuarantineList(
                quarantine_manager.list());
            break;

        case CommandType::Unknown:
        default:
            ConsolePrinter::printError(
                "Unknown or invalid command");

            command_parser.printHelp();
            exit_code = 1;
            break;
    }

    const auto end_time = clock::now();
    const double seconds =
        std::chrono::duration<double>(end_time - start_time).count();
    ConsolePrinter::printMessage(
        "Elapsed time: " + std::to_string(seconds) + " seconds");

    return exit_code;
}

}  // namespace

int main(int argc, char* argv[])
{
    try {
        return runApplication(argc, argv);
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Fatal unknown error\n";
        return 1;
    }
}

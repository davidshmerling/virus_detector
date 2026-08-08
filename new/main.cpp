#include "CLI/CommandParser.h"
#include "Console/ConsolePrinter.h"
#include "CrashHandler/SegfaultHandler.h"
#include "Logger/Logger.h"
#include "Quarantine/QuarantineManager.h"
#include "Scan/ScanManager/ScanManager.h"

#include <exception>
#include <iostream>

namespace {

int handleQuarantineCommand(const Command& command)
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

int dispatch(const Command& command)
{
    CommandParser parser;

    switch (command.type) {
        case CommandType::Help:
            parser.printHelp();
            return 0;

        case CommandType::ScanAll:
            ScanManager().scan("/");
            return 0;

        case CommandType::ScanPath:
            ScanManager().scan(command.argument);
            return 0;

        case CommandType::Restore:
        case CommandType::RestoreAll:
        case CommandType::Delete:
        case CommandType::ListQuarantine:
            return handleQuarantineCommand(command);

        case CommandType::Unknown:
        default:
            ConsolePrinter::printError("Unknown or invalid command");
            parser.printHelp();
            return 1;
    }
}

}  // namespace

int main(int argc, char* argv[])
{
    installSegfaultHandler();

    try {
        const Command command = CommandParser().parse(argc, argv);
        return dispatch(command);
    } catch (const std::exception& exception) {
        std::cerr << "Fatal error: " << exception.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "Fatal unknown error\n";
        return 1;
    }
}




/*

 find new -type f -name "*.cpp" -exec wc -l {} +

*/   
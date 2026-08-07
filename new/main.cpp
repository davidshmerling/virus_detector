#include "CLI/CommandParser.h"
#include "Console/ConsolePrinter.h"
#include "CrashHandler/SegfaultHandler.h"

#include <exception>
#include <iostream>

namespace {

int dispatch(const Command& command)
{
    CommandParser parser;

    switch (command.type) {
        case CommandType::Help:
            parser.printHelp();
            return 0;

        case CommandType::ScanAll:
        case CommandType::ScanPath:
        case CommandType::Restore:
        case CommandType::RestoreAll:
        case CommandType::Delete:
        case CommandType::ListQuarantine:
            ConsolePrinter::printError(
                "Command is recognized but not wired to Application yet");
            return 1;

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

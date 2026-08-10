#include "CLI/CommandParser.h"

#include <iostream>
#include <string_view>

Command CommandParser::parse(int argc, char* argv[]) const
{
    if (argc < 2) {
        return Command{.type = CommandType::Help, .argument = {}};
    }

    const std::string_view action = argv[1];

    if (action == "scan-all") {
        return Command{.type = CommandType::ScanAll, .argument = {}};
    }

    if (action == "scan") {
        if (argc < 3) {
            return Command{.type = CommandType::Unknown, .argument = {}};
        }
        return Command{.type = CommandType::ScanPath, .argument = argv[2]};
    }

    if (action == "restore-all") {
        return Command{.type = CommandType::RestoreAll, .argument = {}};
    }

    if (action == "restore") {
        if (argc < 3) {
            return Command{.type = CommandType::Unknown, .argument = {}};
        }
        return Command{.type = CommandType::Restore, .argument = argv[2]};
    }

    if (action == "delete") {
        if (argc < 3) {
            return Command{.type = CommandType::Unknown, .argument = {}};
        }
        return Command{.type = CommandType::Delete, .argument = argv[2]};
    }

    if (action == "q-list") {
        return Command{.type = CommandType::ListQuarantine, .argument = {}};
    }

    if (action == "help") {
        return Command{.type = CommandType::Help, .argument = {}};
    }

    return Command{.type = CommandType::Unknown, .argument = {}};
}

void CommandParser::printHelp() const
{
    std::cout
        << "Antivirus Scanner\n"
        << "\n"
        << "Usage:\n"
        << "  av scan-all\n"
        << "  av scan <path>\n"
        << "  av restore <id>\n"
        << "  av restore-all\n"
        << "  av delete <id>\n"
        << "  av q-list\n"
        << "  av help\n";
}

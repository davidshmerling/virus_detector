#include "CLI/CommandParser.h"

#include <iostream>
#include <string_view>

Command CommandParser::parse(int argc, char* argv[]) const
{
    Command command;

    if (argc < 2) {
        command.type = CommandType::Help;
        return command;
    }

    const std::string_view action = argv[1];

    if (action == "scan-all") {
        return Command{.type = CommandType::ScanAll, .argument = {}};
    }

    if (action == "scan") {
        if (argc < 3) {
            return Command{.type = CommandType::Unknown, .argument = {}};
        }

        return Command{
            .type = CommandType::ScanPath,
            .argument = argv[2]};
    }

    if (action == "restore") {
        if (argc < 3) {
            return Command{.type = CommandType::Unknown, .argument = {}};
        }

        return Command{
            .type = CommandType::Restore,
            .argument = argv[2]};
    }

    if (action == "delete") {
        if (argc < 3) {
            return Command{.type = CommandType::Unknown, .argument = {}};
        }

        return Command{
            .type = CommandType::Delete,
            .argument = argv[2]};
    }

    if (action == "quarantine-list" || action == "list") {
        return Command{.type = CommandType::ListQuarantine, .argument = {}};
    }

    if (action == "help" || action == "--help" || action == "-h") {
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
        << "  av_scanner scan-all\n"
        << "  av_scanner scan <path>\n"
        << "  av_scanner restore <id>\n"
        << "  av_scanner delete <id>\n"
        << "  av_scanner quarantine-list\n"
        << "  av_scanner help\n";
}

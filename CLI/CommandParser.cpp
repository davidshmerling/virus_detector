#include "CLI/CommandParser.h"

#include <iostream>

Command CommandParser::parse(int argc, char* argv[]) const {
    Command command;

    if (argc < 2) {
        command.type = CommandType::Help;
        return command;
    }

    const std::string action = argv[1];

    if (action == "scan-all") {
        command.type = CommandType::ScanAll;
        return command;
    }

    if (action == "scan") {
        if (argc < 3) {
            command.type = CommandType::Unknown;
            return command;
        }
        command.type = CommandType::ScanPath;
        command.argument = argv[2];
        return command;
    }

    if (action == "restore") {
        if (argc < 3) {
            command.type = CommandType::Unknown;
            return command;
        }
        command.type = CommandType::Restore;
        command.argument = argv[2];
        return command;
    }

    if (action == "delete") {
        if (argc < 3) {
            command.type = CommandType::Unknown;
            return command;
        }
        command.type = CommandType::Delete;
        command.argument = argv[2];
        return command;
    }

    if (action == "quarantine-list" || action == "list") {
        command.type = CommandType::ListQuarantine;
        return command;
    }

    if (action == "help" || action == "--help" || action == "-h") {
        command.type = CommandType::Help;
        return command;
    }

    command.type = CommandType::Unknown;
    return command;
}

void CommandParser::printHelp() const {
    std::cout
        << "Antivirus Scanner\n"
        << "\n"
        << "Usage:\n"
        << "  scanner scan-all\n"
        << "  scanner scan <path>\n"
        << "  scanner restore <id>\n"
        << "  scanner delete <id>\n"
        << "  scanner quarantine-list\n"
        << "  scanner help\n";
}

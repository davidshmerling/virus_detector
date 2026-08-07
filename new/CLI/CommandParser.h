#pragma once

#include <string>

enum class CommandType {
    ScanAll,
    ScanPath,
    Restore,
    RestoreAll,
    Delete,
    ListQuarantine,
    Help,
    Unknown
};

struct Command {
    CommandType type = CommandType::Unknown;
    std::string argument;  // path for ScanPath, or id for Restore/Delete
};

class CommandParser {
public:
    Command parse(int argc, char* argv[]) const;
    void printHelp() const;
};

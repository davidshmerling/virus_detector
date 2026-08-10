#pragma once

#include <string>

// CLI verb selected by CommandParser from argv.
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

// Parsed command-line request. `argument` holds a path for ScanPath, or an id
// for Restore / Delete.
struct Command {
    CommandType type = CommandType::Unknown;
    std::string argument;
};

// Parses argv into a Command and prints usage help.
class CommandParser {
public:
    // Parses `argc`/`argv` into a Command. Unknown or incomplete input yields
    // CommandType::Unknown.
    Command parse(int argc, char* argv[]) const;

    // Prints the usage text to stdout.
    void printHelp() const;
};

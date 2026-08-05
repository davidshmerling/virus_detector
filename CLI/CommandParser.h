#pragma once

#include <string>

// What the user asked the program to do.
enum class CommandType {
    ScanAll,          // scan the whole disk
    ScanPath,         // scan one file or folder
    Restore,          // restore a file from quarantine
    Delete,           // delete a file from quarantine
    ListQuarantine,   // show all quarantined files
    Help,             // show usage
    Unknown           // bad / missing command
};

// Simple result of parsing argv.
struct Command {
    CommandType type = CommandType::Unknown;
    std::string argument;  // path for ScanPath, or id for Restore/Delete
};

// Parses the command line into a Command.
class CommandParser {
public:
    Command parse(int argc, char* argv[]) const;
    void printHelp() const;
};

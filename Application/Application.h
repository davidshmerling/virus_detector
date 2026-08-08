#pragma once

#include "CLI/CommandParser.h"
#include "Logger/Logger.h"

// Top-level entry point of the program's logic: it parses the command line and
// routes each command to the right subsystem (scan or quarantine). It performs
// no scanning or file work itself; it only wires the pieces together, so main()
// can stay a thin shell.
class Application {
public:
    int run(int argc, char* argv[]);

private:
    // One Logger is created per run and threaded through every subsystem so a
    // single run writes to a single log file.
    int dispatch(const Command& command, Logger& logger);

    int runScan(const Command& command, Logger& logger);
    int runQuarantine(const Command& command, Logger& logger);

    CommandParser parser_;
};

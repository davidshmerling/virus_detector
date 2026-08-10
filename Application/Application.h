#pragma once

#include "CLI/CommandParser.h"
#include "Logger/Logger.h"

// Top-level entry point of the program's logic: parses the command line and
// routes each command to the right subsystem (scan or quarantine). Performs no
// scanning or file work itself; it only wires the pieces together, so main()
// can stay a thin shell.
class Application {
public:
    // Parses `argc`/`argv`, sets up logging and timing, and dispatches the
    // command. Returns the process exit code.
    int run(int argc, char* argv[]);

private:
    // Routes `command` to the scan or quarantine subsystem. One Logger is
    // created per run and threaded through every subsystem so a single run
    // writes to a single log file.
    int dispatch(const Command& command, Logger& logger);

    // Runs a scan for ScanAll / ScanPath.
    int runScan(const Command& command, Logger& logger);

    // Runs a quarantine command (restore, delete, list).
    int runQuarantine(const Command& command, Logger& logger);

    CommandParser parser_;
};

#pragma once

#include "CLI/CommandParser.h"

// Top-level entry point of the program's logic: it parses the command line and
// routes each command to the right subsystem (scan or quarantine). It performs
// no scanning or file work itself; it only wires the pieces together, so main()
// can stay a thin shell.
class Application {
public:
    int run(int argc, char* argv[]);

private:
    int dispatch(const Command& command);

    int runScan(const Command& command);
    int runQuarantine(const Command& command);

    CommandParser parser_;
};

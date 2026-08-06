#pragma once

#include "Quarantine/QuarantineEntry.h"
#include "Scanner/ScanSummary.h"

#include <string>
#include <vector>

// Prints simple messages to the console (stdout / stderr).
class ConsolePrinter {
public:
    static void printMessage(const std::string& message);
    static void printError(const std::string& message);
    static void printScanStarted(const std::string& path);

    static void printQuarantineList(
        const std::vector<QuarantineEntry>& entries);

    static void printScanSummary(const ScanSummary& summary);
};

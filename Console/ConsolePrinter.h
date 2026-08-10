#pragma once

#include "Scan/Pipeline/ScanSummary.h"
#include "Quarantine/QuarantineEntry.h"

#include <string>
#include <vector>

// Writes user-facing messages to stdout / stderr. No logging side effects.
class ConsolePrinter {
public:
    static void printMessage(const std::string& message);
    static void printError(const std::string& message);
    static void printScanStarted(const std::string& path);

    // Prints the quarantine list in a readable table-like format.
    static void printQuarantineList(
        const std::vector<QuarantineEntry>& entries);

    // Prints the end-of-scan counters from `summary`.
    static void printScanSummary(const ScanSummary& summary);
};

#include "Console/ConsolePrinter.h"

#include <iostream>

void ConsolePrinter::printMessage(const std::string& message)
{
    std::cout << message << '\n';
}

void ConsolePrinter::printError(const std::string& message)
{
    std::cerr << "Error: " << message << '\n';
}

void ConsolePrinter::printScanStarted(const std::string& path)
{
    std::cout << "Scanning: " << path << '\n';
}

void ConsolePrinter::printQuarantineList(
    const std::vector<QuarantineEntry>& entries)
{
    if (entries.empty()) {
        std::cout << "Quarantine is empty.\n";
        return;
    }

    std::cout << "Quarantined files:\n";

    for (const QuarantineEntry& entry : entries) {
        std::cout << "  ID:       " << entry.id << '\n'
                  << "  Original: " << entry.original_path << '\n'
                  << "  Signatures:";

        if (entry.signatures.empty()) {
            std::cout << " (none)\n";
        } else {
            std::cout << '\n';
            for (const std::string& signature : entry.signatures) {
                std::cout << "    - " << signature << '\n';
            }
        }

        std::cout << "  Size:     " << entry.file_size << '\n'
                  << "  Date:     " << entry.quarantined_at << '\n'
                  << '\n';
    }
}

void ConsolePrinter::printScanSummary(const ScanSummary& summary)
{
    std::cout << "Scan summary:\n"
              << "  Scanned:     " << summary.scanned.load() << '\n'
              << "  Cached:      " << summary.cached.load() << '\n'
              << "  Malicious:   " << summary.malicious.load() << '\n'
              << "  Quarantined: " << summary.quarantined.load() << '\n'
              << "  Failed:      " << summary.failed.load() << '\n';
}

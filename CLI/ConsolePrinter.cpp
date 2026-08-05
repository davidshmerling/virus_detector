#include "CLI/ConsolePrinter.h"

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

void ConsolePrinter::printScanResult(
    const FileScanResult& result,
    const SignatureManager& signature_manager)
{
    switch (result.verdict) {
        case FileVerdict::Clean:
            std::cout << "Clean: " << result.file_path << '\n';
            break;

        case FileVerdict::Malicious: {
            std::cout << "Malicious: " << result.file_path << '\n';

            const auto& signatures =
                signature_manager.getSignatures();

            if (result.matched_signature_index < signatures.size()) {
                std::cout << "Matched signature: "
                          << signatures[result.matched_signature_index]
                          << '\n';
            }

            break;
        }

        case FileVerdict::Error:
            std::cerr << "Could not scan: "
                      << result.file_path;

            if (result.error.hasError()) {
                std::cerr << " - " << result.error.message;
            }

            std::cerr << '\n';
            break;
    }
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
                  << "  Signature:" << entry.signature << '\n'
                  << "  Size:     " << entry.file_size << '\n'
                  << "  Date:     " << entry.quarantined_at << '\n'
                  << '\n';
    }
}

void ConsolePrinter::printScanSummary(const ScanSummary& summary)
{
    std::cout << "Scan summary:\n"
              << "  Discovered:  " << summary.discovered.load() << '\n'
              << "  Scanned:     " << summary.scanned.load() << '\n'
              << "  Cached:      " << summary.cached.load() << '\n'
              << "  Excluded:    " << summary.excluded.load() << '\n'
              << "  Malicious:   " << summary.malicious.load() << '\n'
              << "  Quarantined: " << summary.quarantined.load() << '\n'
              << "  Failed:      " << summary.failed.load() << '\n';
}

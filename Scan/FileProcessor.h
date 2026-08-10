#pragma once

#include "Scan/Automaton/AutomatonScanner.h"

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

// Scans a single file: opens it, reads it in 1 MB chunks, and runs each chunk
// through the automaton (AutomatonScanner), carrying automaton state across
// the whole file so a signature that crosses a chunk boundary is still
// detected. This is plain logic, not a thread — any worker can run it.
//
// The read buffer is thread-local (see process()), so a single FileProcessor
// instance is safe to share across all worker threads; the scanner (and its
// Automaton) are shared read-only too.
class FileProcessor {
public:
    static constexpr std::size_t kChunkSize = 1024 * 1024;  // 1 MB

    // Constructs a processor that uses `scanner` and maps match indexes back
    // through `signatures`.
    FileProcessor(
        const AutomatonScanner& scanner,
        const std::vector<std::string>& signatures);

    // Scans one file. nullopt = could not open (Error); empty vector = Clean;
    // non-empty = matched signature strings (Malicious), unique and sorted.
    std::optional<std::vector<std::string>> process(
        const std::filesystem::path& path) const;

private:
    // Translates matched automaton indexes into the signature strings that
    // triggered them (sorted), for recording in a quarantine entry.
    std::vector<std::string> matchedSignatures(
        const std::unordered_set<std::size_t>& matches) const;

    const AutomatonScanner& scanner_;
    const std::vector<std::string>& signatures_;
};

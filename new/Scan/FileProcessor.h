#pragma once

#include "Scan/Automaton/AutomatonScanner.h"

#include <cstddef>
#include <filesystem>
#include <unordered_set>

// The pipeline for a single file: opens it, reads it in 1 MB chunks, and runs
// each chunk through the automaton (AutomatonScanner), carrying the automaton
// state across the whole file so a signature that crosses a chunk boundary is
// still detected. Plain logic, not a thread — any worker can run it.
//
// The read buffer is thread-local (see process()), so a single FileProcessor
// instance is safe to share across all worker threads; the scanner (and its
// Automaton) are shared read-only too.
class FileProcessor {
public:
    static constexpr std::size_t kChunkSize = 1024 * 1024;  // 1 MB

    explicit FileProcessor(const AutomatonScanner& scanner);

    // Scan one file. Returns the set of matched signature indexes (unique,
    // unordered). Empty means clean, or the file could not be opened.
    std::unordered_set<std::size_t> process(
        const std::filesystem::path& path) const;

private:
    const AutomatonScanner& scanner_;
};

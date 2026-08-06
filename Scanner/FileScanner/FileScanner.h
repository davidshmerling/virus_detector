#pragma once

#include "Common/Error.h"
#include "Common/FileVerdict.h"
#include "Performance/PerformanceProfiler.h"
#include "Scanner/Automaton/AhoCorasick.h"

#include <cstddef>
#include <filesystem>
#include <vector>

struct FileScanResult {
    FileVerdict verdict = FileVerdict::Error;
    std::filesystem::path file_path;
    // Unique signature indexes that matched anywhere in the file.
    std::vector<std::size_t> matched_signature_indices;
    Error error{};
};

// Scans one file with the shared Aho-Corasick automaton.
// Returns Error on local failures; does not log or print.
class FileScanner {
public:
    FileScanner(
        const AhoCorasick& automaton,
        PerformanceProfiler& profiler,
        std::size_t chunk_size = 256 * 1024);

    [[nodiscard]] FileScanResult scan(
        const std::filesystem::path& file_path);

private:
    const AhoCorasick& automaton_;
    PerformanceProfiler& profiler_;
    std::size_t chunk_size_;
    // Reused across scan() calls on the same worker (via thread_local scanner).
    std::vector<char> buffer_;
};

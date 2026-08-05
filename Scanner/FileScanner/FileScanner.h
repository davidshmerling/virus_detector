#pragma once

#include "Common/Error.h"
#include "Common/FileVerdict.h"
#include "Scanner/Automaton/AhoCorasick.h"

#include <cstddef>
#include <filesystem>

struct FileScanResult {
    FileVerdict verdict = FileVerdict::Error;
    std::filesystem::path file_path;
    std::size_t matched_signature_index = 0;
    Error error;
};

// Scans one file with the shared Aho-Corasick automaton.
// Returns Error on local failures; does not log or print.
class FileScanner {
public:
    explicit FileScanner(
        const AhoCorasick& automaton,
        std::size_t chunk_size = 4 * 1024 * 1024);

    FileScanResult scan(const std::filesystem::path& file_path) const;

private:
    // Returns true when a signature match is found in the buffer.
    bool scanBuffer(
        const char* data,
        std::size_t size,
        int& state,
        std::size_t& matched_signature_index) const;

    const AhoCorasick& automaton_;
    std::size_t chunk_size_;
};

#include "Scan/FileProcessor.h"

#include <fstream>
#include <span>
#include <vector>

namespace fs = std::filesystem;

FileProcessor::FileProcessor(const AutomatonScanner& scanner)
    : scanner_(scanner)
{
}

std::unordered_set<std::size_t> FileProcessor::process(const fs::path& path) const
{
    // One 1 MB read buffer per worker thread: allocated on this thread's first
    // call and reused for every file it scans afterwards. Because it is
    // thread_local, a single shared FileProcessor can run on many threads at
    // once without them fighting over the same buffer.
    thread_local std::vector<char> buffer(kChunkSize);

    std::unordered_set<std::size_t> matched_indices;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return matched_indices;
    }

    // State lives across all chunks of this one file, so a signature that
    // crosses a chunk boundary is still detected.
    int state = 0;

    while (true) {
        file.read(
            buffer.data(),
            static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytes_read = file.gcount();

        if (bytes_read <= 0) {
            break;
        }

        scanner_.scanChunk(
            std::span<const char>{
                buffer.data(),
                static_cast<std::size_t>(bytes_read)},
            state,
            matched_indices);
    }

    return matched_indices;
}

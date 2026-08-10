#include "Scan/FileProcessor.h"

#include <algorithm>
#include <fstream>
#include <optional>
#include <span>
#include <vector>

namespace fs = std::filesystem;

FileProcessor::FileProcessor(
    const AutomatonScanner& scanner,
    const std::vector<std::string>& signatures)
    : scanner_(scanner),
      signatures_(signatures)
{
}

std::optional<std::vector<std::string>> FileProcessor::process(
    const fs::path& path) const
{
    // One 1 MB read buffer per worker thread: allocated on this thread's first
    // call and reused for every file it scans afterwards. Because it is
    // thread_local, a single shared FileProcessor can run on many threads at
    // once without them contending over the same buffer.
    thread_local std::vector<char> buffer(kChunkSize);

    std::unordered_set<std::size_t> matched_indices;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    // Automaton state lives across all chunks of this one file, so a signature
    // that crosses a chunk boundary is still detected.
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

    return matchedSignatures(matched_indices);
}

std::vector<std::string> FileProcessor::matchedSignatures(
    const std::unordered_set<std::size_t>& matches) const
{
    std::vector<std::string> result;
    result.reserve(matches.size());
    for (const std::size_t index : matches) {
        if (index < signatures_.size()) {
            result.push_back(signatures_[index]);
        }
    }

    std::ranges::sort(result);
    return result;
}

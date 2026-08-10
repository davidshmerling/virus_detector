#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// Loads malware signature strings from a text file (one string per line).
// Blank lines and lines starting with '#' are ignored. Changing the file
// needs no recompile. The loaded order is stable, so a signature's position
// doubles as its automaton index.
class SignatureLoader {
public:
    // Constructs a loader that reads from `file_path` (default:
    // config/signatures.txt).
    explicit SignatureLoader(
        std::filesystem::path file_path = "config/signatures.txt");

    // Reads the signatures file into memory. Returns false if the file cannot
    // be opened or yields no valid signatures.
    bool load();

    const std::vector<std::string>& signatures() const;
    std::size_t count() const;

    // Returns the last-write time of the signatures file in epoch ticks,
    // captured during load(). The cache uses this to invalidate results when
    // signatures change.
    std::int64_t lastModified() const;

private:
    // Strips leading and trailing whitespace from `text`.
    static std::string trim(const std::string& text);

    // Returns true if `line` is a usable signature (non-empty and not a
    // '#' comment).
    static bool isValidSignature(const std::string& line);

    std::filesystem::path file_path_;
    std::vector<std::string> signatures_;
    std::int64_t last_modified_ = 0;
};

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
    explicit SignatureLoader(
        std::filesystem::path file_path = "config/signatures.txt");

    bool load();

    const std::vector<std::string>& signatures() const;
    std::size_t count() const;

    // Last-write time of the signatures file (epoch ticks), captured during
    // load(). Used by the cache to invalidate results when signatures change.
    std::int64_t lastModified() const;

private:
    static std::string trim(const std::string& text);
    static bool isValidSignature(const std::string& line);

    std::filesystem::path file_path_;
    std::vector<std::string> signatures_;
    std::int64_t last_modified_ = 0;
};

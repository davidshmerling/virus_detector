#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct QuarantineEntry {
    std::string id;
    std::filesystem::path original_path;
    std::filesystem::path quarantine_path;
    // Unique matched signature words (each at most once).
    std::vector<std::string> signatures;
    std::uintmax_t file_size = 0;
    std::string quarantined_at;
};

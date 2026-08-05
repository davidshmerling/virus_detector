#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct QuarantineEntry {
    std::string id;
    std::filesystem::path original_path;
    std::filesystem::path quarantine_path;
    std::string signature;
    std::uintmax_t file_size = 0;
    std::string quarantined_at;
};

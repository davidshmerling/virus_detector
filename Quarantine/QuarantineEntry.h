#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// One quarantine record: identity, locations, matched signatures, and the
// permissions to restore later.
struct QuarantineEntry {
    std::string id;
    std::filesystem::path original_path;
    std::filesystem::path quarantine_path;
    std::vector<std::string> signatures;
    std::uintmax_t file_size = 0;
    std::string quarantined_at;

    // Permissions the file had before it was quarantined, so a restore can put
    // it back exactly as it was. `unknown` means "not recorded" (for example,
    // older metadata) and is left untouched on restore.
    std::filesystem::perms original_permissions =
        std::filesystem::perms::unknown;
};

#pragma once

#include "Quarantine/QuarantineEntry.h"

#include <nlohmann/json.hpp>

// ADL hooks so nlohmann can convert QuarantineEntry (and vectors of them)
// automatically via get_to / dump.
inline void to_json(nlohmann::json& json, const QuarantineEntry& entry)
{
    json = {
        {"id", entry.id},
        {"original_path", entry.original_path.string()},
        {"quarantine_path", entry.quarantine_path.string()},
        {"signatures", entry.signatures},
        {"file_size", entry.file_size},
        {"quarantined_at", entry.quarantined_at}};

    // Persists permissions as a decimal mode (for example, 0755 → 493). Omitted
    // when unknown so older entries stay backward compatible.
    if (entry.original_permissions != std::filesystem::perms::unknown) {
        json["permissions"] = static_cast<int>(
            entry.original_permissions & std::filesystem::perms::mask);
    }
}

inline void from_json(const nlohmann::json& json, QuarantineEntry& entry)
{
    json.at("id").get_to(entry.id);

    std::string original_path;
    std::string quarantine_path;
    json.at("original_path").get_to(original_path);
    json.at("quarantine_path").get_to(quarantine_path);
    entry.original_path = original_path;
    entry.quarantine_path = quarantine_path;

    if (json.contains("signatures")) {
        json.at("signatures").get_to(entry.signatures);
    }

    json.at("file_size").get_to(entry.file_size);
    json.at("quarantined_at").get_to(entry.quarantined_at);

    // Older metadata omits permissions; leave original_permissions as unknown
    // so restore does not invent a mode.
    if (json.contains("permissions")) {
        entry.original_permissions =
            static_cast<std::filesystem::perms>(
                json.at("permissions").get<int>()) &
            std::filesystem::perms::mask;
    }
}

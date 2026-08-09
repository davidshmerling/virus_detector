#include "Quarantine/QuarantineRepository.h"

#include <algorithm>
#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace {

nlohmann::json toJson(const QuarantineEntry& entry)
{
    nlohmann::json json = {
        {"id", entry.id},
        {"original_path", entry.original_path.string()},
        {"quarantine_path", entry.quarantine_path.string()},
        {"signatures", entry.signatures},
        {"file_size", entry.file_size},
        {"quarantined_at", entry.quarantined_at}};

    // Persist permissions as a decimal mode (e.g. 0755 -> 493). Omitted when
    // unknown so older entries stay backward compatible.
    if (entry.original_permissions != fs::perms::unknown) {
        json["permissions"] = static_cast<int>(
            entry.original_permissions & fs::perms::mask);
    }

    return json;
}

QuarantineEntry fromJson(const nlohmann::json& json)
{
    QuarantineEntry entry;

    entry.id = json.at("id").get<std::string>();
    entry.original_path = json.at("original_path").get<std::string>();
    entry.quarantine_path = json.at("quarantine_path").get<std::string>();

    if (json.contains("signatures") && json.at("signatures").is_array()) {
        entry.signatures =
            json.at("signatures").get<std::vector<std::string>>();
    }

    entry.file_size = json.at("file_size").get<std::uintmax_t>();
    entry.quarantined_at = json.at("quarantined_at").get<std::string>();

    if (json.contains("permissions") &&
        json.at("permissions").is_number_integer()) {
        entry.original_permissions =
            static_cast<fs::perms>(json.at("permissions").get<int>()) &
            fs::perms::mask;
    }

    return entry;
}

}  // namespace

QuarantineRepository::QuarantineRepository(fs::path metadata_file)
    : metadata_file_(std::move(metadata_file))
{
}

bool QuarantineRepository::load()
{
    entries_.clear();

    std::error_code error;
    if (!fs::exists(metadata_file_, error) || error) {
        return true;  // No metadata yet: an empty quarantine is valid.
    }

    std::ifstream file(metadata_file_);
    if (!file.is_open()) {
        return false;
    }

    try {
        nlohmann::json root;
        file >> root;

        if (root.contains("entries") && root["entries"].is_array()) {
            for (const auto& item : root["entries"]) {
                entries_.push_back(fromJson(item));
            }
        }
    } catch (const std::exception&) {
        entries_.clear();
        return false;
    }

    return true;
}

bool QuarantineRepository::save() const
{
    nlohmann::json root;
    root["entries"] = nlohmann::json::array();
    for (const QuarantineEntry& entry : entries_) {
        root["entries"].push_back(toJson(entry));
    }

    const fs::path temp_file = metadata_file_.string() + ".tmp";

    {
        std::ofstream file(temp_file, std::ios::trunc);
        if (!file.is_open()) {
            return false;
        }
        file << root.dump(2);
    }

    std::error_code error;
    fs::rename(temp_file, metadata_file_, error);
    if (error) {
        fs::remove(temp_file, error);
        return false;
    }

    return true;
}

void QuarantineRepository::add(const QuarantineEntry& entry)
{
    entries_.push_back(entry);
}

bool QuarantineRepository::removeById(const std::string& id)
{
    const auto last = std::remove_if(
        entries_.begin(),
        entries_.end(),
        [&](const QuarantineEntry& entry) { return entry.id == id; });

    if (last == entries_.end()) {
        return false;
    }

    entries_.erase(last, entries_.end());
    return true;
}

std::optional<QuarantineEntry> QuarantineRepository::find(
    const std::string& id) const
{
    for (const QuarantineEntry& entry : entries_) {
        if (entry.id == id) {
            return entry;
        }
    }

    return std::nullopt;
}

std::vector<QuarantineEntry> QuarantineRepository::all() const
{
    return entries_;
}

#include "Quarantine/QuarantineRepository.h"

#include "Quarantine/QuarantineEntryJson.h"

#include <algorithm>
#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

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

        if (root.contains("entries")) {
            entries_ = root.at("entries").get<std::vector<QuarantineEntry>>();
        }
    } catch (const std::exception&) {
        entries_.clear();
        return false;
    }

    return true;
}

bool QuarantineRepository::save() const
{
    const nlohmann::json root = {{"entries", entries_}};

    // Atomic write: temp + rename, same pattern as resume/cache checkpoints.
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

#include "Quarantine/QuarantineRepository.h"

#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace {

nlohmann::json toJson(const QuarantineEntry& entry)
{
    return {
        {"id", entry.id},
        {"original_path", entry.original_path.string()},
        {"quarantine_path", entry.quarantine_path.string()},
        {"signature", entry.signature},
        {"file_size", entry.file_size},
        {"quarantined_at", entry.quarantined_at}
    };
}

QuarantineEntry fromJson(const nlohmann::json& json)
{
    QuarantineEntry entry;

    entry.id = json.at("id").get<std::string>();
    entry.original_path = json.at("original_path").get<std::string>();
    entry.quarantine_path = json.at("quarantine_path").get<std::string>();
    entry.signature = json.at("signature").get<std::string>();
    entry.file_size = json.at("file_size").get<std::uintmax_t>();
    entry.quarantined_at = json.at("quarantined_at").get<std::string>();

    return entry;
}

}  // namespace

QuarantineRepository::QuarantineRepository(fs::path metadata_file)
    : metadata_file_(std::move(metadata_file))
{
}

bool QuarantineRepository::initialize()
{
    std::scoped_lock lock(mutex_);

    std::error_code error;
    fs::create_directories(metadata_file_.parent_path(), error);

    if (error) {
        return false;
    }

    entries_.clear();
    return load(entries_);
}

bool QuarantineRepository::load(std::vector<QuarantineEntry>& entries) const
{
    entries.clear();

    std::error_code error;
    if (!fs::exists(metadata_file_, error)) {
        return !error;
    }

    if (error) {
        return false;
    }

    std::ifstream file(metadata_file_);
    if (!file.is_open()) {
        return false;
    }

    try {
        nlohmann::json root;
        file >> root;

        if (!root.contains("entries") || !root["entries"].is_array()) {
            return false;
        }

        for (const auto& item : root["entries"]) {
            entries.push_back(fromJson(item));
        }
    } catch (const nlohmann::json::exception&) {
        entries.clear();
        return false;
    } catch (const std::exception&) {
        entries.clear();
        return false;
    }

    return true;
}

bool QuarantineRepository::save(
    const std::vector<QuarantineEntry>& entries) const
{
    nlohmann::json root;
    root["entries"] = nlohmann::json::array();

    for (const QuarantineEntry& entry : entries) {
        root["entries"].push_back(toJson(entry));
    }

    const fs::path temp_file = metadata_file_.string() + ".tmp";

    {
        std::ofstream file(temp_file);
        if (!file.is_open()) {
            return false;
        }

        file << root.dump(2);
        file.flush();

        if (!file.good()) {
            return false;
        }
    }

    std::error_code error;
    fs::rename(temp_file, metadata_file_, error);

    if (error) {
        fs::remove(temp_file, error);
        return false;
    }

    return true;
}

std::optional<QuarantineEntry> QuarantineRepository::findById(
    const std::string& id) const
{
    std::scoped_lock lock(mutex_);

    for (const QuarantineEntry& entry : entries_) {
        if (entry.id == id) {
            return entry;
        }
    }

    return std::nullopt;
}

bool QuarantineRepository::add(const QuarantineEntry& entry)
{
    std::scoped_lock lock(mutex_);

    entries_.push_back(entry);
    return save(entries_);
}

bool QuarantineRepository::remove(const std::string& id)
{
    std::scoped_lock lock(mutex_);

    const auto it = std::remove_if(
        entries_.begin(),
        entries_.end(),
        [&](const QuarantineEntry& entry) {
            return entry.id == id;
        });

    if (it == entries_.end()) {
        return false;
    }

    entries_.erase(it, entries_.end());
    return save(entries_);
}

std::vector<QuarantineEntry> QuarantineRepository::list() const
{
    std::scoped_lock lock(mutex_);
    return entries_;
}

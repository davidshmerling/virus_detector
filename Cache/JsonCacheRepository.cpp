#include "Cache/JsonCacheRepository.h"

#include <fstream>
#include <system_error>

#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

std::string verdictToString(FileVerdict verdict)
{
    switch (verdict) {
        case FileVerdict::Clean:
            return "clean";

        case FileVerdict::Malicious:
            return "malicious";

        case FileVerdict::Error:
            return "error";
    }

    return "error";
}

FileVerdict verdictFromString(const std::string& value)
{
    if (value == "clean") {
        return FileVerdict::Clean;
    }

    if (value == "malicious") {
        return FileVerdict::Malicious;
    }

    return FileVerdict::Error;
}

}  // namespace

JsonCacheRepository::JsonCacheRepository(fs::path cache_file)
    : cache_file_(std::move(cache_file))
{
}

bool JsonCacheRepository::initialize()
{
    std::error_code error;

    const fs::path parent = cache_file_.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, error);
        if (error) {
            return false;
        }
    }

    if (fs::exists(cache_file_, error)) {
        return !error;
    }

    CacheMap empty_cache;
    return save(empty_cache, empty_cache, {});
}

bool JsonCacheRepository::load(CacheMap& entries) const
{
    entries.clear();

    std::ifstream file(cache_file_);
    if (!file.is_open()) {
        return false;
    }

    json document;

    try {
        file >> document;

        if (!document.contains("entries") ||
            !document["entries"].is_object()) {
            return false;
        }

        for (const auto& [path, value] : document["entries"].items()) {
            CacheEntry entry;

            entry.file_last_modified =
                value.at("file_last_modified").get<std::int64_t>();

            entry.file_size =
                value.at("file_size").get<std::uintmax_t>();

            entry.signatures_last_modified =
                value.at("signatures_last_modified").get<std::int64_t>();

            entry.verdict =
                verdictFromString(
                    value.at("verdict").get<std::string>());

            entries.emplace(path, entry);
        }
    } catch (const json::exception&) {
        entries.clear();
        return false;
    } catch (const std::exception&) {
        entries.clear();
        return false;
    }

    return true;
}

bool JsonCacheRepository::save(
    const CacheMap& snapshot,
    const CacheMap& /*dirty_entries*/,
    const std::unordered_set<std::string>& /*removed_paths*/) const
{
    json document;
    document["entries"] = json::object();

    for (const auto& [path, entry] : snapshot) {
        document["entries"][path] = {
            {"file_last_modified", entry.file_last_modified},
            {"file_size", entry.file_size},
            {"signatures_last_modified", entry.signatures_last_modified},
            {"verdict", verdictToString(entry.verdict)}
        };
    }

    const fs::path temporary_file = cache_file_.string() + ".tmp";

    {
        std::ofstream file(temporary_file, std::ios::trunc);
        if (!file.is_open()) {
            return false;
        }

        file << document.dump(2);
        file.flush();

        if (!file) {
            return false;
        }
    }

    std::error_code error;
    fs::rename(temporary_file, cache_file_, error);
    if (error) {
        std::error_code cleanup_error;
        fs::remove(temporary_file, cleanup_error);
        return false;
    }

    return true;
}

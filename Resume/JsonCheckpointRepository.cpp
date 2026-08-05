#include "Resume/JsonCheckpointRepository.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;
using json = nlohmann::json;

JsonCheckpointRepository::JsonCheckpointRepository(fs::path checkpoint_file)
    : checkpoint_file_(std::move(checkpoint_file))
{
}

bool JsonCheckpointRepository::initialize()
{
    std::error_code error;

    const fs::path parent = checkpoint_file_.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, error);
        if (error) {
            return false;
        }
    }

    return true;
}

bool JsonCheckpointRepository::exists() const
{
    std::error_code error;
    return fs::exists(checkpoint_file_, error) && !error;
}

bool JsonCheckpointRepository::load(ScanCheckpoint& checkpoint) const
{
    std::ifstream file(checkpoint_file_);
    if (!file.is_open()) {
        return false;
    }

    try {
        json document;
        file >> document;

        checkpoint.version = document.at("version").get<int>();
        checkpoint.root = document.at("root").get<std::string>();
        checkpoint.next_unfinished_path =
            document.at("next_unfinished_path").get<std::string>();
        checkpoint.status = document.at("status").get<std::string>();
    } catch (const json::exception&) {
        return false;
    } catch (const std::exception&) {
        return false;
    }

    return true;
}

bool JsonCheckpointRepository::save(const ScanCheckpoint& checkpoint) const
{
    json document = {
        {"version", checkpoint.version},
        {"root", checkpoint.root.generic_string()},
        {"next_unfinished_path", checkpoint.next_unfinished_path.generic_string()},
        {"status", checkpoint.status}
    };

    const fs::path temporary_file = checkpoint_file_.string() + ".tmp";

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
    fs::rename(temporary_file, checkpoint_file_, error);
    if (error) {
        std::error_code cleanup_error;
        fs::remove(temporary_file, cleanup_error);
        return false;
    }

    return true;
}

bool JsonCheckpointRepository::remove() const
{
    std::error_code error;
    fs::remove(checkpoint_file_, error);
    return !error;
}

#include "Quarantine/QuarantineFileOperations.h"

#include <chrono>
#include <random>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

QuarantineFileOperations::QuarantineFileOperations(fs::path files_directory)
    : files_directory_(std::move(files_directory))
{
}

bool QuarantineFileOperations::prepareDirectory() const
{
    std::error_code error;
    fs::create_directories(files_directory_, error);
    return !error;
}

fs::path QuarantineFileOperations::reserveDestination(
    const fs::path& original,
    std::string& out_id) const
{
    std::error_code error;

    while (true) {
        const std::string id = generateId();
        const fs::path destination =
            files_directory_ / (id + original.extension().string());

        if (!fs::exists(destination, error) && !error) {
            out_id = id;
            return destination;
        }

        error.clear();
    }
}

bool QuarantineFileOperations::moveIn(
    const fs::path& source,
    const fs::path& destination) const
{
    return moveFile(source, destination);
}

bool QuarantineFileOperations::moveOut(
    const fs::path& quarantine_path,
    const fs::path& original_path) const
{
    std::error_code error;
    fs::create_directories(original_path.parent_path(), error);
    if (error) {
        return false;
    }

    return moveFile(quarantine_path, original_path);
}

bool QuarantineFileOperations::remove(const fs::path& quarantine_path) const
{
    std::error_code error;
    fs::remove(quarantine_path, error);
    return !error;
}

bool QuarantineFileOperations::moveFile(
    const fs::path& source,
    const fs::path& destination)
{
    std::error_code error;
    fs::rename(source, destination, error);
    if (!error) {
        return true;
    }

    // rename fails across filesystems: fall back to copy + delete.
    fs::copy_file(source, destination, fs::copy_options::none, error);
    if (error) {
        return false;
    }

    fs::remove(source, error);
    return !error;
}

std::string QuarantineFileOperations::generateId() const
{
    static std::mt19937 random(static_cast<unsigned>(
        std::chrono::steady_clock::now().time_since_epoch().count()));

    std::uniform_int_distribution<int> dist(0, 15);

    std::ostringstream stream;
    for (int i = 0; i < 12; ++i) {
        stream << std::hex << dist(random);
    }

    return stream.str();
}

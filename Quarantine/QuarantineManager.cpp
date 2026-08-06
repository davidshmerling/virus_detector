#include "Quarantine/QuarantineManager.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <random>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

QuarantineManager::QuarantineManager(
    fs::path quarantine_directory,
    Logger& logger)
    : quarantine_directory_(std::move(quarantine_directory)),
      files_directory_(quarantine_directory_ / "files"),
      logger_(logger),
      repository_(quarantine_directory_ / "metadata.json")
{
}

bool QuarantineManager::moveFile(
    const fs::path& source,
    const fs::path& destination)
{
    std::error_code error;
    fs::rename(source, destination, error);

    if (!error) {
        return true;
    }

    // Never overwrite an existing destination.
    fs::copy_file(
        source,
        destination,
        fs::copy_options::none,
        error);

    if (error) {
        return false;
    }

    fs::remove(source, error);
    return !error;
}

bool QuarantineManager::initialize()
{
    std::error_code error;
    fs::create_directories(files_directory_, error);

    if (error) {
        logger_.error("Could not create quarantine files directory");
        return false;
    }

    if (!repository_.initialize()) {
        logger_.error("Could not initialize quarantine metadata");
        return false;
    }

    logger_.info("Quarantine initialized");
    return true;
}

bool QuarantineManager::quarantine(
    const fs::path& file_path,
    const std::vector<std::string>& signatures)
{
    std::scoped_lock lock(mutex_);

    std::error_code error;

    if (!fs::exists(file_path, error) || error) {
        logger_.error("Quarantine failed: file does not exist");
        return false;
    }

    if (!fs::is_regular_file(file_path, error) || error) {
        logger_.error("Quarantine failed: not a regular file");
        return false;
    }

    const UniqueDestination destination =
        createUniqueDestination(file_path);

    if (!moveFile(file_path, destination.path)) {
        logger_.error(
            "Quarantine failed: could not move file to quarantine");
        return false;
    }

    QuarantineEntry entry;
    entry.id = destination.id;
    entry.original_path = file_path;
    entry.quarantine_path = destination.path;
    entry.signatures = signatures;
    entry.file_size = fs::file_size(destination.path, error);
    entry.quarantined_at = currentTime();

    if (!repository_.add(entry)) {
        moveFile(destination.path, file_path);
        logger_.error(
            "Quarantine failed: could not save metadata, file restored");
        return false;
    }

    logger_.info(
        "File quarantined: " + file_path.string() +
        ", id: " + destination.id);
    return true;
}

bool QuarantineManager::restore(const std::string& id)
{
    std::scoped_lock lock(mutex_);

    const std::optional<QuarantineEntry> entry =
        repository_.findById(id);

    if (!entry) {
        logger_.error("Restore failed: id not found");
        return false;
    }

    std::error_code error;

    if (!fs::exists(entry->quarantine_path, error) || error) {
        logger_.error("Restore failed: quarantined file missing");
        return false;
    }

    if (fs::exists(entry->original_path, error) && !error) {
        logger_.error("Restore failed: original path already exists");
        return false;
    }

    fs::create_directories(
        entry->original_path.parent_path(),
        error);

    if (error) {
        logger_.error("Restore failed: could not create parent directory");
        return false;
    }

    if (!moveFile(entry->quarantine_path, entry->original_path)) {
        logger_.error("Restore failed: could not move file back");
        return false;
    }

    if (!repository_.remove(id)) {
        logger_.error(
            "Restore warning: file restored but metadata not updated");
        return false;
    }

    logger_.info(
        "File restored: " + entry->original_path.string() +
        ", id: " + id);
    return true;
}

bool QuarantineManager::remove(const std::string& id)
{
    std::scoped_lock lock(mutex_);

    const std::optional<QuarantineEntry> entry =
        repository_.findById(id);

    if (!entry) {
        logger_.error("Delete failed: id not found");
        return false;
    }

    std::error_code error;
    fs::remove(entry->quarantine_path, error);

    if (error) {
        logger_.error("Delete failed: could not remove quarantined file");
        return false;
    }

    if (!repository_.remove(id)) {
        logger_.error("Delete failed: could not update metadata");
        return false;
    }

    logger_.info("Quarantined file deleted, id: " + id);
    return true;
}

std::vector<QuarantineEntry> QuarantineManager::list() const
{
    std::scoped_lock lock(mutex_);
    return repository_.list();
}

QuarantineManager::UniqueDestination
QuarantineManager::createUniqueDestination(
    const fs::path& original_path) const
{
    std::error_code error;

    while (true) {
        const std::string id = generateId();
        const fs::path destination =
            files_directory_ /
            (id + original_path.extension().string());

        if (!fs::exists(destination, error) && !error) {
            return {id, destination};
        }

        error.clear();
    }
}

std::string QuarantineManager::generateId() const
{
    static std::mt19937 random(
        static_cast<unsigned>(
            std::chrono::steady_clock::now().time_since_epoch().count()));

    std::uniform_int_distribution<int> dist(0, 15);

    std::ostringstream stream;
    for (int i = 0; i < 12; ++i) {
        stream << std::hex << dist(random);
    }

    return stream.str();
}

std::string QuarantineManager::currentTime() const
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);

    std::tm local_time{};

#ifdef _WIN32
    localtime_s(&local_time, &time);
#else
    localtime_r(&time, &local_time);
#endif

    std::ostringstream stream;
    stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

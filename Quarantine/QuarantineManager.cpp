#include "Quarantine/QuarantineManager.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <optional>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

QuarantineManager::QuarantineManager(
    Logger& logger,
    fs::path quarantine_directory)
    : logger_(logger),
      repository_(quarantine_directory / "metadata.json"),
      file_ops_(quarantine_directory / "files")
{
}

bool QuarantineManager::load()
{
    std::scoped_lock lock(mutex_);

    if (!file_ops_.prepareDirectory()) {
        logger_.error("Could not create quarantine directory");
        return false;
    }

    if (!repository_.load()) {
        logger_.error("Quarantine metadata is corrupt");
        return false;
    }

    return true;
}

bool QuarantineManager::quarantine(
    const fs::path& file,
    const std::vector<std::string>& signatures)
{
    std::scoped_lock lock(mutex_);

    std::error_code error;
    if (!fs::is_regular_file(file, error) || error) {
        logger_.error("Quarantine failed: not a regular file");
        return false;
    }

    // Capture the original permissions before the file is moved away, so a
    // later restore can put it back exactly as it was.
    std::error_code perms_error;
    const fs::file_status original_status = fs::status(file, perms_error);
    const fs::perms original_permissions =
        perms_error ? fs::perms::unknown : original_status.permissions();

    std::string id;
    const fs::path destination = file_ops_.reserveDestination(file, id);

    if (!file_ops_.moveIn(file, destination)) {
        logger_.error("Quarantine failed: could not move file");
        return false;
    }

    QuarantineEntry entry;
    entry.id = id;
    entry.original_path = file;
    entry.quarantine_path = destination;
    entry.signatures = signatures;
    entry.file_size = fs::file_size(destination, error);
    entry.quarantined_at = currentTime();
    entry.original_permissions = original_permissions;

    repository_.add(entry);

    if (!repository_.save()) {
        // Roll back so the file is not lost with no record of it.
        file_ops_.moveIn(destination, file);
        repository_.removeById(id);
        logger_.error("Quarantine failed: could not save metadata");
        return false;
    }

    logger_.info("File quarantined: " + file.string() + ", id: " + id);
    return true;
}

bool QuarantineManager::restore(const std::string& id)
{
    std::scoped_lock lock(mutex_);

    if (!restoreOne(id)) {
        return false;
    }

    return repository_.save();
}

bool QuarantineManager::restoreAll()
{
    std::scoped_lock lock(mutex_);

    bool all_ok = true;
    for (const QuarantineEntry& entry : repository_.all()) {
        all_ok = restoreOne(entry.id) && all_ok;
    }

    return repository_.save() && all_ok;
}

bool QuarantineManager::remove(const std::string& id)
{
    std::scoped_lock lock(mutex_);

    if (!removeOne(id)) {
        return false;
    }

    return repository_.save();
}

std::vector<QuarantineEntry> QuarantineManager::list() const
{
    std::scoped_lock lock(mutex_);
    return repository_.all();
}

bool QuarantineManager::restoreOne(const std::string& id)
{
    const std::optional<QuarantineEntry> entry = repository_.find(id);
    if (!entry) {
        logger_.error("Restore failed: id not found: " + id);
        return false;
    }

    std::error_code error;
    if (fs::exists(entry->original_path, error) && !error) {
        logger_.error(
            "Restore failed: original path already exists: " +
            entry->original_path.string());
        return false;
    }

    if (!file_ops_.moveOut(entry->quarantine_path, entry->original_path)) {
        logger_.error("Restore failed: could not move file back");
        return false;
    }

    // Moving across filesystems (copy + delete) does not preserve permissions,
    // so reapply the ones recorded at quarantine time.
    if (entry->original_permissions != fs::perms::unknown) {
        std::error_code perms_error;
        fs::permissions(
            entry->original_path,
            entry->original_permissions,
            fs::perm_options::replace,
            perms_error);
        if (perms_error) {
            logger_.warning(
                "Restore: could not reapply permissions to " +
                entry->original_path.string());
        }
    }

    logger_.info("File restored: " + entry->original_path.string());
    repository_.removeById(id);
    return true;
}

bool QuarantineManager::removeOne(const std::string& id)
{
    const std::optional<QuarantineEntry> entry = repository_.find(id);
    if (!entry) {
        logger_.error("Delete failed: id not found: " + id);
        return false;
    }

    if (!file_ops_.remove(entry->quarantine_path)) {
        logger_.error("Delete failed: could not remove quarantined file");
        return false;
    }

    logger_.info("Quarantined file deleted, id: " + id);
    repository_.removeById(id);
    return true;
}

std::string QuarantineManager::currentTime()
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

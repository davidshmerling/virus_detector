#include "Resume/ResumeManager.h"

#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

ResumeManager::ResumeManager(Logger& logger, fs::path checkpoint_file)
    : logger_(logger),
      checkpoint_file_(std::move(checkpoint_file))
{
}

bool ResumeManager::begin(
    const fs::path& scan_root,
    bool& resumed)
{
    std::scoped_lock lock(mutex_);

    unfinished_files_.clear();
    discovery_finished_ = false;
    resumed = false;

    root_ = fs::absolute(scan_root);

    fs::path saved_root;
    std::string saved_status;

    if (load(saved_root, saved_status, next_file_) &&
        saved_status == "running" &&
        saved_root == root_) {
        resumed = true;
        return true;
    }

    next_file_.clear();
    return save("running");
}

bool ResumeManager::addFile(const fs::path& file)
{
    std::scoped_lock lock(mutex_);

    unfinished_files_.insert(file.generic_string());
    next_file_ = *unfinished_files_.begin();

    // Discovery alone is not progress — checkpoints advance on fileCompleted.
    return true;
}

bool ResumeManager::fileCompleted(const fs::path& file)
{
    std::scoped_lock lock(mutex_);

    unfinished_files_.erase(file.generic_string());

    if (unfinished_files_.empty()) {
        if (discovery_finished_) {
            next_file_.clear();
            return save("completed");
        }

        return true;
    }

    next_file_ = *unfinished_files_.begin();
    return maybeSave("running");
}

bool ResumeManager::discoveryFinished()
{
    std::scoped_lock lock(mutex_);

    discovery_finished_ = true;

    if (unfinished_files_.empty()) {
        next_file_.clear();
        return save("completed");
    }

    return save("running");
}

const fs::path& ResumeManager::nextFile() const
{
    return next_file_;
}

bool ResumeManager::load(
    fs::path& root,
    std::string& status,
    fs::path& next)
{
    std::ifstream file(checkpoint_file_);

    std::string root_text;
    std::string next_text;

    if (!file ||
        !std::getline(file, root_text) ||
        !std::getline(file, status) ||
        !std::getline(file, next_text)) {
        return false;
    }

    root = root_text;
    next = next_text;

    return true;
}

bool ResumeManager::save(const std::string& status)
{
    const fs::path directory = checkpoint_file_.parent_path();
    if (!directory.empty()) {
        std::error_code error;
        fs::create_directories(directory, error);
    }

    // Write to a temp file, then rename into place. rename is atomic on the
    // same filesystem, so an interrupted save leaves the previous checkpoint
    // intact instead of a half-written (truncated) one.
    const fs::path temp_file = checkpoint_file_.string() + ".tmp";

    {
        std::ofstream file(temp_file, std::ios::trunc);

        if (!file) {
            logger_.error("Could not save resume checkpoint");
            return false;
        }

        file << root_.generic_string() << '\n'
             << status << '\n'
             << next_file_.generic_string() << '\n';
    }

    std::error_code error;
    fs::rename(temp_file, checkpoint_file_, error);
    if (error) {
        fs::remove(temp_file, error);
        logger_.error("Could not save resume checkpoint");
        return false;
    }

    since_last_save_ = 0;
    return true;
}

bool ResumeManager::maybeSave(const std::string& status)
{
    if (++since_last_save_ < kCheckpointInterval) {
        return true;
    }

    return save(status);
}

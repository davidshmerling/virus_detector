#include "Resume/ResumeManager.h"

#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

ResumeManager::ResumeManager(Logger& logger, fs::path checkpoint_file)
    : logger_(logger),
      checkpoint_file_(std::move(checkpoint_file))
{
}

const fs::path& ResumeManager::root() const
{
    return root_;
}

const fs::path& ResumeManager::next() const
{
    return next_;
}

bool ResumeManager::begin(const fs::path& root, bool& resumed)
{
    std::scoped_lock lock(mutex_);

    unfinished_.clear();
    enumeration_finished_ = false;
    dirty_ = 0;
    resumed = false;

    root_ = normalizeRoot(root);

    fs::path saved_root;
    fs::path saved_next;
    std::string saved_status;

    if (load(saved_root, saved_status, saved_next) &&
        saved_status == "running" &&
        normalizeRoot(saved_root) == root_) {
        next_ = saved_next;
        status_ = "running";
        resumed = true;
        return true;
    }

    next_.clear();
    status_ = "running";
    return saveLocked();
}

bool ResumeManager::registerTask(const fs::path& path)
{
    std::scoped_lock lock(mutex_);
    unfinished_.insert(pathKey(path));
    return updateFrontierLocked();
}

bool ResumeManager::markCompleted(const fs::path& path)
{
    std::scoped_lock lock(mutex_);
    unfinished_.erase(pathKey(path));

    if (enumeration_finished_ && unfinished_.empty()) {
        status_ = "completed";
        next_.clear();
        return saveLocked();
    }

    return updateFrontierLocked();
}

bool ResumeManager::markEnumerationFinished()
{
    std::scoped_lock lock(mutex_);
    enumeration_finished_ = true;

    if (unfinished_.empty()) {
        status_ = "completed";
        next_.clear();
    }

    return saveLocked();
}

bool ResumeManager::flush()
{
    std::scoped_lock lock(mutex_);
    return saveLocked();
}

bool ResumeManager::updateFrontierLocked()
{
    if (unfinished_.empty()) {
        return true;
    }

    const fs::path new_next = *unfinished_.begin();
    if (new_next == next_) {
        return true;
    }

    next_ = new_next;
    if (++dirty_ < flush_interval_) {
        return true;
    }

    return saveLocked();
}

std::string ResumeManager::pathKey(const fs::path& path)
{
    return path.lexically_normal().generic_string();
}

fs::path ResumeManager::normalizeRoot(const fs::path& root)
{
    std::error_code error;
    const fs::path absolute = fs::absolute(root, error);
    return error ? root.lexically_normal() : absolute.lexically_normal();
}

bool ResumeManager::load(
    fs::path& root,
    std::string& status,
    fs::path& next) const
{
    std::ifstream file(checkpoint_file_);
    if (!file) {
        return false;
    }

    std::string root_line;
    std::string next_line;

    if (!std::getline(file, root_line) ||
        !std::getline(file, status) ||
        !std::getline(file, next_line)) {
        return false;
    }

    root = root_line;
    next = next_line;
    return true;
}

bool ResumeManager::saveLocked()
{
    const fs::path parent = checkpoint_file_.parent_path();
    if (!parent.empty()) {
        std::error_code error;
        fs::create_directories(parent, error);
        if (error) {
            logger_.error("Could not create checkpoint directory");
            return false;
        }
    }

    const fs::path temporary = checkpoint_file_.string() + ".tmp";

    {
        std::ofstream file(temporary, std::ios::trunc);
        if (!file) {
            logger_.error("Could not write checkpoint");
            return false;
        }

        file << root_.generic_string() << '\n'
             << status_ << '\n'
             << next_.generic_string() << '\n';
    }

    std::error_code error;
    fs::rename(temporary, checkpoint_file_, error);
    if (error) {
        fs::remove(temporary);
        logger_.error("Could not save checkpoint");
        return false;
    }

    dirty_ = 0;
    return true;
}

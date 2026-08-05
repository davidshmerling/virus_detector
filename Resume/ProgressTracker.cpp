#include "Resume/ProgressTracker.h"

#include <utility>

namespace fs = std::filesystem;

ProgressTracker::ProgressTracker(
    JsonCheckpointRepository& repository,
    Logger& logger)
    : repository_(repository),
      logger_(logger)
{
}

bool ProgressTracker::startNewScan(const fs::path& root)
{
    std::scoped_lock lock(mutex_);

    unfinished_paths_.clear();
    enumeration_finished_ = false;

    checkpoint_.version = 2;
    checkpoint_.root = fs::absolute(root).lexically_normal();
    checkpoint_.next_unfinished_path.clear();
    checkpoint_.status = "running";

    return saveLocked();
}

bool ProgressTracker::resumeScan(const ScanCheckpoint& checkpoint)
{
    std::scoped_lock lock(mutex_);

    unfinished_paths_.clear();
    enumeration_finished_ = false;
    checkpoint_ = checkpoint;

    return true;
}

bool ProgressTracker::registerTask(const fs::path& relative_path)
{
    std::scoped_lock lock(mutex_);

    const std::string old_first =
        unfinished_paths_.empty() ? "" : *unfinished_paths_.begin();

    unfinished_paths_.insert(pathKey(relative_path));

    const std::string new_first = *unfinished_paths_.begin();

    if (old_first == new_first) {
        return true;
    }

    return saveLocked();
}

bool ProgressTracker::cancelTask(const fs::path& relative_path)
{
    std::scoped_lock lock(mutex_);

    unfinished_paths_.erase(pathKey(relative_path));
    return saveLocked();
}

bool ProgressTracker::markCompleted(const fs::path& relative_path)
{
    std::scoped_lock lock(mutex_);

    const std::string key = pathKey(relative_path);

    const std::string old_first =
        unfinished_paths_.empty() ? "" : *unfinished_paths_.begin();

    unfinished_paths_.erase(key);

    if (enumeration_finished_ && unfinished_paths_.empty()) {
        checkpoint_.status = "completed";
        checkpoint_.next_unfinished_path.clear();
        return saveLocked();
    }

    const std::string new_first =
        unfinished_paths_.empty() ? "" : *unfinished_paths_.begin();

    if (old_first == new_first) {
        return true;
    }

    return saveLocked();
}

bool ProgressTracker::markEnumerationFinished()
{
    std::scoped_lock lock(mutex_);

    enumeration_finished_ = true;

    if (unfinished_paths_.empty()) {
        checkpoint_.status = "completed";
        checkpoint_.next_unfinished_path.clear();
    }

    return saveLocked();
}

bool ProgressTracker::flush()
{
    std::scoped_lock lock(mutex_);
    return saveLocked();
}

const ScanCheckpoint& ProgressTracker::checkpoint() const
{
    return checkpoint_;
}

std::string ProgressTracker::pathKey(const fs::path& relative_path)
{
    return relative_path.lexically_normal().generic_string();
}

void ProgressTracker::updateNextUnfinished()
{
    if (unfinished_paths_.empty()) {
        checkpoint_.next_unfinished_path.clear();
    } else {
        checkpoint_.next_unfinished_path = *unfinished_paths_.begin();
    }
}

bool ProgressTracker::saveLocked()
{
    updateNextUnfinished();

    if (!repository_.save(checkpoint_)) {
        logger_.error("Could not save checkpoint");
        return false;
    }

    return true;
}

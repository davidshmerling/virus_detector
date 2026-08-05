#include "Resume/ProgressTracker.h"

#include <utility>

namespace fs = std::filesystem;

ProgressTracker::ProgressTracker(
    JsonCheckpointRepository& repository,
    Logger& logger,
    std::size_t flush_interval)
    : repository_(repository),
      logger_(logger),
      flush_interval_(flush_interval == 0 ? 1 : flush_interval)
{
}

bool ProgressTracker::startNewScan(const fs::path& root)
{
    std::lock_guard<std::mutex> lock(mutex_);

    unfinished_paths_.clear();
    enumeration_finished_ = false;
    dirty_count_ = 0;

    checkpoint_.version = 2;
    checkpoint_.root = fs::absolute(root).lexically_normal();
    checkpoint_.next_unfinished_path.clear();
    checkpoint_.status = "running";

    return saveLocked(true);
}

bool ProgressTracker::resumeScan(const ScanCheckpoint& checkpoint)
{
    std::lock_guard<std::mutex> lock(mutex_);

    unfinished_paths_.clear();
    enumeration_finished_ = false;
    dirty_count_ = 0;
    checkpoint_ = checkpoint;

    return true;
}

bool ProgressTracker::registerTask(const fs::path& relative_path)
{
    std::lock_guard<std::mutex> lock(mutex_);

    unfinished_paths_.insert(pathKey(relative_path));
    ++dirty_count_;

    return saveLocked(false);
}

bool ProgressTracker::cancelTask(const fs::path& relative_path)
{
    std::lock_guard<std::mutex> lock(mutex_);

    unfinished_paths_.erase(pathKey(relative_path));
    ++dirty_count_;

    // Keep disk consistent after a failed enqueue.
    return saveLocked(true);
}

bool ProgressTracker::markCompleted(const fs::path& relative_path)
{
    std::lock_guard<std::mutex> lock(mutex_);

    unfinished_paths_.erase(pathKey(relative_path));
    ++dirty_count_;

    if (enumeration_finished_ && unfinished_paths_.empty()) {
        checkpoint_.status = "completed";
        checkpoint_.next_unfinished_path.clear();
        return saveLocked(true);
    }

    return saveLocked(false);
}

bool ProgressTracker::markEnumerationFinished()
{
    std::lock_guard<std::mutex> lock(mutex_);

    enumeration_finished_ = true;

    if (unfinished_paths_.empty()) {
        checkpoint_.status = "completed";
        checkpoint_.next_unfinished_path.clear();
    }

    return saveLocked(true);
}

bool ProgressTracker::flush()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return saveLocked(true);
}

const ScanCheckpoint& ProgressTracker::checkpoint() const
{
    return checkpoint_;
}

std::string ProgressTracker::pathKey(const fs::path& relative_path)
{
    return relative_path.lexically_normal().generic_string();
}

void ProgressTracker::refreshNextPathLocked()
{
    if (unfinished_paths_.empty()) {
        checkpoint_.next_unfinished_path.clear();
    } else {
        checkpoint_.next_unfinished_path = *unfinished_paths_.begin();
    }
}

bool ProgressTracker::saveLocked(bool force)
{
    refreshNextPathLocked();

    if (!force && dirty_count_ < flush_interval_) {
        return true;
    }

    if (!repository_.save(checkpoint_)) {
        logger_.error("Could not save checkpoint");
        return false;
    }

    dirty_count_ = 0;
    return true;
}

#include "Scanner/FileEnumerator/FileEnumerator.h"

#include <algorithm>
#include <atomic>
#include <format>
#include <system_error>
#include <thread>

namespace fs = std::filesystem;

FileEnumerator::FileEnumerator(
    const ExcludeManager& exclude_manager,
    Logger& logger,
    PerformanceProfiler& profiler)
    : exclude_manager_(exclude_manager),
      logger_(logger),
      profiler_(profiler)
{
}

bool FileEnumerator::readSortedChildren(
    const fs::path& directory,
    std::vector<fs::directory_entry>& children) const
{
    children.clear();

    {
        ScopedPerformanceTimer timer(
            profiler_,
            PerformanceSection::DirectoryRead);

        std::error_code error;
        fs::directory_iterator iterator(
            directory,
            fs::directory_options::skip_permission_denied,
            error);

        if (error) {
            logger_.warning(
                std::format(
                    "Open directory failed for {}: {}",
                    directory.string(),
                    error.message()));
            return false;
        }

        const fs::directory_iterator end;

        while (iterator != end) {
            const fs::path entry_path = iterator->path();
            children.push_back(*iterator);

            iterator.increment(error);
            if (error) {
                logger_.warning(
                    std::format(
                        "Read directory entry failed for {}: {}",
                        entry_path.string(),
                        error.message()));
                error.clear();
            }
        }
    }

    {
        ScopedPerformanceTimer timer(
            profiler_,
            PerformanceSection::DirectorySort);

        std::ranges::sort(
            children,
            [](const fs::directory_entry& left,
               const fs::directory_entry& right) {
                return left.path().filename().generic_string()
                     < right.path().filename().generic_string();
            });
    }

    return true;
}

void FileEnumerator::logEntryAccessError(
    const fs::path& path,
    const char* action,
    const std::error_code& error) const
{
    if (!error) {
        return;
    }

    logger_.warning(
        std::string(action) + " failed for " + path.string() +
        ": " + error.message());
}

bool FileEnumerator::skipSymbolicLink(
    const fs::path& path,
    ScanSummary& summary) const
{
    std::error_code error;
    const fs::file_status status = fs::symlink_status(path, error);

    if (error) {
        logEntryAccessError(path, "Stat", error);
        return false;
    }

    if (status.type() != fs::file_type::symlink) {
        return false;
    }

    logger_.warning("Skipping symbolic link: " + path.string());
    ++summary.excluded;
    return true;
}

bool FileEnumerator::shouldSkip(
    const fs::path& path,
    ScanSummary& summary) const
{
    if (skipSymbolicLink(path, summary)) {
        return true;
    }

    if (exclude_manager_.isExcluded(path)) {
        ++summary.excluded;
        return true;
    }

    return false;
}

bool FileEnumerator::processWholeEntry(
    const fs::directory_entry& entry,
    ScanSummary& summary,
    const FileCallback& on_file) const
{
    const fs::path path = entry.path();

    if (shouldSkip(path, summary)) {
        return true;
    }

    std::error_code error;

    if (entry.is_directory(error)) {
        if (error) {
            logEntryAccessError(path, "Stat", error);
            return true;
        }

        return walkAll(path, summary, on_file);
    }

    error.clear();

    if (entry.is_regular_file(error)) {
        if (error) {
            logEntryAccessError(path, "Stat", error);
            return true;
        }

        return on_file(path);
    }

    return true;
}

bool FileEnumerator::enumerateSorted(
    const fs::path& root,
    ScanSummary& summary,
    const FileCallback& on_file) const
{
    std::error_code error;

    if (!fs::exists(root, error) || error) {
        logEntryAccessError(root, "Access", error);
        return false;
    }

    if (shouldSkip(root, summary)) {
        return true;
    }

    if (fs::is_regular_file(root, error)) {
        if (error) {
            logEntryAccessError(root, "Stat", error);
            return true;
        }

        return on_file(root);
    }

    if (!fs::is_directory(root, error) || error) {
        logEntryAccessError(root, "Open directory", error);
        return false;
    }

    // 1) Read and sort direct children of root (lexicographic).
    std::vector<fs::directory_entry> root_children;
    if (!readSortedChildren(root, root_children)) {
        return true;
    }

    if (root_children.empty()) {
        return true;
    }

    // 2) Workers claim the next child index atomically and DFS that subtree.
    const std::size_t worker_count = std::min(
        kEnumerationWorkers,
        root_children.size());

    logger_.info(
        "Parallel enumeration: " +
        std::to_string(worker_count) +
        " workers over " +
        std::to_string(root_children.size()) +
        " root children");

    std::atomic<std::size_t> next_child{0};
    std::atomic<bool> failed{false};

    std::vector<std::thread> workers;
    workers.reserve(worker_count);

    for (std::size_t i = 0; i < worker_count; ++i) {
        workers.emplace_back(
            [this,
             &root_children,
             &next_child,
             &failed,
             &summary,
             &on_file]() {
                while (!failed.load(std::memory_order_relaxed)) {
                    const std::size_t index = next_child.fetch_add(
                        1,
                        std::memory_order_relaxed);

                    if (index >= root_children.size()) {
                        return;
                    }

                    if (!processWholeEntry(
                            root_children[index],
                            summary,
                            on_file)) {
                        failed.store(true, std::memory_order_relaxed);
                        return;
                    }
                }
            });
    }

    for (std::thread& worker : workers) {
        worker.join();
    }

    return !failed.load(std::memory_order_relaxed);
}

bool FileEnumerator::resumeFromSorted(
    const fs::path& root,
    const fs::path& first_unfinished_path,
    ScanSummary& summary,
    const FileCallback& on_file) const
{
    if (first_unfinished_path.empty()) {
        return enumerateSorted(root, summary, on_file);
    }

    std::error_code error;

    if (!fs::exists(root, error) || error) {
        logEntryAccessError(root, "Access", error);
        return false;
    }

    if (!fs::is_directory(root, error) || error) {
        logEntryAccessError(root, "Open directory", error);
        return false;
    }

    // Resume stays single-threaded: lexicographic skip logic is ordered.
    std::vector<std::string> resume_parts;
    for (const fs::path& part : first_unfinished_path) {
        resume_parts.push_back(part.generic_string());
    }

    return walkResume(root, resume_parts, 0, summary, on_file);
}

bool FileEnumerator::walkAll(
    const fs::path& directory,
    ScanSummary& summary,
    const FileCallback& on_file) const
{
    if (exclude_manager_.isExcluded(directory)) {
        ++summary.excluded;
        return true;
    }

    std::vector<fs::directory_entry> children;
    if (!readSortedChildren(directory, children)) {
        return true;
    }

    for (const fs::directory_entry& entry : children) {
        if (!processWholeEntry(entry, summary, on_file)) {
            return false;
        }
    }

    return true;
}

bool FileEnumerator::walkResume(
    const fs::path& directory,
    const std::vector<std::string>& resume_parts,
    std::size_t depth,
    ScanSummary& summary,
    const FileCallback& on_file) const
{
    if (depth >= resume_parts.size()) {
        return walkAll(directory, summary, on_file);
    }

    if (exclude_manager_.isExcluded(directory)) {
        ++summary.excluded;
        return true;
    }

    const std::string& target_name = resume_parts[depth];

    std::vector<fs::directory_entry> children;
    if (!readSortedChildren(directory, children)) {
        return true;
    }

    for (const fs::directory_entry& entry : children) {
        const fs::path path = entry.path();

        if (skipSymbolicLink(path, summary)) {
            continue;
        }

        const std::string current_name =
            path.filename().generic_string();

        if (current_name < target_name) {
            continue;
        }

        if (current_name > target_name) {
            if (!processWholeEntry(entry, summary, on_file)) {
                return false;
            }
            continue;
        }

        const bool is_last_component =
            depth + 1 == resume_parts.size();

        if (is_last_component) {
            if (!processWholeEntry(entry, summary, on_file)) {
                return false;
            }
            continue;
        }

        if (shouldSkip(path, summary)) {
            continue;
        }

        std::error_code error;
        if (entry.is_directory(error)) {
            if (error) {
                logEntryAccessError(path, "Stat", error);
                continue;
            }

            if (!walkResume(
                    path,
                    resume_parts,
                    depth + 1,
                    summary,
                    on_file)) {
                return false;
            }
        }
    }

    return true;
}

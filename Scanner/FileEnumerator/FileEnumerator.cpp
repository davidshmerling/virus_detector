#include "Scanner/FileEnumerator/FileEnumerator.h"

#include <algorithm>
#include <system_error>

namespace fs = std::filesystem;

FileEnumerator::FileEnumerator(
    const ExcludeManager& exclude_manager,
    Logger& logger)
    : exclude_manager_(exclude_manager),
      logger_(logger)
{
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

    if (exclude_manager_.isExcluded(root)) {
        ++summary.excluded;
        return true;
    }

    if (skipSymbolicLink(root, summary)) {
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

    return walkAll(root, summary, on_file);
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

    const auto children = getSortedChildren(directory);

    for (const fs::directory_entry& entry : children) {
        const fs::path path = entry.path();

        if (skipSymbolicLink(path, summary)) {
            continue;
        }

        std::error_code error;

        if (exclude_manager_.isExcluded(path)) {
            ++summary.excluded;
            continue;
        }

        error.clear();

        if (entry.is_directory(error)) {
            if (error) {
                logEntryAccessError(path, "Stat", error);
                continue;
            }

            if (!walkAll(path, summary, on_file)) {
                return false;
            }

            continue;
        }

        error.clear();

        if (entry.is_regular_file(error)) {
            if (error) {
                logEntryAccessError(path, "Stat", error);
                continue;
            }

            if (!on_file(path)) {
                return false;
            }
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
    const auto children = getSortedChildren(directory);

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

        std::error_code error;

        if (current_name > target_name) {
            if (exclude_manager_.isExcluded(path)) {
                ++summary.excluded;
                continue;
            }

            if (entry.is_directory(error)) {
                if (error) {
                    logEntryAccessError(path, "Stat", error);
                    continue;
                }

                if (!walkAll(path, summary, on_file)) {
                    return false;
                }
            } else {
                error.clear();

                if (entry.is_regular_file(error)) {
                    if (error) {
                        logEntryAccessError(path, "Stat", error);
                        continue;
                    }

                    if (!on_file(path)) {
                        return false;
                    }
                }
            }

            continue;
        }

        const bool is_last_component =
            depth + 1 == resume_parts.size();

        if (is_last_component) {
            if (exclude_manager_.isExcluded(path)) {
                ++summary.excluded;
                continue;
            }

            if (entry.is_regular_file(error)) {
                if (error) {
                    logEntryAccessError(path, "Stat", error);
                    continue;
                }

                if (!on_file(path)) {
                    return false;
                }
            } else if (entry.is_directory(error)) {
                if (error) {
                    logEntryAccessError(path, "Stat", error);
                    continue;
                }

                if (!walkAll(path, summary, on_file)) {
                    return false;
                }
            }

            continue;
        }

        if (exclude_manager_.isExcluded(path)) {
            ++summary.excluded;
            continue;
        }

        if (entry.is_directory(error)) {
            if (error) {
                logEntryAccessError(path, "Stat", error);
                continue;
            }

            if (!walkResume(path, resume_parts, depth + 1, summary, on_file)) {
                return false;
            }
        }
    }

    return true;
}

std::vector<fs::directory_entry> FileEnumerator::getSortedChildren(
    const fs::path& directory) const
{
    std::vector<fs::directory_entry> children;

    std::error_code error;
    fs::directory_iterator iterator(
        directory,
        fs::directory_options::skip_permission_denied,
        error);

    if (error) {
        logEntryAccessError(directory, "Open directory", error);
        return children;
    }

    const fs::directory_iterator end;

    while (iterator != end) {
        const fs::path entry_path = iterator->path();
        children.push_back(*iterator);

        iterator.increment(error);
        if (error) {
            logEntryAccessError(entry_path, "Read directory entry", error);
            error.clear();
        }
    }

    std::sort(
        children.begin(),
        children.end(),
        [](const fs::directory_entry& left,
           const fs::directory_entry& right) {
            return left.path().filename().generic_string()
                 < right.path().filename().generic_string();
        });

    return children;
}

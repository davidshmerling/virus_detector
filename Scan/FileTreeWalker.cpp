#include "Scan/FileTreeWalker.h"

#include <algorithm>
#include <system_error>

namespace fs = std::filesystem;

FileTreeWalker::FileTreeWalker(Logger& logger, const ExcludeManager& exclude)
    : logger_(logger),
      exclude_(exclude)
{
}

bool FileTreeWalker::walk(
    const fs::path& root,
    const fs::path& resume_from,
    const FileCallback& on_file) const
{
    std::error_code error;
    if (!fs::exists(root, error) || error) {
        logger_.warning("Access failed for " + root.string());
        return false;
    }

    // One-time ancestry check for the requested root only.
    // After this, DFS uses exact contains() — excluded dirs are never entered.
    if (exclude_.isScanRootExcluded(root)) {
        logger_.warning("Scan root is excluded: " + root.string());
        return true;
    }

    if (fs::is_regular_file(root, error)) {
        if (error) {
            return false;
        }
        FileInfo info;
        if (!makeFileInfo(root, fs::directory_entry(root), info)) {
            return true;
        }
        return on_file(info);
    }
    if (!fs::is_directory(root, error) || error) {
        logger_.warning("Open directory failed for " + root.string());
        return false;
    }

    std::vector<std::string> parts;
    for (const fs::path& part : resume_from) {
        parts.push_back(part.generic_string());
    }
    return walkDirectory(root, root, parts, 0, on_file);
}

bool FileTreeWalker::walkDirectory(
    const fs::path& root,
    const fs::path& directory,
    const std::vector<std::string>& resume_parts,
    std::size_t depth,
    const FileCallback& on_file) const
{
    if (shouldSkip(directory)) {
        return true;
    }

    std::vector<fs::directory_entry> children;
    if (!readSortedChildren(directory, children)) {
        return true;
    }

    const std::string* target =
        depth < resume_parts.size() ? &resume_parts[depth] : nullptr;

    for (const fs::directory_entry& entry : children) {
        const fs::path path = entry.path();
        if (shouldSkip(path)) {
            continue;
        }

        const std::string name = path.filename().generic_string();
        if (target && name < *target) {
            continue;
        }

        std::error_code error;
        if (target && name == *target && depth + 1 < resume_parts.size()) {
            if (entry.is_directory(error) && !error &&
                !walkDirectory(root, path, resume_parts, depth + 1, on_file)) {
                return false;
            }
            continue;
        }

        if (entry.is_directory(error)) {
            if (!error && !walkDirectory(root, path, {}, 0, on_file)) {
                return false;
            }
        } else if (entry.is_regular_file(error) && !error) {
            FileInfo info;
            if (!makeFileInfo(root, entry, info)) {
                continue;
            }
            if (!on_file(info)) {
                return false;
            }
        }
    }
    return true;
}

bool FileTreeWalker::makeFileInfo(
    const fs::path& root,
    const fs::directory_entry& entry,
    FileInfo& info) const
{
    std::error_code error;

    // Reuses the stat the directory_entry already cached during the DFS, so
    // this does not issue new filesystem calls in the common case.
    const std::uintmax_t size = entry.file_size(error);
    if (error) {
        logger_.warning("Could not read file size: " + entry.path().string());
        return false;
    }

    const fs::file_time_type write_time = entry.last_write_time(error);
    if (error) {
        logger_.warning("Could not read file time: " + entry.path().string());
        return false;
    }

    info.path = entry.path();
    info.relative_path = entry.path().lexically_relative(root);
    info.size = size;
    info.last_modified =
        static_cast<std::int64_t>(write_time.time_since_epoch().count());
    return true;
}

bool FileTreeWalker::readSortedChildren(
    const fs::path& directory,
    std::vector<fs::directory_entry>& children) const
{
    children.clear();
    std::error_code error;
    fs::directory_iterator it(
        directory, fs::directory_options::skip_permission_denied, error);
    if (error) {
        logger_.warning("Open directory failed for " + directory.string());
        return false;
    }

    for (const fs::directory_iterator end; it != end; it.increment(error)) {
        if (error) {
            error.clear();
            continue;
        }
        children.push_back(*it);
    }

    std::ranges::sort(children, [](const auto& a, const auto& b) {
        return a.path().filename().generic_string()
             < b.path().filename().generic_string();
    });
    return true;
}

bool FileTreeWalker::shouldSkip(const fs::path& path) const
{
    std::error_code error;
    const fs::file_status status = fs::symlink_status(path, error);
    if (error) {
        return true;
    }
    if (status.type() == fs::file_type::symlink) {
        logger_.warning("Skipping symbolic link: " + path.string());
        return true;
    }

    // Exact exclude only — parents already passed DFS exclusion.
    return exclude_.contains(path);
}

#include "FileTreeWalker/FileTreeWalker.h"
#include <algorithm>
#include <system_error>

namespace fs = std::filesystem;

FileTreeWalker::FileTreeWalker(Logger& logger, ExcludeCallback is_excluded)
    : logger_(logger), is_excluded_(std::move(is_excluded)) {}

bool FileTreeWalker::walk(const fs::path& root,
                          const fs::path& resume_from,
                          const FileCallback& on_file) const
{
    std::error_code error;
    if (!fs::exists(root, error) || error) {
        logger_.warning("Access failed for " + root.string());
        return false;
    }
    if (shouldSkip(root)) {
        return true;
    }
    if (fs::is_regular_file(root, error)) {
        return !error && on_file(root);
    }
    if (!fs::is_directory(root, error) || error) {
        logger_.warning("Open directory failed for " + root.string());
        return false;
    }

    std::vector<std::string> parts;
    for (const fs::path& part : resume_from) {
        parts.push_back(part.generic_string());
    }
    return walkDirectory(root, parts, 0, on_file);
}

bool FileTreeWalker::walkDirectory(const fs::path& directory,
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
                !walkDirectory(path, resume_parts, depth + 1, on_file)) {
                return false;
            }
            continue;
        }

        if (entry.is_directory(error)) {
            if (!error && !walkDirectory(path, {}, 0, on_file)) {
                return false;
            }
        } else if (entry.is_regular_file(error) && !error) {
            if (!on_file(path)) {
                return false;
            }
        }
    }
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
    return static_cast<bool>(is_excluded_) && is_excluded_(path);
}

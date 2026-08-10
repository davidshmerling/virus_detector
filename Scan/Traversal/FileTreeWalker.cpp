#include "Scan/Traversal/FileTreeWalker.h"

#include <algorithm>
#include <cstddef>
#include <system_error>

namespace fs = std::filesystem;

FileTreeWalker::FileTreeWalker(Logger& logger, const ExcludeSet& exclude)
    : logger_(logger),
      path_filter_(exclude),
      scan_root_validator_(exclude),
      file_info_builder_(logger)
{
}

bool FileTreeWalker::walk(
    const fs::path& root,
    const fs::path& resume_from,
    const FileCallback& file_handler) const
{
    std::error_code error;

    if (!fs::exists(root, error) || error) {
        logger_.warning("Access failed for " + root.string());
        return false;
    }

    // One-time ancestry check for the requested root only. After this, the DFS
    // uses per-node shouldSkip(); excluded directories are never entered.
    if (scan_root_validator_.isRootInsideExcludedPath(root)) {
        logger_.warning("Scan root is excluded: " + root.string());
        return true;
    }

    if (fs::is_regular_file(root, error)) {
        FileInfo info;
        return file_info_builder_.build(root, fs::directory_entry(root), info)
            ? file_handler(info)
            : true;
    }

    if (!fs::is_directory(root, error) || error) {
        logger_.warning("Open directory failed for " + root.string());
        return false;
    }

    const ResumePathFilter resume_filter(resume_from);
    return walkDirectory(root, root, resume_filter, 0, file_handler);
}

bool FileTreeWalker::walkDirectory(
    const fs::path& root,
    const fs::path& directory,
    const ResumePathFilter& resume_filter,
    std::size_t depth,
    const FileCallback& file_handler) const
{
    if (path_filter_.shouldSkip(directory)) {
        return true;
    }

    std::vector<fs::directory_entry> children;
    if (!readSortedChildren(directory, children)) {
        return true;
    }

    for (const fs::directory_entry& entry : children) {
        if (path_filter_.shouldSkip(entry.path())) {
            continue;
        }

        const ResumeDecision decision = resume_filter.decide(
            entry.path().filename().generic_string(), depth);

        if (decision == ResumeDecision::SkipEntry) {
            continue;
        }

        std::error_code error;

        if (entry.is_directory(error)) {
            if (error) {
                continue;
            }

            if (decision == ResumeDecision::ContinueTowardResumePoint) {
                if (!walkDirectory(
                        root,
                        entry.path(),
                        resume_filter,
                        depth + 1,
                        file_handler)) {
                    return false;
                }
            } else {
                // Fresh subtree: no resume checkpoint applies from here down,
                // so switch to an inactive filter for the recursive walk.
                const ResumePathFilter no_resume;
                if (!walkDirectory(
                        root, entry.path(), no_resume, 0, file_handler)) {
                    return false;
                }
            }

            continue;
        }

        if (!entry.is_regular_file(error) || error) {
            continue;
        }

        FileInfo info;
        if (!file_info_builder_.build(root, entry, info)) {
            continue;
        }

        if (!file_handler(info)) {
            return false;
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

#include "Scan/Traversal/FileTreeWalker.h"

#include <algorithm>
#include <cstddef>
#include <system_error>

namespace fs = std::filesystem;

FileTreeWalker::FileTreeWalker(Logger& logger, const ExcludeSet& exclude)
    : logger_(logger),
      path_filter_(exclude),
      root_guard_(exclude),
      file_info_builder_(logger)
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
    // After this, the DFS uses per-node shouldSkip() — excluded dirs are never
    // entered.
    if (root_guard_.isRootInsideExcludedPath(root)) {
        logger_.warning("Scan root is excluded: " + root.string());
        return true;
    }

    if (fs::is_regular_file(root, error)) {
        if (error) {
            return false;
        }
        FileInfo info;
        if (!file_info_builder_.build(root, fs::directory_entry(root), info)) {
            return true;
        }
        return on_file(info);
    }
    if (!fs::is_directory(root, error) || error) {
        logger_.warning("Open directory failed for " + root.string());
        return false;
    }

    const ResumePathFilter resume_filter(resume_from);
    return walkDirectory(root, root, resume_filter, 0, on_file);
}

bool FileTreeWalker::walkDirectory(
    const fs::path& root,
    const fs::path& directory,
    const ResumePathFilter& resume_filter,
    std::size_t depth,
    const FileCallback& on_file) const
{
    if (path_filter_.shouldSkip(directory)) {
        return true;
    }

    std::vector<fs::directory_entry> children;
    if (!readSortedChildren(directory, children)) {
        return true;
    }

    for (const fs::directory_entry& entry : children) {
        if (!visitEntry(root, entry, resume_filter, depth, on_file)) {
            return false;
        }
    }

    return true;
}

bool FileTreeWalker::visitEntry(
    const fs::path& root,
    const fs::directory_entry& entry,
    const ResumePathFilter& resume_filter,
    std::size_t depth,
    const FileCallback& on_file) const
{
    const fs::path path = entry.path();

    if (path_filter_.shouldSkip(path)) {
        return true;
    }

    const ResumeDecision decision =
        resume_filter.decide(path.filename().generic_string(), depth);

    switch (decision) {
        case ResumeDecision::Skip:
            return true;
        case ResumeDecision::FollowPath:
            return followResumePath(root, entry, resume_filter, depth, on_file);
        case ResumeDecision::ScanNormally:
            return scanEntry(root, entry, on_file);
    }

    return true;
}

bool FileTreeWalker::followResumePath(
    const fs::path& root,
    const fs::directory_entry& entry,
    const ResumePathFilter& resume_filter,
    std::size_t depth,
    const FileCallback& on_file) const
{
    std::error_code error;

    if (!entry.is_directory(error) || error) {
        return true;
    }

    return walkDirectory(root, entry.path(), resume_filter, depth + 1, on_file);
}

bool FileTreeWalker::scanEntry(
    const fs::path& root,
    const fs::directory_entry& entry,
    const FileCallback& on_file) const
{
    std::error_code error;

    if (entry.is_directory(error)) {
        if (error) {
            return true;
        }
        // A fresh subtree: no resume checkpoint applies from here down.
        const ResumePathFilter no_resume;
        return walkDirectory(root, entry.path(), no_resume, 0, on_file);
    }

    if (!entry.is_regular_file(error) || error) {
        return true;
    }

    FileInfo info;
    if (!file_info_builder_.build(root, entry, info)) {
        return true;
    }

    return on_file(info);
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

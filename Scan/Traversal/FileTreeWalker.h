#pragma once

#include "Exclude/ExcludeSet.h"
#include "Exclude/PathFilter.h"
#include "Exclude/ScanRootGuard.h"
#include "Logger/Logger.h"
#include "Resume/ResumePathFilter.h"
#include "Scan/Traversal/FileInfo.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

// Sorted DFS: exclude (exact), resume, hand each regular file to on_file.
// For every regular file it collects size and last-modified time from the
// directory_entry's already-cached stat, so the worker never has to touch the
// filesystem again just to read that metadata.
class FileTreeWalker {
public:
    using FileCallback = std::function<bool(const FileInfo&)>;

    FileTreeWalker(Logger& logger, const ExcludeSet& exclude);

    // Empty resume_from → full walk from root.
    // Checks the root once via ScanRootGuard, then the DFS uses PathFilter per
    // node.
    bool walk(const std::filesystem::path& root,
              const std::filesystem::path& resume_from,
              const FileCallback& on_file) const;

private:
    bool walkDirectory(const std::filesystem::path& root,
                       const std::filesystem::path& directory,
                       const ResumePathFilter& resume_filter,
                       std::size_t depth,
                       const FileCallback& on_file) const;

    // One entry: skip it, follow it deeper along the resume path, or scan it
    // normally — whichever the resume filter decides. Returns false only when
    // on_file asks to stop the whole walk.
    bool visitEntry(const std::filesystem::path& root,
                    const std::filesystem::directory_entry& entry,
                    const ResumePathFilter& resume_filter,
                    std::size_t depth,
                    const FileCallback& on_file) const;

    // FollowPath: descend into the directory on the resume path, continuing to
    // match the checkpoint one level deeper.
    bool followResumePath(const std::filesystem::path& root,
                          const std::filesystem::directory_entry& entry,
                          const ResumePathFilter& resume_filter,
                          std::size_t depth,
                          const FileCallback& on_file) const;

    // ScanNormally: recurse into a directory as a fresh subtree (no resume), or
    // hand a regular file to on_file.
    bool scanEntry(const std::filesystem::path& root,
                   const std::filesystem::directory_entry& entry,
                   const FileCallback& on_file) const;

    bool readSortedChildren(
        const std::filesystem::path& directory,
        std::vector<std::filesystem::directory_entry>& children) const;

    Logger& logger_;
    PathFilter path_filter_;
    ScanRootGuard root_guard_;
    FileInfoBuilder file_info_builder_;
};

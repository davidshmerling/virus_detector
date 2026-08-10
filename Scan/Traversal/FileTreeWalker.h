#pragma once

#include "Exclude/ExcludeSet.h"
#include "Exclude/PathFilter.h"
#include "Exclude/ScanRootValidator.h"
#include "Logger/Logger.h"
#include "Resume/ResumePathFilter.h"
#include "Scan/Traversal/FileInfo.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

// Walks a directory tree with a sorted DFS. Applies exclude rules (exact match)
// and resume decisions, then hands each regular file to `file_handler`.
// For every regular file it collects size and last-modified time from the
// directory_entry's already-cached stat, so the worker never has to touch the
// filesystem again just to read that metadata.
class FileTreeWalker {
public:
    using FileCallback = std::function<bool(const FileInfo&)>;

    FileTreeWalker(Logger& logger, const ExcludeSet& exclude);

    // Walks from `root`. An empty `resume_from` means a full walk; otherwise
    // the DFS resumes from that checkpoint. Validates the root once via
    // ScanRootValidator, then uses PathFilter per node. Returns false if the
    // walk must abort (for example, an inaccessible root).
    bool walk(const std::filesystem::path& root,
              const std::filesystem::path& resume_from,
              const FileCallback& file_handler) const;

private:
    // Processes one DFS level: skip, continue toward the resume point, or
    // traverse normally, then recurse into directories or hand regular files
    // to `file_handler`.
    bool walkDirectory(const std::filesystem::path& root,
                       const std::filesystem::path& directory,
                       const ResumePathFilter& resume_filter,
                       std::size_t depth,
                       const FileCallback& file_handler) const;

    // Fills `children` with the directory's entries sorted by filename.
    // Returns false if the directory cannot be opened.
    bool readSortedChildren(
        const std::filesystem::path& directory,
        std::vector<std::filesystem::directory_entry>& children) const;

    Logger& logger_;
    PathFilter path_filter_;
    ScanRootValidator scan_root_validator_;
    FileInfoBuilder file_info_builder_;
};

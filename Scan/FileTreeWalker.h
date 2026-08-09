#pragma once

#include "Exclude/ExcludeManager.h"
#include "Logger/Logger.h"
#include "Scan/FileInfo.h"

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

    FileTreeWalker(Logger& logger, const ExcludeManager& exclude);

    // Empty resume_from → full walk from root.
    // Calls isScanRootExcluded(root) once, then DFS uses contains() only.
    bool walk(const std::filesystem::path& root,
              const std::filesystem::path& resume_from,
              const FileCallback& on_file) const;

private:
    bool walkDirectory(const std::filesystem::path& root,
                       const std::filesystem::path& directory,
                       const std::vector<std::string>& resume_parts,
                       std::size_t depth,
                       const FileCallback& on_file) const;
    bool readSortedChildren(
        const std::filesystem::path& directory,
        std::vector<std::filesystem::directory_entry>& children) const;
    bool shouldSkip(const std::filesystem::path& path) const;

    // Builds a FileInfo from an entry already visited by the DFS, reusing the
    // stat the directory_entry cached. Returns false if the metadata cannot be
    // read (e.g. the file vanished mid-scan), in which case it is skipped.
    bool makeFileInfo(const std::filesystem::path& root,
                      const std::filesystem::directory_entry& entry,
                      FileInfo& info) const;

    Logger& logger_;
    const ExcludeManager& exclude_;
};

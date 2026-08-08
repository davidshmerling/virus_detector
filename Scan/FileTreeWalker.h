#pragma once

#include "Exclude/ExcludeManager.h"
#include "Logger/Logger.h"

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

// Sorted DFS: exclude (exact), resume, hand each regular file to on_file.
class FileTreeWalker {
public:
    using FileCallback = std::function<bool(const std::filesystem::path&)>;

    FileTreeWalker(Logger& logger, const ExcludeManager& exclude);

    // Empty resume_from → full walk from root.
    // Calls isScanRootExcluded(root) once, then DFS uses contains() only.
    bool walk(const std::filesystem::path& root,
              const std::filesystem::path& resume_from,
              const FileCallback& on_file) const;

private:
    bool walkDirectory(const std::filesystem::path& directory,
                       const std::vector<std::string>& resume_parts,
                       std::size_t depth,
                       const FileCallback& on_file) const;
    bool readSortedChildren(
        const std::filesystem::path& directory,
        std::vector<std::filesystem::directory_entry>& children) const;
    bool shouldSkip(const std::filesystem::path& path) const;

    Logger& logger_;
    const ExcludeManager& exclude_;
};

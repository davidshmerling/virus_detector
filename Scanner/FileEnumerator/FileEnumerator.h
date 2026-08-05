#pragma once

#include "Exclude/ExcludeManager.h"
#include "Logger/Logger.h"
#include "Scanner/ScanSummary.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class FileEnumerator {
public:
    using FileCallback =
        std::function<bool(const std::filesystem::path&)>;

    FileEnumerator(
        const ExcludeManager& exclude_manager,
        Logger& logger);

    bool enumerateSorted(
        const fs::path& root,
        ScanSummary& summary,
        const FileCallback& on_file) const;

    bool resumeFromSorted(
        const fs::path& root,
        const fs::path& first_unfinished_path,
        ScanSummary& summary,
        const FileCallback& on_file) const;

private:
    bool walkAll(
        const fs::path& directory,
        ScanSummary& summary,
        const FileCallback& on_file) const;

    bool walkResume(
        const fs::path& directory,
        const std::vector<std::string>& resume_parts,
        std::size_t depth,
        ScanSummary& summary,
        const FileCallback& on_file) const;

    std::vector<fs::directory_entry> getSortedChildren(
        const fs::path& directory) const;

    void logEntryAccessError(
        const fs::path& path,
        const char* action,
        const std::error_code& error) const;

    // Does not follow the link. Returns true if traversal should skip this path.
    bool skipSymbolicLink(
        const fs::path& path,
        ScanSummary& summary) const;

    const ExcludeManager& exclude_manager_;
    Logger& logger_;
};

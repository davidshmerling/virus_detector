#pragma once

#include <filesystem>
#include <string>

// Performs the physical file work of quarantine and nothing else:
// moving a file into the quarantine folder, moving it back out, and deleting
// it. It holds no metadata and makes no policy decisions.
class QuarantineFileOperations {
public:
    explicit QuarantineFileOperations(std::filesystem::path files_directory);

    // Create the folder that holds quarantined file contents.
    bool prepareDirectory() const;

    // Reserve a unique "<id><ext>" path inside the quarantine folder and
    // report the chosen id.
    std::filesystem::path reserveDestination(
        const std::filesystem::path& original,
        std::string& out_id) const;

    // Move a file into quarantine (source -> destination).
    bool moveIn(
        const std::filesystem::path& source,
        const std::filesystem::path& destination) const;

    // Move a quarantined file back to its original location, creating the
    // parent directory if needed.
    bool moveOut(
        const std::filesystem::path& quarantine_path,
        const std::filesystem::path& original_path) const;

    // Delete a quarantined file for good.
    bool remove(const std::filesystem::path& quarantine_path) const;

private:
    // rename first; fall back to copy + delete across filesystems.
    static bool moveFile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination);

    std::string generateId() const;

    std::filesystem::path files_directory_;
};

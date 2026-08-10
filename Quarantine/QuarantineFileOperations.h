#pragma once

#include <filesystem>
#include <string>

// Performs the physical file work of quarantine and nothing else: moving a
// file into the quarantine folder, moving it back out, and deleting it. It
// holds no metadata and makes no policy decisions.
class QuarantineFileOperations {
public:
    explicit QuarantineFileOperations(std::filesystem::path files_directory);

    // Creates the folder that holds quarantined file contents.
    bool prepareDirectory() const;

    // Reserves a unique "<id><ext>" path inside the quarantine folder and
    // reports the chosen id via `out_id`.
    std::filesystem::path reserveDestination(
        const std::filesystem::path& original,
        std::string& out_id) const;

    // Moves a file into quarantine (`source` → `destination`).
    bool moveIn(
        const std::filesystem::path& source,
        const std::filesystem::path& destination) const;

    // Moves a quarantined file back to its original location, creating the
    // parent directory if needed.
    bool moveOut(
        const std::filesystem::path& quarantine_path,
        const std::filesystem::path& original_path) const;

    // Deletes a quarantined file permanently.
    bool remove(const std::filesystem::path& quarantine_path) const;

private:
    // Tries rename first; falls back to copy + delete across filesystems.
    static bool moveFile(
        const std::filesystem::path& source,
        const std::filesystem::path& destination);

    // Generates a unique id string for a new quarantine destination.
    std::string generateId() const;

    std::filesystem::path files_directory_;
};

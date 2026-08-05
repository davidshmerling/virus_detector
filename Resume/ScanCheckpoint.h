#pragma once

#include <filesystem>
#include <string>

struct ScanCheckpoint {
    int version = 2;

    std::filesystem::path root;

    // Relative path of the first task that may not have completed.
    std::filesystem::path next_unfinished_path;

    std::string status = "running";
};

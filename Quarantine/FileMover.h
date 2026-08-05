#pragma once

#include <filesystem>

class FileMover {
public:
    bool move(
        const std::filesystem::path& source,
        const std::filesystem::path& destination) const;
};

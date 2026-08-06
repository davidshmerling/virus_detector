#pragma once

#include "Resume/ScanCheckpoint.h"

#include <filesystem>

class JsonCheckpointRepository {
public:
    explicit JsonCheckpointRepository(std::filesystem::path checkpoint_file);

    bool initialize();
    bool exists() const;

    bool load(ScanCheckpoint& checkpoint) const;
    bool save(const ScanCheckpoint& checkpoint) const;

private:
    std::filesystem::path checkpoint_file_;
};

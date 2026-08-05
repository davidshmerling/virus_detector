#pragma once

#include "Cache/CacheRepository.h"

#include <filesystem>

class JsonCacheRepository : public CacheRepository {
public:
    explicit JsonCacheRepository(std::filesystem::path cache_file);

    bool initialize() override;
    bool load(CacheMap& entries) const override;
    bool save(const CacheMap& entries) const override;

private:
    std::filesystem::path cache_file_;
};

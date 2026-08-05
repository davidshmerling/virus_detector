#pragma once

#include "Cache/CacheEntry.h"

#include <string>
#include <unordered_map>

using CacheMap = std::unordered_map<std::string, CacheEntry>;

class CacheRepository {
public:
    virtual ~CacheRepository() = default;

    virtual bool initialize() = 0;
    virtual bool load(CacheMap& entries) const = 0;
    virtual bool save(const CacheMap& entries) const = 0;
};

#pragma once

#include <atomic>
#include <cstddef>
#include <string>

struct ScanSummary {
    std::atomic<std::size_t> discovered{0};
    std::atomic<std::size_t> scanned{0};
    std::atomic<std::size_t> cached{0};
    std::atomic<std::size_t> excluded{0};
    std::atomic<std::size_t> malicious{0};
    std::atomic<std::size_t> quarantined{0};
    std::atomic<std::size_t> failed{0};

    ScanSummary() = default;

    ScanSummary(const ScanSummary&) = delete;
    ScanSummary& operator=(const ScanSummary&) = delete;

    ScanSummary(ScanSummary&& other) noexcept
        : discovered(other.discovered.load()),
          scanned(other.scanned.load()),
          cached(other.cached.load()),
          excluded(other.excluded.load()),
          malicious(other.malicious.load()),
          quarantined(other.quarantined.load()),
          failed(other.failed.load())
    {
    }

    ScanSummary& operator=(ScanSummary&& other) noexcept
    {
        discovered.store(other.discovered.load());
        scanned.store(other.scanned.load());
        cached.store(other.cached.load());
        excluded.store(other.excluded.load());
        malicious.store(other.malicious.load());
        quarantined.store(other.quarantined.load());
        failed.store(other.failed.load());
        return *this;
    }

    std::string toLogLine() const
    {
        return "Scan summary: discovered=" +
               std::to_string(discovered.load()) +
               ", scanned=" + std::to_string(scanned.load()) +
               ", cached=" + std::to_string(cached.load()) +
               ", excluded=" + std::to_string(excluded.load()) +
               ", malicious=" + std::to_string(malicious.load()) +
               ", quarantined=" + std::to_string(quarantined.load()) +
               ", failed=" + std::to_string(failed.load());
    }
};

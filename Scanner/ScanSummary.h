#pragma once

#include <atomic>
#include <cstddef>
#include <format>
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

    [[nodiscard]] std::string toLogLine() const
    {
        return std::format(
            "Scan summary: discovered={}, scanned={}, cached={}, "
            "excluded={}, malicious={}, quarantined={}, failed={}",
            discovered.load(),
            scanned.load(),
            cached.load(),
            excluded.load(),
            malicious.load(),
            quarantined.load(),
            failed.load());
    }
};

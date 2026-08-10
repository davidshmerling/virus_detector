#pragma once

#include <atomic>
#include <cstddef>
#include <format>
#include <string>

// Thread-safe counters for one scan run. Move-only because atomics are not
// copyable; toLogLine() formats a single summary line for the log.
struct ScanSummary {
    std::atomic<std::size_t> discovered{0};   // Files handed off by the walker.
    std::atomic<std::size_t> scanned{0};      // Cache misses that were read.
    std::atomic<std::size_t> cached{0};       // Cache hits (not re-read).
    std::atomic<std::size_t> excluded{0};     // Reserved; not incremented yet.
    std::atomic<std::size_t> malicious{0};    // Files judged malicious.
    std::atomic<std::size_t> quarantined{0};  // Successfully quarantined.
    std::atomic<std::size_t> failed{0};       // Open/read failures (Error).

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

    // Returns a single log line with all counters.
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

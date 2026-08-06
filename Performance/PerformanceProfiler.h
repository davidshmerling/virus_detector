#pragma once

#include "Logger/Logger.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

enum class PerformanceSection {
    TotalScan,
    Enumeration,
    SubmitTask,
    DirectoryRead,
    DirectorySort,
    RelativePath,
    ProgressRegister,
    QueueWait,
    CacheLookupLockWait,
    CacheLookupWork,
    CacheUpdateLockWait,
    CacheUpdateWork,
    CachePersistence,
    FileOpen,
    FileRead,
    AutomatonSearch,
    // Search-time phases inside AhoCorasick::scanChunk (TSC cycles).
    // Failure links are compiled into next[] at build time — not a search phase.
    AutomatonTransition,
    AutomatonOutputCheck,
    AutomatonMatchHandle,
    FileProcessing,
    CheckpointSave,
    Quarantine
};

// Current thread CPU time (Linux: CLOCK_THREAD_CPUTIME_ID).
// Returns 0 when unavailable.
[[nodiscard]] std::chrono::nanoseconds threadCpuTime() noexcept;

class PerformanceProfiler {
public:
    explicit PerformanceProfiler(Logger& logger);

    void addMeasurement(
        PerformanceSection section,
        std::chrono::nanoseconds wall_duration,
        std::chrono::nanoseconds cpu_duration = {});

    // CPU-bound micro-phases (e.g. automaton inner loop via TSC).
    void addCycleMeasurement(
        PerformanceSection section,
        std::uint64_t cycles,
        std::size_t operation_count = 1);

    void logReport() const;

private:
    struct Measurement {
        std::chrono::nanoseconds total_wall{0};
        std::chrono::nanoseconds total_cpu{0};
        std::uint64_t total_cycles = 0;
        std::size_t call_count = 0;
        std::size_t operation_count = 0;
    };

    static std::string sectionName(
        PerformanceSection section);

    Logger& logger_;

    mutable std::mutex mutex_;

    std::unordered_map<
        PerformanceSection,
        Measurement> measurements_;
};

class ScopedPerformanceTimer {
public:
    ScopedPerformanceTimer(
        PerformanceProfiler& profiler,
        PerformanceSection section);

    ~ScopedPerformanceTimer();

    ScopedPerformanceTimer(
        const ScopedPerformanceTimer&) = delete;

    ScopedPerformanceTimer& operator=(
        const ScopedPerformanceTimer&) = delete;

private:
    PerformanceProfiler& profiler_;
    PerformanceSection section_;

    std::chrono::steady_clock::time_point start_wall_;
    std::chrono::nanoseconds start_cpu_{0};
};

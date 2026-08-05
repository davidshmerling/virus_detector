#pragma once

#include "Logger/Logger.h"

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

enum class PerformanceSection {
    TotalScan,
    Enumeration,
    SubmitTask,
    CacheValidation,
    FileOpen,
    FileRead,
    AutomatonSearch,
    FileProcessing,
    CheckpointSave,
    Quarantine,
    CacheFlush
};

class PerformanceProfiler {
public:
    explicit PerformanceProfiler(Logger& logger);

    void addMeasurement(
        PerformanceSection section,
        std::chrono::nanoseconds duration);

    void logReport() const;

private:
    struct Measurement {
        std::chrono::nanoseconds total_time{0};
        std::size_t call_count = 0;
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

    std::chrono::steady_clock::time_point start_time_;
};

#pragma once

#include "Logger/Logger.h"

#include <chrono>
#include <string>

// Measures the wall-clock duration of a scope — typically the whole program
// run. Construction starts the clock; stop() (or destruction, if stop() was
// never called) computes the elapsed time, writes it to the log, and prints it
// to the console. stop() is idempotent, so the RAII path never double-reports.
class PerformanceMonitor {
public:
    // Starts timing immediately. Does not take ownership of `logger`.
    explicit PerformanceMonitor(
        Logger& logger,
        std::string label = "Total time");
    ~PerformanceMonitor();

    PerformanceMonitor(const PerformanceMonitor&) = delete;
    PerformanceMonitor& operator=(const PerformanceMonitor&) = delete;

    // Computes elapsed time, logs it, and prints it. Safe to call once; later
    // calls (including the destructor) are no-ops.
    void stop();

private:
    // Formats `elapsed` as a human-readable duration string.
    static std::string formatDuration(std::chrono::nanoseconds elapsed);

    Logger& logger_;
    std::string label_;
    std::chrono::steady_clock::time_point start_;
    bool stopped_ = false;
};

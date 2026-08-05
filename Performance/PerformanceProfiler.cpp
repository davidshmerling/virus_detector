#include "Performance/PerformanceProfiler.h"

#include <iomanip>
#include <sstream>

PerformanceProfiler::PerformanceProfiler(Logger& logger)
    : logger_(logger)
{
}

void PerformanceProfiler::addMeasurement(
    PerformanceSection section,
    std::chrono::nanoseconds duration)
{
    std::scoped_lock lock(mutex_);

    Measurement& measurement =
        measurements_[section];

    measurement.total_time += duration;
    ++measurement.call_count;
}

void PerformanceProfiler::logReport() const
{
    std::scoped_lock lock(mutex_);

    logger_.info("Performance report:");

    static constexpr PerformanceSection kReportOrder[] = {
        PerformanceSection::TotalScan,
        PerformanceSection::Enumeration,
        PerformanceSection::SubmitTask,
        PerformanceSection::DirectoryRead,
        PerformanceSection::DirectorySort,
        PerformanceSection::RelativePath,
        PerformanceSection::ProgressRegister,
        PerformanceSection::QueueWait,
        PerformanceSection::FileProcessing,
        PerformanceSection::CacheValidation,
        PerformanceSection::CacheUpdate,
        PerformanceSection::CacheJsonSave,
        PerformanceSection::FileOpen,
        PerformanceSection::FileRead,
        PerformanceSection::AutomatonSearch,
        PerformanceSection::CheckpointSave,
        PerformanceSection::Quarantine
    };

    for (const PerformanceSection section : kReportOrder) {
        const auto it = measurements_.find(section);
        if (it == measurements_.end()) {
            continue;
        }

        const Measurement& measurement = it->second;

        const double total_seconds =
            std::chrono::duration<double>(
                measurement.total_time).count();

        const double average_milliseconds =
            measurement.call_count == 0
                ? 0.0
                : std::chrono::duration<double, std::milli>(
                      measurement.total_time).count() /
                      static_cast<double>(
                          measurement.call_count);

        std::ostringstream message;

        message
            << "  "
            << sectionName(section)
            << ": total="
            << std::fixed
            << std::setprecision(6)
            << total_seconds
            << " seconds, calls="
            << measurement.call_count
            << ", average="
            << average_milliseconds
            << " ms";

        logger_.info(message.str());
    }

    logger_.info(
        "Note: parallel section totals may overlap and "
        "can exceed wall-clock time.");
}

std::string PerformanceProfiler::sectionName(
    PerformanceSection section)
{
    switch (section) {
        case PerformanceSection::TotalScan:
            return "Total scan";

        case PerformanceSection::Enumeration:
            return "Enumeration";

        case PerformanceSection::SubmitTask:
            return "Submit task";

        case PerformanceSection::DirectoryRead:
            return "Directory read";

        case PerformanceSection::DirectorySort:
            return "Directory sort";

        case PerformanceSection::RelativePath:
            return "Relative path";

        case PerformanceSection::ProgressRegister:
            return "Progress register";

        case PerformanceSection::QueueWait:
            return "Queue wait";

        case PerformanceSection::CacheValidation:
            return "Cache lookup";

        case PerformanceSection::CacheUpdate:
            return "Cache update";

        case PerformanceSection::CacheJsonSave:
            return "Cache save";

        case PerformanceSection::FileOpen:
            return "File open";

        case PerformanceSection::FileRead:
            return "File read";

        case PerformanceSection::AutomatonSearch:
            return "Automaton search";

        case PerformanceSection::FileProcessing:
            return "File processing";

        case PerformanceSection::CheckpointSave:
            return "Checkpoint save";

        case PerformanceSection::Quarantine:
            return "Quarantine";
    }

    return "Unknown";
}

ScopedPerformanceTimer::ScopedPerformanceTimer(
    PerformanceProfiler& profiler,
    PerformanceSection section)
    : profiler_(profiler),
      section_(section),
      start_time_(std::chrono::steady_clock::now())
{
}

ScopedPerformanceTimer::~ScopedPerformanceTimer()
{
    const auto end_time =
        std::chrono::steady_clock::now();

    profiler_.addMeasurement(
        section_,
        end_time - start_time_);
}

#include "Performance/PerformanceProfiler.h"

#include <iomanip>
#include <sstream>

#if defined(__linux__)
#include <ctime>
#endif

std::chrono::nanoseconds threadCpuTime() noexcept
{
#if defined(__linux__)
    timespec ts{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) {
        return {};
    }

    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::seconds(ts.tv_sec) +
        std::chrono::nanoseconds(ts.tv_nsec));
#else
    return {};
#endif
}

PerformanceProfiler::PerformanceProfiler(Logger& logger)
    : logger_(logger)
{
}

void PerformanceProfiler::addMeasurement(
    PerformanceSection section,
    std::chrono::nanoseconds wall_duration,
    std::chrono::nanoseconds cpu_duration)
{
    std::scoped_lock lock(mutex_);

    Measurement& measurement =
        measurements_[section];

    measurement.total_wall += wall_duration;
    measurement.total_cpu += cpu_duration;
    ++measurement.call_count;
}

void PerformanceProfiler::addCycleMeasurement(
    PerformanceSection section,
    std::uint64_t cycles,
    std::size_t operation_count)
{
    if (cycles == 0 && operation_count == 0) {
        return;
    }

    std::scoped_lock lock(mutex_);

    Measurement& measurement =
        measurements_[section];

    measurement.total_cycles += cycles;
    measurement.operation_count += operation_count;
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
        PerformanceSection::CacheLookupLockWait,
        PerformanceSection::CacheLookupWork,
        PerformanceSection::CacheUpdateLockWait,
        PerformanceSection::CacheUpdateWork,
        PerformanceSection::CachePersistence,
        PerformanceSection::FileOpen,
        PerformanceSection::FileRead,
        PerformanceSection::AutomatonSearch,
        PerformanceSection::AutomatonTransition,
        PerformanceSection::AutomatonOutputCheck,
        PerformanceSection::AutomatonMatchHandle,
        PerformanceSection::CheckpointSave,
        PerformanceSection::Quarantine
    };

    std::uint64_t automaton_phase_cycles = 0;
    for (const PerformanceSection section : {
             PerformanceSection::AutomatonTransition,
             PerformanceSection::AutomatonOutputCheck,
             PerformanceSection::AutomatonMatchHandle}) {
        const auto it = measurements_.find(section);
        if (it != measurements_.end()) {
            automaton_phase_cycles += it->second.total_cycles;
        }
    }

    for (const PerformanceSection section : kReportOrder) {
        const auto it = measurements_.find(section);
        if (it == measurements_.end()) {
            continue;
        }

        const Measurement& measurement = it->second;

        std::ostringstream message;
        message << "  " << sectionName(section) << ": ";

        if (measurement.total_cycles > 0) {
            const double share =
                automaton_phase_cycles == 0
                    ? 0.0
                    : (100.0 *
                       static_cast<double>(measurement.total_cycles) /
                       static_cast<double>(automaton_phase_cycles));

            const double avg_cycles =
                measurement.operation_count == 0
                    ? 0.0
                    : static_cast<double>(measurement.total_cycles) /
                          static_cast<double>(
                              measurement.operation_count);

            message
                << std::fixed
                << std::setprecision(2)
                << "cycles="
                << measurement.total_cycles
                << " ("
                << share
                << "% of automaton phases), ops="
                << measurement.operation_count
                << ", avg_cycles/op="
                << avg_cycles
                << ", samples="
                << measurement.call_count;
        } else {
            const double wall_seconds =
                std::chrono::duration<double>(
                    measurement.total_wall).count();

            const double cpu_seconds =
                std::chrono::duration<double>(
                    measurement.total_cpu).count();

            const double average_wall_ms =
                measurement.call_count == 0
                    ? 0.0
                    : std::chrono::duration<double, std::milli>(
                          measurement.total_wall).count() /
                          static_cast<double>(
                              measurement.call_count);

            const double average_cpu_ms =
                measurement.call_count == 0
                    ? 0.0
                    : std::chrono::duration<double, std::milli>(
                          measurement.total_cpu).count() /
                          static_cast<double>(
                              measurement.call_count);

            message
                << std::fixed
                << std::setprecision(6)
                << "wall="
                << wall_seconds
                << "s, cpu="
                << cpu_seconds
                << "s, calls="
                << measurement.call_count
                << ", avg_wall="
                << average_wall_ms
                << "ms, avg_cpu="
                << average_cpu_ms
                << "ms";
        }

        logger_.info(message.str());
    }

    logger_.info(
        "Note: wall = steady_clock; cpu = thread CPU "
        "(CLOCK_THREAD_CPUTIME_ID). Automaton phases use TSC "
        "cycles (relative). Failure links are baked into next[] "
        "at build time. Parallel totals may overlap.");
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

        case PerformanceSection::CacheLookupLockWait:
            return "Cache lookup lock wait";

        case PerformanceSection::CacheLookupWork:
            return "Cache lookup work";

        case PerformanceSection::CacheUpdateLockWait:
            return "Cache update lock wait";

        case PerformanceSection::CacheUpdateWork:
            return "Cache update work";

        case PerformanceSection::CachePersistence:
            return "Cache persistence";

        case PerformanceSection::FileOpen:
            return "File open";

        case PerformanceSection::FileRead:
            return "File read";

        case PerformanceSection::AutomatonSearch:
            return "Automaton search";

        case PerformanceSection::AutomatonTransition:
            return "Automaton transition (next[])";

        case PerformanceSection::AutomatonOutputCheck:
            return "Automaton output check";

        case PerformanceSection::AutomatonMatchHandle:
            return "Automaton match handle";

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
      start_wall_(std::chrono::steady_clock::now()),
      start_cpu_(threadCpuTime())
{
}

ScopedPerformanceTimer::~ScopedPerformanceTimer()
{
    const auto end_wall =
        std::chrono::steady_clock::now();
    const auto end_cpu = threadCpuTime();

    profiler_.addMeasurement(
        section_,
        end_wall - start_wall_,
        end_cpu - start_cpu_);
}

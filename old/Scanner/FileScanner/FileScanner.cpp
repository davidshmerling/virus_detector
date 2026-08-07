#include "Scanner/FileScanner/FileScanner.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <fstream>
#include <span>
#include <system_error>
#include <unordered_set>

namespace fs = std::filesystem;

FileScanner::FileScanner(
    const AhoCorasick& automaton,
    PerformanceProfiler& profiler,
    std::size_t chunk_size)
    : automaton_(automaton),
      profiler_(profiler),
      chunk_size_(chunk_size == 0 ? 256 * 1024 : chunk_size),
      buffer_(chunk_size_)
{
}

FileScanResult FileScanner::scan(const fs::path& file_path)
{
    FileScanResult result{
        .verdict = FileVerdict::Error,
        .file_path = file_path,
        .matched_signature_indices = {}};

    std::ifstream file;

    {
        ScopedPerformanceTimer timer(
            profiler_,
            PerformanceSection::FileOpen);

        file.open(file_path, std::ios::binary);
    }

    if (!file.is_open()) {
        result.error = {
            .code = ErrorCode::FileOpenFailed,
            .message = "Could not open file: " + file_path.string() +
                       " - " +
                       std::system_category().message(errno)};
        return result;
    }

    // Important: state lives across chunks, so a signature
    // that crosses a chunk boundary is still detected.
    int state = 0;
    std::unordered_set<std::size_t> matched_indices;

    // Accumulate per file to avoid locking the profiler on every chunk.
    std::chrono::nanoseconds read_wall{0};
    std::chrono::nanoseconds read_cpu{0};
    std::chrono::nanoseconds search_wall{0};
    std::chrono::nanoseconds search_cpu{0};
    AutomatonScanBreakdown search_breakdown;

    while (true) {
        const auto read_wall_start =
            std::chrono::steady_clock::now();
        const auto read_cpu_start = threadCpuTime();

        file.read(
            buffer_.data(),
            static_cast<std::streamsize>(buffer_.size()));
        const auto bytes_read = file.gcount();

        read_wall +=
            std::chrono::steady_clock::now() - read_wall_start;
        read_cpu += threadCpuTime() - read_cpu_start;

        if (bytes_read <= 0) {
            break;
        }

        const auto search_wall_start =
            std::chrono::steady_clock::now();
        const auto search_cpu_start = threadCpuTime();

        automaton_.scanChunk(
            std::span<const char>{
                buffer_.data(),
                static_cast<std::size_t>(bytes_read)},
            state,
            matched_indices,
            &search_breakdown);

        search_wall +=
            std::chrono::steady_clock::now() - search_wall_start;
        search_cpu += threadCpuTime() - search_cpu_start;
    }

    profiler_.addMeasurement(
        PerformanceSection::FileRead,
        read_wall,
        read_cpu);
    profiler_.addMeasurement(
        PerformanceSection::AutomatonSearch,
        search_wall,
        search_cpu);

    profiler_.addCycleMeasurement(
        PerformanceSection::AutomatonTransition,
        search_breakdown.transition_cycles,
        search_breakdown.bytes_scanned);
    profiler_.addCycleMeasurement(
        PerformanceSection::AutomatonOutputCheck,
        search_breakdown.output_check_cycles,
        search_breakdown.bytes_scanned);
    profiler_.addCycleMeasurement(
        PerformanceSection::AutomatonMatchHandle,
        search_breakdown.match_handle_cycles,
        search_breakdown.match_inserts == 0
            ? search_breakdown.output_hits
            : search_breakdown.match_inserts);

    if (file.bad()) {
        result.error = {
            .code = ErrorCode::FileReadFailed,
            .message =
                "Read error while scanning file: " + file_path.string()};
        return result;
    }

    if (!matched_indices.empty()) {
        result.matched_signature_indices.assign(
            matched_indices.begin(),
            matched_indices.end());
        std::ranges::sort(result.matched_signature_indices);
        result.verdict = FileVerdict::Malicious;
        return result;
    }

    result.verdict = FileVerdict::Clean;
    return result;
}

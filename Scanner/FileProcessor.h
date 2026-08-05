#pragma once

#include "Cache/CacheManager.h"
#include "Logger/Logger.h"
#include "Performance/PerformanceProfiler.h"
#include "Quarantine/QuarantineManager.h"
#include "Scanner/Automaton/AhoCorasick.h"
#include "Scanner/FileScanner/FileScanner.h"
#include "Scanner/ScanSummary.h"
#include "Scanner/SignatureManager/SignatureManager.h"

#include <cstdint>
#include <filesystem>

class FileProcessor {
public:
    FileProcessor(
        const AhoCorasick& automaton,
        const SignatureManager& signature_manager,
        CacheManager& cache_manager,
        QuarantineManager& quarantine_manager,
        Logger& logger,
        PerformanceProfiler& profiler,
        std::int64_t signatures_last_modified);

    void process(
        const std::filesystem::path& file_path,
        ScanSummary& summary);

private:
    bool checkCache(
        const std::filesystem::path& file_path,
        ScanSummary& summary);

    FileScanResult scanFile(const std::filesystem::path& file_path);

    void handleCleanFile(
        const std::filesystem::path& file_path,
        ScanSummary& summary);

    void handleMaliciousFile(
        const std::filesystem::path& file_path,
        std::size_t matched_signature_index,
        ScanSummary& summary);

    void handleScanError(
        const FileScanResult& result,
        ScanSummary& summary);

    const AhoCorasick& automaton_;
    const SignatureManager& signature_manager_;
    CacheManager& cache_manager_;
    QuarantineManager& quarantine_manager_;
    Logger& logger_;
    PerformanceProfiler& profiler_;
    std::int64_t signatures_last_modified_;
};

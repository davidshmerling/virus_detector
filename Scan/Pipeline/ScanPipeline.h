#pragma once

#include "Scan/Automaton/AutomatonBuilder.h"
#include "Cache/CacheManager.h"
#include "Scan/Pipeline/ScanSummary.h"
#include "Scan/Traversal/FileInfo.h"
#include "Exclude/ExcludeSet.h"
#include "Logger/Logger.h"
#include "Quarantine/QuarantineManager.h"
#include "Resume/ResumeManager.h"
#include "Signature/SignatureLoader.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class FileProcessor;  // used only by reference when discovering / enqueueing
class ThreadPool;     // used only by reference when enqueuing scan tasks

// Wires the whole scan pipeline together for one run:
//   load signatures → build automaton → prepare resume → walk the tree →
//   cache hit / miss per file → wait → commit cache generation.
// It performs none of the heavy lifting itself (no chunk reading, no automaton
// search, no DFS, no SQLite); it only connects the classes that do. Open this
// one file to see what happens from the start of a scan to the end.
class ScanPipeline {
public:
    // Binds `logger` for the run and wires resume/cache/quarantine under
    // runtime/. Does not take ownership of `logger`.
    explicit ScanPipeline(Logger& logger);

    // Runs a full scan starting at `root` (typically "/" or a user path).
    void scan(const std::filesystem::path& root);

private:
    // Handles one file discovered by the walker: builds metadata, checks the
    // cache, then routes to handleCacheHit or handleCacheMiss. Always returns
    // true so the walk continues.
    bool handleDiscoveredFile(
        const FileInfo& info,
        const FileProcessor& processor,
        ThreadPool& pool,
        std::int64_t signatures_last_modified,
        ScanSummary& summary);

    // Reuses a cached verdict without reading the file. Re-stamps the entry
    // with the current generation so it survives the next stale cleanup.
    void handleCacheHit(
        const FileInfo& info,
        FileVerdict verdict,
        FileMetadata metadata,
        ScanSummary& summary);

    // Enqueues a worker that scans the file. Open failure is handled in the
    // task (failed++, no cache); a successful scan goes to handleScanResult.
    void handleCacheMiss(
        const FileInfo& info,
        const FileProcessor& processor,
        ThreadPool& pool,
        FileMetadata metadata,
        ScanSummary& summary);

    // Applies a scan result only: updates summary, quarantine, cache, and
    // resume state.
    void handleScanResult(
        const FileInfo& info,
        FileMetadata metadata,
        const std::vector<std::string>& signatures,
        ScanSummary& summary);

    Logger& logger_;
    SignatureLoader signature_loader_;
    AutomatonBuilder automaton_builder_;
    ExcludeSet exclude_set_;
    CacheManager cache_manager_;
    ResumeManager resume_manager_;
    QuarantineManager quarantine_manager_;
};

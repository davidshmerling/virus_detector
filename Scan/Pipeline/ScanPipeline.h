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

class FileProcessor;  // used only by reference in the per-file helper
class ThreadPool;     // used only by reference when enqueuing scan tasks

// Wires the whole scan pipeline together for one run:
//   load signatures → build automaton → prepare resume → walk the tree →
//   enqueue each file → workers scan → wait → commit cache generation.
// It performs none of the heavy lifting itself (no chunk reading, no automaton
// search, no DFS, no SQLite); it only connects the classes that do. Open this
// one file to see what happens from the start of a scan to the end.
class ScanPipeline {
public:
    explicit ScanPipeline(Logger& logger);

    void scan(const std::filesystem::path& root);

private:
    // Runs on the discovery thread for each file the walker finds: a cache hit
    // is settled at once, a miss is enqueued for a worker. Always returns true
    // to keep the walk going.
    bool handleDiscoveredFile(
        const FileInfo& info,
        const FileProcessor& processor,
        ThreadPool& pool,
        std::int64_t signatures_last_modified,
        ScanSummary& summary);

    // Reuses a cached verdict without touching the file, updating the summary
    // and the resume log. Re-stamps the cache entry with the current generation
    // so this still-present file survives the next stale-entry cleanup.
    void handleCacheHit(
        const FileInfo& info,
        FileVerdict verdict,
        FileMetadata metadata,
        ScanSummary& summary);

    // Hands a cache miss to a worker as one pool task, carrying the precomputed
    // metadata so the worker can record the verdict without recomputing it.
    void enqueueForScan(
        const FileInfo& info,
        const FileProcessor& processor,
        ThreadPool& pool,
        FileMetadata metadata,
        ScanSummary& summary);

    // Runs on a worker thread for one file: scan, quarantine on a match, cache
    // the fresh verdict, and mark the file complete for resume. The file's
    // size, last-modified time, and relative path already come from the walker
    // inside FileInfo, so this does no extra filesystem calls before scanning.
    void handleFile(
        const FileProcessor& processor,
        const FileInfo& info,
        FileMetadata metadata,
        ScanSummary& summary);

    Logger& logger_;
    SignatureLoader signature_loader_;
    AutomatonBuilder automaton_builder_;
    ExcludeSet exclude_set_;
    CacheManager cache_manager_;
    ResumeManager resume_manager_;
    QuarantineManager quarantine_manager_;
};

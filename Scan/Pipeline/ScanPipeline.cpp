#include "Scan/Pipeline/ScanPipeline.h"

#include "Scan/Automaton/AutomatonScanner.h"
#include "Cache/CacheEntry.h"
#include "Common/FileVerdict.h"
#include "Console/ConsolePrinter.h"
#include "Scan/FileProcessor.h"
#include "Scan/Traversal/FileTreeWalker.h"
#include "ThreadPool/ThreadPool.h"

#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

ScanPipeline::ScanPipeline(Logger& logger)
    : logger_(logger),
      cache_manager_(logger_),
      resume_manager_(logger_, "runtime/resume/checkpoint.txt"),
      quarantine_manager_(logger_, "runtime/quarantine")
{
}

void ScanPipeline::scan(const fs::path& root)
{
    // Load signatures before any other scan work.
    if (!signature_loader_.load()) {
        ConsolePrinter::printError("No signatures loaded");
        logger_.error("Scan aborted: no signatures loaded");
        return;
    }

    // Build the automaton once, then a shared scanner over it.
    const Automaton automaton =
        automaton_builder_.build(signature_loader_.signatures());
    const AutomatonScanner scanner(automaton);

    // A single processor is shared by all workers: its read buffer is
    // thread_local, so each worker still gets its own buffer.
    const FileProcessor processor(scanner, signature_loader_.signatures());

    // Load exclude rules, the persisted cache, and quarantine state.
    exclude_set_.load();
    cache_manager_.load();
    quarantine_manager_.load();

    const std::int64_t signatures_last_modified = signature_loader_.lastModified();

    // Start fresh or resume from the last checkpoint.
    bool resumed = false;
    resume_manager_.begin(root, resumed);
    const fs::path resume_from =
        resumed ? resume_manager_.nextFile() : fs::path{};

    ThreadPool pool;
    ScanSummary summary;

    ConsolePrinter::printScanStarted(root.string());
    logger_.info("Scan started: " + root.string());
    if (resumed) {
        logger_.info("Scan resumed from: " + resume_from.generic_string());
    }

    // Walk the tree; each discovered file is a cache hit or miss.
    FileTreeWalker walker(logger_, exclude_set_);
    walker.walk(root, resume_from, [&](const FileInfo& info) {
        return handleDiscoveredFile(
            info, processor, pool, signatures_last_modified, summary);
    });

    // No more files will be discovered.
    resume_manager_.discoveryFinished();

    // Wait for all in-flight files to finish.
    pool.wait();

    // Persist leftover cache upserts; on scan-all, drop entries this run never
    // saw; then promote the generation. Reaching here means no crash — a kill
    // never gets here, so pruning never runs on an incomplete scan.
    // Only "/" is treated as a full-system scan for stale-entry pruning.
    const bool full_system_scan = (root == "/");
    cache_manager_.commitGeneration(full_system_scan);

    ConsolePrinter::printScanSummary(summary);
    logger_.info("Scan completed. " + summary.toLogLine());
}

bool ScanPipeline::handleDiscoveredFile(
    const FileInfo& info,
    const FileProcessor& processor,
    ThreadPool& pool,
    std::int64_t signatures_last_modified,
    ScanSummary& summary)
{
    ++summary.discovered;

    // Size and last-modified time were already gathered by the walker, so the
    // cache lookup needs no extra filesystem calls.
    const FileMetadata metadata{
        .last_modified = info.last_modified,
        .size = info.size,
        .signatures_last_modified = signatures_last_modified};

    const std::optional<FileVerdict> cached =
        cache_manager_.cachedVerdict(info.path.generic_string(), metadata);

    if (cached) {
        handleCacheHit(info, *cached, metadata, summary);
    } else {
        handleCacheMiss(info, processor, pool, metadata, summary);
    }

    return true;
}

void ScanPipeline::handleCacheHit(
    const FileInfo& info,
    FileVerdict verdict,
    FileMetadata metadata,
    ScanSummary& summary)
{
    ++summary.cached;
    if (verdict == FileVerdict::Malicious) {
        ++summary.malicious;
    }

    // Re-stamp with the current generation so this still-present file is not
    // reclaimed as stale on the next run.
    cache_manager_.update(CacheEntry{
        .path = info.path.generic_string(),
        .metadata = metadata,
        .verdict = verdict});

    // Mark discovered and completed immediately so a checkpoint never re-scans
    // a cache hit.
    resume_manager_.addFile(info.relative_path);
    resume_manager_.fileCompleted(info.relative_path);
}

void ScanPipeline::handleCacheMiss(
    const FileInfo& info,
    const FileProcessor& processor,
    ThreadPool& pool,
    FileMetadata metadata,
    ScanSummary& summary)
{
    resume_manager_.addFile(info.relative_path);

    // Captures `processor` and `summary` by reference: both outlive every task
    // because scan() calls pool.wait() before they leave scope.
    pool.enqueue([this, &processor, &summary, info, metadata]() {
        const std::optional<std::vector<std::string>> result =
            processor.process(info.path);

        // Open/read failure is Error, not Clean — do not cache.
        if (!result) {
            ++summary.failed;
            logger_.warning("Could not scan file: " + info.path.string());
            resume_manager_.fileCompleted(info.relative_path);
            return;
        }

        handleScanResult(info, metadata, *result, summary);
    });
}

void ScanPipeline::handleScanResult(
    const FileInfo& info,
    FileMetadata metadata,
    const std::vector<std::string>& signatures,
    ScanSummary& summary)
{
    ++summary.scanned;

    const FileVerdict verdict =
        signatures.empty() ? FileVerdict::Clean : FileVerdict::Malicious;

    if (verdict == FileVerdict::Malicious) {
        ++summary.malicious;
        logger_.info("Malicious file detected: " + info.path.string());
        if (quarantine_manager_.quarantine(info.path, signatures)) {
            ++summary.quarantined;
        }
    }

    cache_manager_.update(CacheEntry{
        .path = info.path.generic_string(),
        .metadata = metadata,
        .verdict = verdict});

    resume_manager_.fileCompleted(info.relative_path);
}

#include "Scan/Pipeline/ScanPipeline.h"

#include "Scan/Automaton/AutomatonScanner.h"
#include "Cache/CacheEntry.h"
#include "Common/FileVerdict.h"
#include "Console/ConsolePrinter.h"
#include "Scan/FileProcessor.h"
#include "Scan/Traversal/FileTreeWalker.h"
#include "ThreadPool/ThreadPool.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr std::size_t kWorkerCount = 16;
constexpr std::size_t kQueueCapacity = 1024;

}  // namespace

ScanPipeline::ScanPipeline(Logger& logger)
    : logger_(logger),
      cache_manager_(logger_),
      resume_manager_(logger_, "runtime/resume/checkpoint.txt"),
      quarantine_manager_(logger_, "runtime/quarantine")
{
}

void ScanPipeline::scan(const fs::path& root)
{
    // 1. Load signatures.
    if (!signature_loader_.load()) {
        ConsolePrinter::printError("No signatures loaded");
        logger_.error("Scan aborted: no signatures loaded");
        return;
    }

    // 2. Build the automaton once, then a shared scanner over it.
    const Automaton automaton =
        automaton_builder_.build(signature_loader_.signatures());
    const AutomatonScanner scanner(automaton);

    // A single processor is shared by all workers: its read buffer is
    // thread_local, so each worker still gets its own buffer.
    const FileProcessor processor(scanner, signature_loader_.signatures());

    // 3. Load exclude rules, the persisted cache, and quarantine state.
    exclude_set_.load();
    exclude_set_.excludeProjectRoot();
    cache_manager_.load();
    quarantine_manager_.load();

    const std::int64_t signatures_last_modified = signature_loader_.lastModified();

    // 4. Start fresh or resume from the last checkpoint.
    bool resumed = false;
    resume_manager_.begin(root, resumed);
    const fs::path resume_from =
        resumed ? resume_manager_.nextFile() : fs::path{};

    // 5. Workers + summary.
    ThreadPool pool(kWorkerCount, kQueueCapacity);
    ScanSummary summary;

    ConsolePrinter::printScanStarted(root.string());
    logger_.info("Scan started: " + root.string());
    if (resumed) {
        logger_.info("Scan resumed from: " + resume_from.generic_string());
    }

    // 6. Walk the tree; each discovered file is either served from cache or
    // handed to a worker for scanning.
    FileTreeWalker walker(logger_, exclude_set_);
    walker.walk(root, resume_from, [&](const FileInfo& info) {
        return handleDiscoveredFile(
            info, processor, pool, signatures_last_modified, summary);
    });

    // 7. No more files will be discovered.
    resume_manager_.discoveryFinished();

    // 8. Wait for all in-flight files to finish.
    pool.wait();

    // 9. Persist leftover cache upserts; on scan-all, drop entries this run
    // never saw; then promote the generation. Reaching here means no crash —
    // a kill never gets here, so pruning never runs on an incomplete scan.
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

    // Size and last-modified time were already gathered by the walker; no
    // file_size()/last_write_time() calls are needed here.
    FileMetadata metadata;
    metadata.last_modified = info.last_modified;
    metadata.size = info.size;
    metadata.signatures_last_modified = signatures_last_modified;

    // Cache hit: file and signatures unchanged since last scan — reuse verdict.
    if (const std::optional<FileVerdict> cached =
            cache_manager_.cachedVerdict(info.path.generic_string(), metadata)) {
        handleCacheHit(info, *cached, metadata, summary);
        return true;
    }

    enqueueForScan(info, processor, pool, metadata, summary);
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

    // Re-stamp the entry with the current generation (done inside update) so
    // this still-present file is not reclaimed as stale on the next run.
    cache_manager_.update(CacheEntry{
        .path = info.path.generic_string(),
        .metadata = metadata,
        .verdict = verdict});

    // Record the file as discovered and immediately completed so a checkpoint
    // never asks to re-scan it.
    resume_manager_.addFile(info.relative_path);
    resume_manager_.fileCompleted(info.relative_path);
}

void ScanPipeline::enqueueForScan(
    const FileInfo& info,
    const FileProcessor& processor,
    ThreadPool& pool,
    FileMetadata metadata,
    ScanSummary& summary)
{
    resume_manager_.addFile(info.relative_path);

    // The precomputed metadata rides along so the worker records the fresh
    // verdict without recomputing it.
    pool.enqueue([&, info, metadata]() {
        handleFile(processor, info, metadata, summary);
    });
}

void ScanPipeline::handleFile(
    const FileProcessor& processor,
    const FileInfo& info,
    FileMetadata metadata,
    ScanSummary& summary)
{
    const fs::path& file = info.path;
    const fs::path& relative = info.relative_path;

    const std::vector<std::string> signatures = processor.process(file);
    ++summary.scanned;

    const FileVerdict verdict =
        signatures.empty() ? FileVerdict::Clean : FileVerdict::Malicious;
    if (verdict == FileVerdict::Malicious) {
        ++summary.malicious;
        logger_.info("Malicious file detected: " + file.string());
        if (quarantine_manager_.quarantine(file, signatures)) {
            ++summary.quarantined;
        }
    }

    // Record the fresh verdict so the next run can skip this file.
    cache_manager_.update(CacheEntry{
        .path = file.generic_string(),
        .metadata = metadata,
        .verdict = verdict});

    resume_manager_.fileCompleted(relative);
}

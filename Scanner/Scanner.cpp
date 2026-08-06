#include "Scanner/Scanner.h"

#include "Cache/SqliteCacheRepository.h"
#include "CLI/ConsolePrinter.h"
#include "Common/Error.h"
#include "Common/OperationResult.h"

#include <exception>
#include <filesystem>
#include <memory>
#include <string>

namespace fs = std::filesystem;

Scanner::Scanner(
    std::string signatures_file,
    std::string exclude_file,
    QuarantineManager& quarantine_manager,
    Logger& logger,
    PerformanceProfiler& profiler,
    fs::path project_root,
    std::size_t worker_count,
    std::size_t queue_capacity)
    : signature_manager_(std::move(signatures_file)),
      exclude_manager_(std::move(exclude_file)),
      file_enumerator_(exclude_manager_, logger, profiler),
      checkpoint_repository_("runtime/resume/checkpoint.json"),
      progress_tracker_(checkpoint_repository_, logger, profiler),
      cache_manager_(
          std::make_unique<SqliteCacheRepository>("runtime/cache/cache.db"),
          logger,
          profiler,
          100),
      quarantine_manager_(quarantine_manager),
      thread_pool_(worker_count, queue_capacity),
      logger_(logger),
      profiler_(profiler),
      project_root_(std::move(project_root))
{
    // Checkpoint advances only after CacheWriter makes results durable.
    cache_manager_.setOnDurableComplete(
        [this](const fs::path& relative_path) {
            return progress_tracker_.markCompleted(relative_path);
        });
}

void Scanner::applyInternalExclusions(const fs::path& scan_root)
{
    exclude_manager_.clearInternalExcludedPaths();

    // Always skip scanner infrastructure under the project root.
    exclude_manager_.addInternalExcludedPath(project_root_ / "runtime");
    exclude_manager_.addInternalExcludedPath(project_root_ / "build");
    exclude_manager_.addInternalExcludedPath(project_root_ / ".git");
    exclude_manager_.addInternalExcludedPath(
        project_root_ / "config" / "signatures.txt");

    // Full-filesystem scan: also skip the whole project tree so we do not
    // scan/quarantine our own sources, docs, or config.
    // Targeted `scan <path>` must still be able to scan e.g. ./test_scan.
    if (scan_root == scan_root.root_path()) {
        exclude_manager_.addInternalExcludedPath(project_root_);
        logger_.info(
            "scan-all: excluding project root " + project_root_.string());
    }
}

bool Scanner::normalizeRoot(
    const fs::path& root,
    fs::path& normalized) const
{
    std::error_code error;
    normalized = fs::absolute(root, error).lexically_normal();

    if (error) {
        logger_.error(
            formatError({
                ErrorCode::InvalidArgument,
                "Could not normalize root: " + root.string() +
                    " - " + error.message()}));
        return false;
    }

    return true;
}

bool Scanner::loadRunningCheckpoint(
    const fs::path& root,
    ScanCheckpoint& checkpoint) const
{
    if (!checkpoint_repository_.exists()) {
        return false;
    }

    if (!checkpoint_repository_.load(checkpoint)) {
        return false;
    }

    if (checkpoint.status != "running") {
        return false;
    }

    fs::path checkpoint_root;
    fs::path current_root;

    if (!normalizeRoot(checkpoint.root, checkpoint_root) ||
        !normalizeRoot(root, current_root)) {
        return false;
    }

    return checkpoint_root.generic_string() ==
           current_root.generic_string();
}

bool Scanner::initialize()
{
    logger_.info("Initializing scanner");

    if (!loadSignatures()) {
        return false;
    }

    if (!buildAutomaton()) {
        return false;
    }

    if (!loadExclusions()) {
        return false;
    }

    if (!initializeResume()) {
        return false;
    }

    if (!initializeCache()) {
        return false;
    }

    if (!initializeQuarantine()) {
        return false;
    }

    if (!createFileProcessor()) {
        return false;
    }

    initialized_ = true;
    return true;
}

bool Scanner::loadSignatures()
{
    const OperationResult result = signature_manager_.load();
    if (!result.success) {
        logger_.error(formatError(result.error));
        ConsolePrinter::printError("Could not initialize scanner");
        return false;
    }

    signatures_last_modified_ = signature_manager_.lastModified();
    return true;
}

bool Scanner::buildAutomaton()
{
    automaton_.build(signature_manager_.getSignatures());

    if (!automaton_.isBuilt() || signature_manager_.count() == 0) {
        logger_.error("Could not build Aho-Corasick automaton");
        ConsolePrinter::printError("Could not initialize scanner");
        return false;
    }

    logger_.info(
        "Automaton built. Signatures: " +
        std::to_string(automaton_.signatureCount()) +
        ", nodes: " +
        std::to_string(automaton_.nodeCount()));

    return true;
}

bool Scanner::loadExclusions()
{
    const OperationResult result = exclude_manager_.load();
    if (!result.success) {
        logger_.error(formatError(result.error));
        ConsolePrinter::printError("Could not load exclude file");
        return false;
    }

    logger_.info(
        "Exclude list loaded. Paths: " +
        std::to_string(exclude_manager_.getExcludedPaths().size()));

    return true;
}

bool Scanner::initializeResume()
{
    if (!checkpoint_repository_.initialize()) {
        logger_.error(
            formatError({
                ErrorCode::CheckpointFailed,
                "Could not initialize checkpoint repository"}));
        ConsolePrinter::printError("Could not initialize checkpoint");
        return false;
    }

    return true;
}

bool Scanner::initializeCache()
{
    if (!cache_manager_.initialize()) {
        logger_.warning(
            "Cache could not be loaded. Starting with empty cache");
    }

    return true;
}

bool Scanner::initializeQuarantine()
{
    if (!quarantine_manager_.initialize()) {
        logger_.error(
            formatError({
                ErrorCode::QuarantineFailed,
                "Could not initialize quarantine"}));
        ConsolePrinter::printError("Could not initialize quarantine");
        return false;
    }

    return true;
}

bool Scanner::createFileProcessor()
{
    file_processor_ = std::make_unique<FileProcessor>(
        automaton_,
        signature_manager_,
        cache_manager_,
        quarantine_manager_,
        logger_,
        profiler_,
        signatures_last_modified_);

    return file_processor_ != nullptr;
}

bool Scanner::prepareProgress(
    const fs::path& root,
    ScanCheckpoint& checkpoint,
    bool& resuming)
{
    resuming = loadRunningCheckpoint(root, checkpoint);

    if (resuming) {
        if (!progress_tracker_.resumeScan(checkpoint)) {
            logger_.error("Could not initialize scan progress");
            return false;
        }

        logger_.info(
            "Resuming scan from: " +
            checkpoint.next_unfinished_path.generic_string());
        return true;
    }

    if (!progress_tracker_.startNewScan(root)) {
        logger_.error("Could not initialize scan progress");
        return false;
    }

    return true;
}

bool Scanner::submitFile(
    const fs::path& file_path,
    const fs::path& root,
    ScanSummary& summary)
{
    ScopedPerformanceTimer timer(
        profiler_,
        PerformanceSection::SubmitTask);

    ++summary.discovered;

    std::error_code error;
    fs::path relative_path;

    {
        ScopedPerformanceTimer relative_timer(
            profiler_,
            PerformanceSection::RelativePath);

        relative_path =
            fs::relative(file_path, root, error).lexically_normal();
    }

    if (error) {
        logger_.warning(
            "Could not create relative path: " +
            file_path.string());
        return true;
    }

    {
        ScopedPerformanceTimer register_timer(
            profiler_,
            PerformanceSection::ProgressRegister);

        if (!progress_tracker_.registerTask(relative_path)) {
            logger_.error(
                "Could not register task: " +
                relative_path.string());
            return false;
        }
    }

    bool enqueued = false;

    {
        ScopedPerformanceTimer queue_timer(
            profiler_,
            PerformanceSection::QueueWait);

        enqueued = thread_pool_.enqueue(
            [this, file_path, relative_path, &summary]() {
                try {
                    file_processor_->process(
                        file_path,
                        relative_path,
                        summary);
                } catch (const std::exception& exception) {
                    logger_.error(
                        "Unhandled error while scanning " +
                        file_path.string() +
                        ": " +
                        exception.what());
                    ++summary.failed;
                    cache_manager_.notifyDurableComplete(relative_path);
                } catch (...) {
                    logger_.error(
                        "Unknown error while scanning: " +
                        file_path.string());
                    ++summary.failed;
                    cache_manager_.notifyDurableComplete(relative_path);
                }
            });
    }

    if (!enqueued) {
        progress_tracker_.cancelTask(relative_path);
        logger_.error(
            "Could not enqueue task: " +
            file_path.string());
        return false;
    }

    return true;
}

bool Scanner::enumerateFiles(
    const fs::path& root,
    bool resuming,
    const ScanCheckpoint& checkpoint,
    ScanSummary& summary)
{
    const auto on_file =
        [&](const fs::path& file_path) -> bool {
            return submitFile(file_path, root, summary);
        };

    if (resuming) {
        return file_enumerator_.resumeFromSorted(
            root,
            checkpoint.next_unfinished_path,
            summary,
            on_file);
    }

    return file_enumerator_.enumerateSorted(root, summary, on_file);
}

void Scanner::finishScan(
    const fs::path& root,
    bool enumeration_ok,
    ScanSummary& summary)
{
    progress_tracker_.markEnumerationFinished();
    thread_pool_.wait();

    // Persist cache first: successful COMMIT drives markCompleted.
    if (!cache_manager_.flush()) {
        logger_.error("Could not flush cache");
    }

    progress_tracker_.flush();

    if (!enumeration_ok) {
        logger_.warning("Scan stopped before completion");
        ++summary.failed;
    } else {
        logger_.info("Scan completed: " + root.string());
    }

    logger_.info(summary.toLogLine());
}

ScanSummary Scanner::scanRoot(const fs::path& root)
{
    ScopedPerformanceTimer total_timer(
        profiler_,
        PerformanceSection::TotalScan);

    ScanSummary summary;

    if (!initialized_ || !file_processor_) {
        logger_.error("Scan requested before scanner initialization");
        ConsolePrinter::printError("Scanner is not initialized");
        summary.failed = 1;
        return summary;
    }

    fs::path normalized_root;
    if (!normalizeRoot(root, normalized_root)) {
        ConsolePrinter::printError("Could not normalize scan root");
        summary.failed = 1;
        return summary;
    }

    logger_.info("Scan started. Root: " + normalized_root.string());
    ConsolePrinter::printScanStarted(normalized_root.string());

    applyInternalExclusions(normalized_root);

    ScanCheckpoint checkpoint;
    bool resuming = false;

    if (!prepareProgress(normalized_root, checkpoint, resuming)) {
        ++summary.failed;
        return summary;
    }

    bool enumeration_ok = false;

    {
        ScopedPerformanceTimer timer(
            profiler_,
            PerformanceSection::Enumeration);

        enumeration_ok =
            enumerateFiles(normalized_root, resuming, checkpoint, summary);
    }

    finishScan(normalized_root, enumeration_ok, summary);

    return summary;
}

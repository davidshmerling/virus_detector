#include "Scanner/Scanner.h"

#include "Cache/JsonCacheRepository.h"
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
    std::size_t worker_count,
    std::size_t queue_capacity)
    : signature_manager_(std::move(signatures_file)),
      exclude_manager_(std::move(exclude_file)),
      file_enumerator_(exclude_manager_, logger),
      checkpoint_repository_("runtime/resume/checkpoint.json"),
      progress_tracker_(checkpoint_repository_, logger),
      cache_manager_(
          std::make_unique<JsonCacheRepository>("runtime/cache/cache.json"),
          logger,
          100),
      quarantine_manager_(quarantine_manager),
      thread_pool_(worker_count, queue_capacity),
      logger_(logger)
{
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

    const OperationResult signatures_result = signature_manager_.load();
    if (!signatures_result.success) {
        logger_.error(formatError(signatures_result.error));
        ConsolePrinter::printError("Could not initialize scanner");
        return false;
    }

    signatures_last_modified_ = signature_manager_.lastModified();

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

    const OperationResult exclude_result = exclude_manager_.load();
    if (!exclude_result.success) {
        logger_.error(formatError(exclude_result.error));
        ConsolePrinter::printError("Could not load exclude file");
        return false;
    }

    logger_.info(
        "Exclude list loaded. Paths: " +
        std::to_string(exclude_manager_.getExcludedPaths().size()));

    if (!checkpoint_repository_.initialize()) {
        logger_.error(
            formatError({
                ErrorCode::CheckpointFailed,
                "Could not initialize checkpoint repository"}));
        ConsolePrinter::printError("Could not initialize checkpoint");
        return false;
    }

    if (!cache_manager_.initialize()) {
        logger_.warning(
            "Cache could not be loaded. Starting with empty cache");
    }

    if (!quarantine_manager_.initialize()) {
        logger_.error(
            formatError({
                ErrorCode::QuarantineFailed,
                "Could not initialize quarantine"}));
        ConsolePrinter::printError("Could not initialize quarantine");
        return false;
    }

    file_processor_ = std::make_unique<FileProcessor>(
        automaton_,
        signature_manager_,
        cache_manager_,
        quarantine_manager_,
        logger_,
        signatures_last_modified_);

    initialized_ = true;
    return true;
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
    ++summary.discovered;

    std::error_code error;
    const fs::path relative_path =
        fs::relative(file_path, root, error).lexically_normal();

    if (error) {
        logger_.warning(
            "Could not create relative path: " +
            file_path.string());
        return true;
    }

    if (!progress_tracker_.registerTask(relative_path)) {
        logger_.error(
            "Could not register task: " +
            relative_path.string());
        return false;
    }

    const bool enqueued = thread_pool_.enqueue(
        [this, file_path, relative_path, &summary]() {
            try {
                file_processor_->process(file_path, summary);
            } catch (const std::exception& exception) {
                logger_.error(
                    "Unhandled error while scanning " +
                    file_path.string() +
                    ": " +
                    exception.what());
                ++summary.failed;
            } catch (...) {
                logger_.error(
                    "Unknown error while scanning: " +
                    file_path.string());
                ++summary.failed;
            }

            progress_tracker_.markCompleted(relative_path);
        });

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
    progress_tracker_.flush();

    if (!cache_manager_.flush()) {
        logger_.error("Could not flush cache");
    }

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

    ScanCheckpoint checkpoint;
    bool resuming = false;

    if (!prepareProgress(normalized_root, checkpoint, resuming)) {
        ++summary.failed;
        return summary;
    }

    const bool enumeration_ok =
        enumerateFiles(normalized_root, resuming, checkpoint, summary);

    finishScan(normalized_root, enumeration_ok, summary);

    return summary;
}

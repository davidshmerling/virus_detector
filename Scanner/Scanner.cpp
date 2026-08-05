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
    fs::path quarantine_directory,
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
      quarantine_manager_(
          std::move(quarantine_directory),
          logger),
      thread_pool_(worker_count, queue_capacity),
      logger_(logger)
{
}

fs::path Scanner::normalizeRoot(const fs::path& root)
{
    std::error_code error;
    return fs::absolute(root, error).lexically_normal();
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

    return normalizeRoot(checkpoint.root).generic_string() ==
           normalizeRoot(root).generic_string();
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
        // Cache is an optimization — continue with an empty cache.
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

    initialized_ = true;
    return true;
}

void Scanner::processFile(
    const fs::path& file_path,
    ScanSummary& summary)
{
    const auto cached_verdict = cache_manager_.getValidVerdict(
        file_path,
        signatures_last_modified_);

    if (cached_verdict.has_value()) {
        ++summary.cached;
        return;
    }

    FileScanner file_scanner(automaton_);
    const FileScanResult result = file_scanner.scan(file_path);

    switch (result.verdict) {
        case FileVerdict::Clean:
            ++summary.scanned;
            cache_manager_.update(
                file_path,
                signatures_last_modified_,
                FileVerdict::Clean);
            break;

        case FileVerdict::Malicious: {
            ++summary.scanned;
            ++summary.malicious;

            const auto& signatures =
                signature_manager_.getSignatures();

            std::string matched_signature = "unknown";

            if (result.matched_signature_index < signatures.size()) {
                matched_signature =
                    signatures[result.matched_signature_index];
            }

            logger_.warning(
                "Malicious file found: " +
                result.file_path.string() +
                ", signature: " +
                matched_signature);

            if (!quarantine_manager_.quarantine(
                    result.file_path,
                    matched_signature)) {

                logger_.error(
                    formatError({
                        ErrorCode::QuarantineFailed,
                        "Could not quarantine file: " +
                            result.file_path.string()}));

                ConsolePrinter::printError(
                    "Malicious file was found but could not "
                    "be quarantined: " +
                    result.file_path.string());

                ++summary.failed;
            } else {
                ++summary.quarantined;
                ConsolePrinter::printMessage(
                    "Malicious file moved to quarantine: " +
                    result.file_path.string());
            }

            break;
        }

        case FileVerdict::Error:
            logger_.error(formatError(result.error));
            ++summary.failed;
            break;
    }
}

ScanSummary Scanner::scanRoot(const fs::path& root)
{
    ScanSummary summary;

    if (!initialized_) {
        logger_.error("Scan requested before scanner initialization");
        ConsolePrinter::printError("Scanner is not initialized");
        summary.failed = 1;
        return summary;
    }

    const fs::path absolute_root = normalizeRoot(root);

    logger_.info("Scan started. Root: " + absolute_root.string());
    ConsolePrinter::printScanStarted(absolute_root.string());

    ScanCheckpoint checkpoint;
    const bool resuming = loadRunningCheckpoint(absolute_root, checkpoint);

    if (resuming) {
        if (!progress_tracker_.resumeScan(checkpoint)) {
            logger_.error("Could not initialize scan progress");
            summary.failed = 1;
            return summary;
        }

        logger_.info(
            "Resuming scan from: " +
            checkpoint.next_unfinished_path.generic_string());
    } else if (!progress_tracker_.startNewScan(absolute_root)) {
        logger_.error("Could not initialize scan progress");
        summary.failed = 1;
        return summary;
    }

    const auto on_file =
        [&](const fs::path& file_path) -> bool {
            ++summary.discovered;

            std::error_code error;
            const fs::path relative_path =
                fs::relative(file_path, absolute_root, error)
                    .lexically_normal();

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
                    /*
                     * Always mark completed, even on failure,
                     * so the checkpoint never stalls forever.
                     */
                    try {
                        processFile(file_path, summary);
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
        };

    const bool enumeration_ok = resuming
        ? file_enumerator_.resumeFromSorted(
              absolute_root,
              checkpoint.next_unfinished_path,
              summary,
              on_file)
        : file_enumerator_.enumerateSorted(
              absolute_root,
              summary,
              on_file);

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
        logger_.info("Scan completed: " + absolute_root.string());
    }

    logger_.info(summary.toLogLine());

    return summary;
}

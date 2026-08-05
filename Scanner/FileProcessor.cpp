#include "Scanner/FileProcessor.h"

#include "CLI/ConsolePrinter.h"
#include "Common/Error.h"
#include "Scanner/FileScanner/FileScanner.h"

namespace fs = std::filesystem;

FileProcessor::FileProcessor(
    const AhoCorasick& automaton,
    const SignatureManager& signature_manager,
    CacheManager& cache_manager,
    QuarantineManager& quarantine_manager,
    Logger& logger,
    std::int64_t signatures_last_modified)
    : automaton_(automaton),
      signature_manager_(signature_manager),
      cache_manager_(cache_manager),
      quarantine_manager_(quarantine_manager),
      logger_(logger),
      signatures_last_modified_(signatures_last_modified)
{
}

void FileProcessor::process(
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

        case FileVerdict::Malicious:
            ++summary.scanned;
            ++summary.malicious;
            handleMaliciousFile(
                result.file_path,
                result.matched_signature_index,
                summary);
            break;

        case FileVerdict::Error:
            logger_.error(formatError(result.error));
            ++summary.failed;
            break;
    }
}

void FileProcessor::handleMaliciousFile(
    const fs::path& file_path,
    std::size_t matched_signature_index,
    ScanSummary& summary)
{
    const auto& signatures = signature_manager_.getSignatures();

    std::string matched_signature = "unknown";
    if (matched_signature_index < signatures.size()) {
        matched_signature = signatures[matched_signature_index];
    }

    logger_.warning(
        "Malicious file found: " +
        file_path.string() +
        ", signature: " +
        matched_signature);

    if (!quarantine_manager_.quarantine(file_path, matched_signature)) {
        logger_.error(
            formatError({
                ErrorCode::QuarantineFailed,
                "Could not quarantine file: " + file_path.string()}));

        ConsolePrinter::printError(
            "Malicious file was found but could not "
            "be quarantined: " +
            file_path.string());

        ++summary.failed;
        return;
    }

    ++summary.quarantined;
    ConsolePrinter::printMessage(
        "Malicious file moved to quarantine: " +
        file_path.string());
}

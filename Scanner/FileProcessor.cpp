#include "Scanner/FileProcessor.h"

#include "CLI/ConsolePrinter.h"
#include "Common/Error.h"

#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

FileProcessor::FileProcessor(
    const AhoCorasick& automaton,
    const SignatureManager& signature_manager,
    CacheManager& cache_manager,
    QuarantineManager& quarantine_manager,
    Logger& logger,
    PerformanceProfiler& profiler,
    std::int64_t signatures_last_modified)
    : automaton_(automaton),
      signature_manager_(signature_manager),
      cache_manager_(cache_manager),
      quarantine_manager_(quarantine_manager),
      logger_(logger),
      profiler_(profiler),
      signatures_last_modified_(signatures_last_modified)
{
}

void FileProcessor::completeDurable(const fs::path& relative_path)
{
    cache_manager_.notifyDurableComplete(relative_path);
}

bool FileProcessor::checkCache(
    const fs::path& file_path,
    ScanSummary& summary)
{
    const auto cached_verdict = cache_manager_.getValidVerdict(
        file_path,
        signatures_last_modified_);

    if (!cached_verdict.has_value()) {
        return false;
    }

    ++summary.cached;
    return true;
}

FileScanResult FileProcessor::scanFile(const fs::path& file_path)
{
    // One scanner (and read buffer) per worker thread — reused across files.
    thread_local FileScanner file_scanner(automaton_, profiler_);
    return file_scanner.scan(file_path);
}

void FileProcessor::handleCleanFile(
    const fs::path& file_path,
    const fs::path& relative_path,
    ScanSummary& summary)
{
    ++summary.scanned;

    if (!cache_manager_.update(
            file_path,
            relative_path,
            signatures_last_modified_,
            FileVerdict::Clean)) {
        // No cache write queued — still advance progress so resume cannot stall.
        completeDurable(relative_path);
    }
}

void FileProcessor::handleMaliciousFile(
    const fs::path& file_path,
    const std::vector<std::size_t>& matched_signature_indices,
    ScanSummary& summary)
{
    ++summary.scanned;
    ++summary.malicious;

    const auto& signatures = signature_manager_.getSignatures();

    std::vector<std::string> matched_words;
    matched_words.reserve(matched_signature_indices.size());
    std::unordered_set<std::string> seen_words;

    for (const std::size_t index : matched_signature_indices) {
        if (index >= signatures.size()) {
            continue;
        }

        const std::string& word = signatures[index];
        if (seen_words.insert(word).second) {
            matched_words.push_back(word);
        }
    }

    if (matched_words.empty()) {
        matched_words.push_back("unknown");
    }

    std::string joined;
    for (std::size_t i = 0; i < matched_words.size(); ++i) {
        if (i > 0) {
            joined += ", ";
        }
        joined += matched_words[i];
    }

    logger_.warning(
        "Malicious file found: " +
        file_path.string() +
        ", signatures: " +
        joined);

    bool quarantine_success = false;

    {
        ScopedPerformanceTimer timer(
            profiler_,
            PerformanceSection::Quarantine);

        quarantine_success =
            quarantine_manager_.quarantine(
                file_path,
                matched_words);
    }

    if (!quarantine_success) {
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

void FileProcessor::handleScanError(
    const FileScanResult& result,
    ScanSummary& summary)
{
    logger_.error(formatError(result.error));
    ++summary.failed;
}

void FileProcessor::process(
    const fs::path& file_path,
    const fs::path& relative_path,
    ScanSummary& summary)
{
    ScopedPerformanceTimer timer(
        profiler_,
        PerformanceSection::FileProcessing);

    if (checkCache(file_path, summary)) {
        // Already durable in SQLite from a previous commit.
        completeDurable(relative_path);
        return;
    }

    const FileScanResult result = scanFile(file_path);

    switch (result.verdict) {
        case FileVerdict::Clean:
            handleCleanFile(file_path, relative_path, summary);
            break;

        case FileVerdict::Malicious:
            handleMaliciousFile(
                result.file_path,
                result.matched_signature_indices,
                summary);
            completeDurable(relative_path);
            break;

        case FileVerdict::Error:
            handleScanError(result, summary);
            completeDurable(relative_path);
            break;
    }
}

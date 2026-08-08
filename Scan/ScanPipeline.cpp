#include "Scan/ScanPipeline.h"

#include "Scan/Automaton/AutomatonScanner.h"
#include "Cache/CacheEntry.h"
#include "Common/FileVerdict.h"
#include "Console/ConsolePrinter.h"
#include "Scan/FileProcessor.h"
#include "Scan/FileTreeWalker.h"
#include "ThreadPool/ThreadPool.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_set>
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
    const FileProcessor processor(scanner);

    // 3. Load exclude rules, the persisted cache, and quarantine state.
    exclude_manager_.load();
    excludeSelf();
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

    // 6. Walk the tree; every discovered file becomes one pool task.
    FileTreeWalker walker(logger_, exclude_manager_);
    walker.walk(root, resume_from, [&](const fs::path& file) {
        ++summary.discovered;
        resume_manager_.addFile(fs::relative(file, root));

        pool.enqueue([&, file]() {
            handleFile(processor, file, root, signatures_last_modified, summary);
        });
        return true;
    });

    // 7. No more files will be discovered.
    resume_manager_.discoveryFinished();

    // 8. Wait for all in-flight files to finish.
    pool.wait();

    // 9. Persist the cache (the checkpoint is saved on every state change).
    cache_manager_.flush();

    ConsolePrinter::printScanSummary(summary);
    logger_.info("Scan completed. " + summary.toLogLine());
}

void ScanPipeline::handleFile(
    const FileProcessor& processor,
    const fs::path& file,
    const fs::path& root,
    std::int64_t signatures_last_modified,
    ScanSummary& summary)
{
    const fs::path relative = fs::relative(file, root);
    const std::string key = file.generic_string();

    std::error_code error;
    const std::uintmax_t size = fs::file_size(file, error);
    const fs::file_time_type write_time = fs::last_write_time(file, error);
    if (error) {
        ++summary.failed;
        logger_.warning("Could not read file: " + file.string());
        resume_manager_.fileCompleted(relative);
        return;
    }

    FileMetadata metadata;
    metadata.last_modified =
        static_cast<std::int64_t>(write_time.time_since_epoch().count());
    metadata.size = size;
    metadata.signatures_last_modified = signatures_last_modified;

    // Cache hit: file and signatures unchanged since last scan — reuse verdict.
    if (const std::optional<FileVerdict> cached =
            cache_manager_.cachedVerdict(key, metadata)) {
        ++summary.cached;
        if (*cached == FileVerdict::Malicious) {
            ++summary.malicious;
        }
        resume_manager_.fileCompleted(relative);
        return;
    }

    // Cache miss: scan the file.
    const std::unordered_set<std::size_t> matches = processor.process(file);
    ++summary.scanned;

    const FileVerdict verdict =
        matches.empty() ? FileVerdict::Clean : FileVerdict::Malicious;
    if (verdict == FileVerdict::Malicious) {
        ++summary.malicious;
        logger_.info("Malicious file detected: " + file.string());
        if (quarantine_manager_.quarantine(file, matchedSignatures(matches))) {
            ++summary.quarantined;
        }
    }

    // Record the fresh verdict so the next run can skip this file.
    cache_manager_.update(CacheEntry{
        .path = key,
        .metadata = metadata,
        .verdict = verdict});

    resume_manager_.fileCompleted(relative);
}

std::vector<std::string> ScanPipeline::matchedSignatures(
    const std::unordered_set<std::size_t>& matches) const
{
    const std::vector<std::string>& all = signature_loader_.signatures();

    std::vector<std::string> result;
    result.reserve(matches.size());
    for (const std::size_t index : matches) {
        if (index < all.size()) {
            result.push_back(all[index]);
        }
    }

    std::ranges::sort(result);
    return result;
}

void ScanPipeline::excludeSelf()
{
    std::error_code error;
    const fs::path base = fs::current_path(error);
    if (error) {
        return;
    }

    // Walk up to the project root (the directory holding .git); fall back to
    // the working directory if no repository marker is found.
    fs::path root = base;
    for (fs::path dir = base; !dir.empty(); dir = dir.parent_path()) {
        std::error_code exists_error;
        if (fs::exists(dir / ".git", exists_error) && !exists_error) {
            root = dir;
            break;
        }
        if (dir == dir.root_path()) {
            break;
        }
    }

    exclude_manager_.clearInternalExcludedPaths();
    exclude_manager_.addInternalExcludedPath(root);
}

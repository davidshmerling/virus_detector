#pragma once

#include "Cache/CacheManager.h"
#include "Exclude/ExcludeManager.h"
#include "Logger/Logger.h"
#include "Quarantine/QuarantineManager.h"
#include "Resume/JsonCheckpointRepository.h"
#include "Resume/ProgressTracker.h"
#include "Scanner/Automaton/AhoCorasick.h"
#include "Scanner/FileEnumerator/FileEnumerator.h"
#include "Scanner/FileProcessor.h"
#include "Scanner/ScanSummary.h"
#include "Scanner/SignatureManager/SignatureManager.h"
#include "ThreadPool/ThreadPool.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

class Scanner {
public:
    Scanner(
        std::string signatures_file,
        std::string exclude_file,
        QuarantineManager& quarantine_manager,
        Logger& logger,
        std::size_t worker_count = 0,
        std::size_t queue_capacity = 256);

    bool initialize();
    ScanSummary scanRoot(const std::filesystem::path& root);

private:
    bool prepareProgress(
        const std::filesystem::path& root,
        ScanCheckpoint& checkpoint,
        bool& resuming);

    bool submitFile(
        const std::filesystem::path& file_path,
        const std::filesystem::path& root,
        ScanSummary& summary);

    bool enumerateFiles(
        const std::filesystem::path& root,
        bool resuming,
        const ScanCheckpoint& checkpoint,
        ScanSummary& summary);

    void finishScan(
        const std::filesystem::path& root,
        bool enumeration_ok,
        ScanSummary& summary);

    bool loadRunningCheckpoint(
        const std::filesystem::path& root,
        ScanCheckpoint& checkpoint) const;

    bool normalizeRoot(
        const std::filesystem::path& root,
        std::filesystem::path& normalized) const;

    SignatureManager signature_manager_;
    AhoCorasick automaton_;
    ExcludeManager exclude_manager_;
    FileEnumerator file_enumerator_;
    JsonCheckpointRepository checkpoint_repository_;
    ProgressTracker progress_tracker_;
    CacheManager cache_manager_;
    QuarantineManager& quarantine_manager_;
    ThreadPool thread_pool_;
    Logger& logger_;

    std::unique_ptr<FileProcessor> file_processor_;

    std::int64_t signatures_last_modified_ = 0;
    bool initialized_ = false;
};

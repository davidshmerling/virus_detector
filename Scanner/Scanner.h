#pragma once

#include "Cache/CacheManager.h"
#include "Exclude/ExcludeManager.h"
#include "Logger/Logger.h"
#include "Quarantine/QuarantineManager.h"
#include "Resume/JsonCheckpointRepository.h"
#include "Resume/ProgressTracker.h"
#include "Scanner/Automaton/AhoCorasick.h"
#include "Scanner/FileEnumerator/FileEnumerator.h"
#include "Scanner/FileScanner/FileScanner.h"
#include "Scanner/ScanSummary.h"
#include "Scanner/SignatureManager/SignatureManager.h"
#include "ThreadPool/ThreadPool.h"

#include <cstdint>
#include <filesystem>
#include <string>

class Scanner {
public:
    Scanner(
        std::string signatures_file,
        std::string exclude_file,
        std::filesystem::path quarantine_directory,
        Logger& logger,
        std::size_t worker_count = 4,
        std::size_t queue_capacity = 256);

    bool initialize();
    ScanSummary scanRoot(const std::filesystem::path& root);

private:
    void processFile(
        const std::filesystem::path& file_path,
        ScanSummary& summary);

    bool loadRunningCheckpoint(
        const std::filesystem::path& root,
        ScanCheckpoint& checkpoint) const;

    static std::filesystem::path normalizeRoot(
        const std::filesystem::path& root);

    SignatureManager signature_manager_;
    AhoCorasick automaton_;
    ExcludeManager exclude_manager_;
    FileEnumerator file_enumerator_;
    JsonCheckpointRepository checkpoint_repository_;
    ProgressTracker progress_tracker_;
    CacheManager cache_manager_;
    QuarantineManager quarantine_manager_;
    ThreadPool thread_pool_;
    Logger& logger_;

    std::int64_t signatures_last_modified_ = 0;
    bool initialized_ = false;
};

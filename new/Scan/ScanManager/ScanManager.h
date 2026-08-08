#pragma once

#include "Scan/AutomatonBuilder/AutomatonBuilder.h"
#include "Cache/CacheManager.h"
#include "Common/ScanSummary.h"
#include "Exclude/ExcludeManager.h"
#include "Logger/Logger.h"
#include "Quarantine/QuarantineManager.h"
#include "Resume/ResumeManager.h"
#include "Signature/SignatureLoader.h"

#include <cstdint>
#include <filesystem>

class FileProcessor;  // used only by reference in the per-file helper

// Wires the whole scan pipeline together for one run:
//   load signatures → build automaton → prepare resume → walk the tree →
//   enqueue each file → workers scan → wait → flush.
// It performs none of the heavy lifting itself (no chunk reading, no automaton
// search, no DFS, no SQLite); it only connects the classes that do. Open this
// one file to see what happens from the start of a scan to the end.
class ScanManager {
public:
    ScanManager();

    void scan(const std::filesystem::path& root);

private:
    // Runs on a worker thread for one discovered file: cache lookup, scan on
    // miss, cache update, and summary/resume bookkeeping.
    void handleFile(
        const FileProcessor& processor,
        const std::filesystem::path& file,
        const std::filesystem::path& root,
        std::int64_t signatures_last_modified,
        ScanSummary& summary);

    Logger logger_;
    SignatureLoader signature_loader_;
    AutomatonBuilder automaton_builder_;
    ExcludeManager exclude_manager_;
    CacheManager cache_manager_;
    ResumeManager resume_manager_;
    QuarantineManager quarantine_manager_;
};

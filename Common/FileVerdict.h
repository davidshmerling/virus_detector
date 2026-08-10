#pragma once

// Verdict produced by a scan (or reused from cache) for one file.
enum class FileVerdict {
    Clean,
    Malicious,
    Error
};

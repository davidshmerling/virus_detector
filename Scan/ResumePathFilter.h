#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

// What the sorted DFS should do with one directory entry, given a resume
// checkpoint:
//   Skip         — the entry sorts before the checkpoint; already processed.
//   FollowPath   — the entry is a directory that lies on the checkpoint path
//                  and must be descended to reach the checkpoint.
//   ScanNormally — the entry is at or past the checkpoint; scan it in full.
enum class ResumeDecision {
    Skip,
    FollowPath,
    ScanNormally,
};

// Holds a resume checkpoint (a path split into its parts) and, for each entry
// the walker meets, decides how it relates to that checkpoint. This is all the
// resume knowledge the DFS needs; the walker itself only has to walk.
//
// An empty resume_from means "no checkpoint": decide() always returns
// ScanNormally, i.e. a full walk.
class ResumePathFilter {
public:
    // Inactive filter: no checkpoint at all, so decide() is always
    // ScanNormally. Use this when entering a fresh subtree.
    ResumePathFilter() = default;

    explicit ResumePathFilter(const std::filesystem::path& resume_from);

    // name  — the entry's filename at this DFS level.
    // depth — how deep the walk is (0 at the scan root's children).
    ResumeDecision decide(const std::string& name, std::size_t depth) const;

    // True when a checkpoint is set. An inactive filter never skips or follows.
    bool active() const;

private:
    std::vector<std::string> parts_;
};

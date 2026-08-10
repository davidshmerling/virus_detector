#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

// Decision the sorted DFS should take for one directory entry given a resume
// checkpoint:
//   SkipEntry                 — sorts before the checkpoint; already processed.
//   ContinueTowardResumePoint — directory on the checkpoint path; descend to
//                               reach the checkpoint.
//   TraverseNormally          — at or past the checkpoint; traverse in full.
enum class ResumeDecision {
    SkipEntry,
    ContinueTowardResumePoint,
    TraverseNormally,
};

// Holds a resume checkpoint (a path split into its parts) and, for each entry
// the walker meets, decides how it relates to that checkpoint. This is all the
// resume knowledge the DFS needs; the walker itself only has to walk.
//
// An empty `resume_from` means no checkpoint: decide() always returns
// TraverseNormally (a full walk).
class ResumePathFilter {
public:
    // Builds an inactive filter with no checkpoint, so decide() always returns
    // TraverseNormally. Use this when entering a fresh subtree.
    ResumePathFilter() = default;

    // Builds a filter from the resume checkpoint path `resume_from`.
    explicit ResumePathFilter(const std::filesystem::path& resume_from);

    // Returns how the DFS should treat an entry named `name` at `depth`.
    // `name` is the entry's filename at this DFS level.
    // `depth` is how deep the walk is (0 at the scan root's children).
    ResumeDecision decide(const std::string& name, std::size_t depth) const;

    // Returns true when a checkpoint is set. An inactive filter never skips or
    // follows.
    bool active() const;

private:
    std::vector<std::string> parts_;
};

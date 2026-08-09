#include "Resume/ResumePathFilter.h"

namespace fs = std::filesystem;

ResumePathFilter::ResumePathFilter(const fs::path& resume_from)
{
    // Iterating an empty path yields no parts, which leaves parts_ empty and
    // turns decide() into an unconditional ScanNormally (a full walk).
    for (const fs::path& part : resume_from) {
        parts_.push_back(part.generic_string());
    }
}

bool ResumePathFilter::active() const
{
    return !parts_.empty();
}

ResumeDecision ResumePathFilter::decide(
    const std::string& name,
    std::size_t depth) const
{
    // No checkpoint, or deeper than the checkpoint path: nothing left to skip
    // or follow.
    if (!active() || depth >= parts_.size()) {
        return ResumeDecision::ScanNormally;
    }

    const std::string& target = parts_[depth];

    // Children are visited in sorted order, so anything before the checkpoint
    // name at this level was already handled on the previous run.
    if (name < target) {
        return ResumeDecision::Skip;
    }

    // Exactly on the checkpoint path, with more parts below: descend to reach
    // the checkpoint. (When this is the last part, it is the checkpoint entry
    // itself and falls through to ScanNormally so it is re-processed.)
    if (name == target && depth + 1 < parts_.size()) {
        return ResumeDecision::FollowPath;
    }

    return ResumeDecision::ScanNormally;
}

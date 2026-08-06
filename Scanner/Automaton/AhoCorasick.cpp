#include "Scanner/Automaton/AhoCorasick.h"

#include <queue>
#include <ranges>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <x86intrin.h>
#define AV_HAS_TSC 1
#elif defined(__aarch64__)
#define AV_HAS_TSC 1
#else
#define AV_HAS_TSC 0
#endif

namespace {

[[nodiscard]] inline std::uint64_t readTsc() noexcept
{
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    return __rdtsc();
#elif defined(__aarch64__)
    std::uint64_t value = 0;
    asm volatile("mrs %0, cntvct_el0" : "=r"(value));
    return value;
#else
    return 0;
#endif
}

}  // namespace

void AhoCorasick::clear()
{
    nodes_.clear();
    signature_count_ = 0;
    built_ = false;
}

void AhoCorasick::build(const std::vector<std::string>& signatures)
{
    clear();
    nodes_.emplace_back();  // root

    for (const auto& [index, signature] : signatures | std::views::enumerate) {
        if (signature.empty()) {
            continue;
        }

        addPattern(signature, static_cast<std::size_t>(index));
        ++signature_count_;
    }

    buildFailureLinks();
    built_ = true;
}

void AhoCorasick::addPattern(
    const std::string& pattern,
    std::size_t signatureIndex)
{
    int state = 0;

    for (const unsigned char byte : pattern) {
        int nextState = nodes_[state].next[byte];

        if (nextState == -1) {
            // Capture indexes before emplace_back — reallocation
            // would invalidate any reference into nodes_.
            nextState = static_cast<int>(nodes_.size());
            nodes_.emplace_back();
            nodes_[state].next[byte] = nextState;
        }

        state = nextState;
    }

    nodes_[state].output.push_back(signatureIndex);
}

void AhoCorasick::initializeRootTransitions(std::queue<int>& pending)
{
    for (int& nextState : nodes_[0].next) {
        if (nextState == -1) {
            nextState = 0;
        } else {
            nodes_[nextState].failure_link = 0;
            pending.push(nextState);
        }
    }
}

void AhoCorasick::bfsFailureLinks(std::queue<int>& pending)
{
    while (!pending.empty()) {
        const int state = pending.front();
        pending.pop();

        for (int byte = 0; byte < 256; ++byte) {
            int& nextState = nodes_[state].next[byte];

            if (nextState == -1) {
                nextState =
                    nodes_[nodes_[state].failure_link].next[byte];
                continue;
            }

            const int failureState =
                nodes_[nodes_[state].failure_link].next[byte];

            nodes_[nextState].failure_link = failureState;

            const auto& inheritedOutput = nodes_[failureState].output;
            nodes_[nextState].output.insert(
                nodes_[nextState].output.end(),
                inheritedOutput.begin(),
                inheritedOutput.end());

            pending.push(nextState);
        }
    }
}

void AhoCorasick::buildFailureLinks()
{
    std::queue<int> pending;
    initializeRootTransitions(pending);
    bfsFailureLinks(pending);
}

bool AhoCorasick::isBuilt() const
{
    return built_;
}

std::size_t AhoCorasick::nodeCount() const
{
    return nodes_.size();
}

std::size_t AhoCorasick::signatureCount() const
{
    return signature_count_;
}

void AhoCorasick::scanChunk(
    std::span<const char> data,
    int& state,
    std::unordered_set<std::size_t>& matched_indices,
    AutomatonScanBreakdown* breakdown) const
{
#if AV_HAS_TSC
    if (breakdown != nullptr) {
        for (const char byte_value : data) {
            const auto byte = static_cast<unsigned char>(byte_value);

            const std::uint64_t t0 = readTsc();
            state = nodes_[state].next[byte];
            const std::uint64_t t1 = readTsc();
            breakdown->transition_cycles += t1 - t0;

            const std::uint64_t t2 = readTsc();
            const auto& output = nodes_[state].output;
            const bool has_output = !output.empty();
            const std::uint64_t t3 = readTsc();
            breakdown->output_check_cycles += t3 - t2;

            ++breakdown->bytes_scanned;

            if (!has_output) {
                continue;
            }

            ++breakdown->output_hits;

            const std::uint64_t t4 = readTsc();
            for (const std::size_t index : output) {
                matched_indices.insert(index);
                ++breakdown->match_inserts;
            }
            const std::uint64_t t5 = readTsc();
            breakdown->match_handle_cycles += t5 - t4;
        }
        return;
    }
#else
    (void)breakdown;
#endif

    for (const char byte_value : data) {
        const auto byte = static_cast<unsigned char>(byte_value);
        state = nodes_[state].next[byte];

        const auto& output = nodes_[state].output;
        if (output.empty()) {
            continue;
        }

        for (const std::size_t index : output) {
            matched_indices.insert(index);
        }
    }
}

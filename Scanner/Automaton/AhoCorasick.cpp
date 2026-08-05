#include "Scanner/Automaton/AhoCorasick.h"

#include <queue>
#include <ranges>

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

int AhoCorasick::nextState(int current_state, unsigned char byte) const
{
    // After buildFailureLinks(), every next[byte] is filled.
    return nodes_[current_state].next[byte];
}

bool AhoCorasick::hasMatch(int state) const
{
    return !nodes_[state].output.empty();
}

const std::vector<std::size_t>& AhoCorasick::matches(int state) const
{
    return nodes_[state].output;
}

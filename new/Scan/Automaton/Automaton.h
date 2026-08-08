#pragma once

#include <array>
#include <cstddef>
#include <vector>

// One state in the Aho-Corasick automaton.
// Alphabet is all bytes: 0-255 (good for binary files).
struct AutomatonNode {
    // next[b] = next state for byte b, or -1 before build finishes.
    std::array<int, 256> next{};

    // Where to go when next[b] is missing (used while building).
    int failure_link = 0;

    // Signature indexes that end in this state (own + inherited).
    std::vector<std::size_t> output;

    AutomatonNode() {
        next.fill(-1);
    }
};

// Holds the nodes and transitions of the built automaton. Read-only after
// AutomatonBuilder fills it, so it is safe to share between threads.
// Scanning (AutomatonScanner) only reads through the const accessors below.
class Automaton {
public:
    bool isBuilt() const { return built_; }
    std::size_t nodeCount() const { return nodes_.size(); }
    std::size_t signatureCount() const { return signature_count_; }

    // Every next[b] is filled after the build finishes.
    int step(int state, unsigned char byte) const
    {
        return nodes_[static_cast<std::size_t>(state)].next[byte];
    }

    const std::vector<std::size_t>& outputs(int state) const
    {
        return nodes_[static_cast<std::size_t>(state)].output;
    }

private:
    // Only the builder may populate the internals.
    friend class AutomatonBuilder;

    std::vector<AutomatonNode> nodes_;
    std::size_t signature_count_ = 0;
    bool built_ = false;
};

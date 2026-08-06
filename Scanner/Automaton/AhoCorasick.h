#pragma once

#include <array>
#include <cstddef>
#include <queue>
#include <span>
#include <string>
#include <unordered_set>
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

// Builds a Trie + failure links from all signature strings.
// After build(), the automaton is read-only and safe to share between threads
// as long as search functions are const.
class AhoCorasick {
public:
    // Build the automaton once from the loaded signatures.
    void build(const std::vector<std::string>& signatures);

    // Scan a full chunk. Updates state across calls so signatures that
    // cross chunk boundaries are still found. Appends every matching
    // signature index into matched_indices (unique via the set).
    void scanChunk(
        std::span<const char> data,
        int& state,
        std::unordered_set<std::size_t>& matched_indices) const;

    bool isBuilt() const;
    std::size_t nodeCount() const;
    std::size_t signatureCount() const;

private:
    void clear();
    void addPattern(const std::string& pattern, std::size_t signature_index);
    void buildFailureLinks();
    void initializeRootTransitions(std::queue<int>& pending);
    void bfsFailureLinks(std::queue<int>& pending);

    std::vector<AutomatonNode> nodes_;
    std::size_t signature_count_ = 0;
    bool built_ = false;
};

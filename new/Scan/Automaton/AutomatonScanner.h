#pragma once

#include "Scan/Automaton/Automaton.h"

#include <cstddef>
#include <span>
#include <unordered_set>

// Scans bytes that are already in memory. Knows nothing about files:
// FileChunkReader does the reading and hands over one chunk at a time.
// The automaton is shared and never mutated, so many scanners can run in
// parallel; only the per-file `state` carried between chunks is per scan.
class AutomatonScanner {
public:
    explicit AutomatonScanner(const Automaton& automaton);

    // Scan a full chunk. Updates state across calls so signatures that
    // cross chunk boundaries are still found. Inserts every matching
    // signature index into matched_indices.
    void scanChunk(
        std::span<const char> data,
        int& state,
        std::unordered_set<std::size_t>& matched_indices) const;

private:
    const Automaton& automaton_;
};

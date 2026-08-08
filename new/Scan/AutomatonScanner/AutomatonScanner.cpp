#include "Scan/AutomatonScanner/AutomatonScanner.h"

AutomatonScanner::AutomatonScanner(const Automaton& automaton)
    : automaton_(automaton)
{
}

void AutomatonScanner::scanChunk(
    std::span<const char> data,
    int& state,
    std::unordered_set<std::size_t>& matched_indices) const
{
    for (const char byte_value : data) {
        const auto byte = static_cast<unsigned char>(byte_value);
        state = automaton_.step(state, byte);

        for (const std::size_t index : automaton_.outputs(state)) {
            matched_indices.insert(index);
        }
    }
}

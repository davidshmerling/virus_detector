#pragma once

#include "Scan/Automaton/Automaton.h"

#include <cstddef>
#include <string>
#include <vector>

// Builds the automaton once at startup from signatures into a ready,
// read-only Automaton. After build(), the returned Automaton is never mutated.
class AutomatonBuilder {
public:
    // Builds an automaton from `signatures`. A signature's position in the
    // list is used as its match index.
    Automaton build(const std::vector<std::string>& signatures) const;

private:
    // Inserts one signature into the trie under construction.
    void addSignature(
        Automaton& automaton,
        const std::string& signature,
        std::size_t signature_index) const;

    // Computes failure links and merges output lists (Aho-Corasick).
    void buildFailureLinks(Automaton& automaton) const;
};

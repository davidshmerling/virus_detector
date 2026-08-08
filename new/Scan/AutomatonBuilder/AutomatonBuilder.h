#pragma once

#include "Scan/Automaton/Automaton.h"

#include <cstddef>
#include <string>
#include <vector>

// Builds the automaton once at startup: signatures → ready read-only Automaton.
// After build(), the returned Automaton is never mutated.
class AutomatonBuilder {
public:
    // Build from a list of signature strings. A signature's position in the
    // list is used as its match index.
    Automaton build(const std::vector<std::string>& signatures) const;

private:
    void addSignature(
        Automaton& automaton,
        const std::string& signature,
        std::size_t signature_index) const;
    void buildFailureLinks(Automaton& automaton) const;
};

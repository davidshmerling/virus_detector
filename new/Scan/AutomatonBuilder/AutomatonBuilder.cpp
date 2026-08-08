#include "Scan/AutomatonBuilder/AutomatonBuilder.h"

#include <queue>

Automaton AutomatonBuilder::build(
    const std::vector<std::string>& signatures) const
{
    Automaton automaton;
    automaton.nodes_.emplace_back();  // root

    for (std::size_t i = 0; i < signatures.size(); ++i) {
        if (signatures[i].empty()) {
            continue;
        }

        addSignature(automaton, signatures[i], i);
        ++automaton.signature_count_;
    }

    buildFailureLinks(automaton);
    automaton.built_ = true;

    return automaton;
}

void AutomatonBuilder::addSignature(
    Automaton& automaton,
    const std::string& signature,
    std::size_t signature_index) const
{
    if (signature.empty()) {
        return;
    }

    int state = 0;

    for (const unsigned char byte : signature) {
        int next_state = automaton.nodes_[state].next[byte];

        if (next_state == -1) {
            next_state = static_cast<int>(automaton.nodes_.size());
            automaton.nodes_.emplace_back();
            automaton.nodes_[state].next[byte] = next_state;
        }

        state = next_state;
    }

    automaton.nodes_[state].output.push_back(signature_index);
}

void AutomatonBuilder::buildFailureLinks(Automaton& automaton) const
{
    std::vector<AutomatonNode>& nodes = automaton.nodes_;
    std::queue<int> pending;

    // Direct children of root fail back to root; missing root transitions
    // loop back to root.
    for (int& next_state : nodes[0].next) {
        if (next_state == -1) {
            next_state = 0;
        } else {
            nodes[next_state].failure_link = 0;
            pending.push(next_state);
        }
    }

    // BFS to build the remaining failure links.
    while (!pending.empty()) {
        const int state = pending.front();
        pending.pop();

        const int failure = nodes[state].failure_link;

        for (int byte = 0; byte < 256; ++byte) {
            int& next_state = nodes[state].next[byte];

            if (next_state == -1) {
                next_state = nodes[failure].next[byte];
                continue;
            }

            const int next_failure = nodes[failure].next[byte];

            nodes[next_state].failure_link = next_failure;

            const auto& inherited = nodes[next_failure].output;

            nodes[next_state].output.insert(
                nodes[next_state].output.end(),
                inherited.begin(),
                inherited.end());

            pending.push(next_state);
        }
    }
}

#include "eke_dx_wire/topology/topology_semantic_resolver.hpp"

#include <cassert>

using namespace eke::dx::wire;

static TopologyNode node(
    const char* id,
    TopologyNodeType type,
    bool connected) {
    TopologyNode n;
    n.id = id;
    n.type = type;
    n.electrically_connective = connected;
    return n;
}

int main() {
    {
        auto result = TopologySemanticResolver().resolve(
            {node("x", TopologyNodeType::Crossing, false)},
            {{"x", TopologyConnectionState::Connected,
              ConfidenceClass::High, "junction-marker"}});

        assert(result.size() == 1);
        assert(result.front().type == TopologyNodeType::Splice);
        assert(result.front().electrically_connective);
    }

    {
        auto result = TopologySemanticResolver().resolve(
            {node("s", TopologyNodeType::Splice, true)},
            {{"s", TopologyConnectionState::NotConnected,
              ConfidenceClass::High, "explicit-crossing"}});

        assert(result.front().type == TopologyNodeType::Crossing);
        assert(!result.front().electrically_connective);
    }

    {
        auto result = TopologySemanticResolver().resolve(
            {node("x", TopologyNodeType::Crossing, false)},
            {{"x", TopologyConnectionState::Connected,
              ConfidenceClass::Low, "weak-source"},
             {"x", TopologyConnectionState::NotConnected,
              ConfidenceClass::High, "strong-source"}});

        assert(result.front().type == TopologyNodeType::Crossing);
        assert(!result.front().electrically_connective);
    }

    {
        // No semantic evidence means geometry-derived topology is preserved.
        auto result = TopologySemanticResolver().resolve(
            {node("x", TopologyNodeType::Crossing, false)}, {});

        assert(result.front().type == TopologyNodeType::Crossing);
        assert(!result.front().electrically_connective);
    }

    return 0;
}

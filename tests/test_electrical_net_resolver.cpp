#include "eke_dx_wire/topology/electrical_net_resolver.hpp"

#include <cassert>

using namespace eke::dx::wire;

static ConductorSegment segment(const char* id, Point2D a, Point2D b) {
    ConductorSegment s;
    s.id = id;
    s.geometry = {a, b};
    return s;
}

static EndpointCandidate endpoint(
    const char* id,
    const char* node,
    EndpointKind kind,
    TerminalRole role = TerminalRole::Unknown) {
    EndpointCandidate e;
    e.id = id;
    e.node_id = node;
    e.kind = kind;
    e.terminal_role = role;
    e.confidence = ConfidenceClass::High;
    return e;
}

int main() {
    std::vector<TopologyNode> nodes{
        {"g", {0, 0}, TopologyNodeType::ConductorEnd, true},
        {"s", {10, 0}, TopologyNodeType::Splice, true},
        {"a", {20, 0}, TopologyNodeType::ConductorEnd, true},
        {"b", {10, 10}, TopologyNodeType::ConductorEnd, true}
    };

    std::vector<TopologyEdge> edges{
        {"e1", "g", "s", "c1"},
        {"e2", "s", "a", "c2"},
        {"e3", "s", "b", "c3"}
    };

    const auto result = ElectricalNetResolver().resolve(
        nodes,
        edges,
        {
            endpoint("G", "g", EndpointKind::Ground,
                     TerminalRole::GroundTerminal),
            endpoint("A", "a", EndpointKind::ComponentTerminal),
            endpoint("B", "b", EndpointKind::ComponentTerminal)
        },
        {
            segment("c1", {0, 0}, {10, 0}),
            segment("c2", {10, 0}, {20, 0}),
            segment("c3", {10, 0}, {10, 10})
        },
        {},
        "ap-wire-022");

    assert(result.nets.size() == 1);
    assert(result.nets.front().role == DistributionRole::Ground);
    assert(result.nets.front().anchor_endpoint == "G");
    assert(result.nets.front().confidence == ConfidenceClass::High);
    assert(result.nets.front().endpoint_ids.size() == 3);
    assert(result.nets.front().splice_node_ids.size() == 1);
    assert(result.wires.size() == 2);
    assert(result.unresolved_net_ids.empty());

    const auto power_result = ElectricalNetResolver().resolve(
        {
            {"p", {0, 0}, TopologyNodeType::ConductorEnd, true},
            {"s", {10, 0}, TopologyNodeType::Splice, true},
            {"a", {20, 0}, TopologyNodeType::ConductorEnd, true},
            {"b", {10, 10}, TopologyNodeType::ConductorEnd, true}
        },
        {
            {"e1", "p", "s", "c1"},
            {"e2", "s", "a", "c2"},
            {"e3", "s", "b", "c3"}
        },
        {
            endpoint("P", "p", EndpointKind::ComponentTerminal),
            endpoint("A", "a", EndpointKind::ComponentTerminal),
            endpoint("B", "b", EndpointKind::ComponentTerminal)
        },
        {
            segment("c1", {0, 0}, {10, 0}),
            segment("c2", {10, 0}, {20, 0}),
            segment("c3", {10, 0}, {10, 10})
        },
        {{"P", DistributionRole::PowerFeed,
          ConfidenceClass::High, "ap-wire-022-fixture"}},
        "ap-wire-022-power");

    assert(power_result.nets.size() == 1);
    assert(power_result.nets.front().role == DistributionRole::PowerFeed);
    assert(power_result.nets.front().anchor_endpoint == "P");
    assert(power_result.nets.front().confidence == ConfidenceClass::High);

    return 0;
}

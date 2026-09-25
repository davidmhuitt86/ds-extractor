#include "eke_dx_wire/topology/electrical_net_resolver.hpp"

#include <algorithm>
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

    // AP-DIAG-FIX-002 / TEST 4: two ConnectorTerminal endpoints that share
    // the same owning connector (component_id) but sit on two structurally
    // disconnected, independently Ground-anchored trees must resolve to two
    // independent electrical nets - a shared connector is never, by itself,
    // electrical connectivity. Net resolution here sees only nodes/edges/
    // segments; it has no notion of "connector" at all (confirmed by
    // inspection: neither DistributionDecomposer nor ElectricalNetResolver
    // reference Connector/ConnectorTerminal anywhere), so this also
    // demonstrates that introducing connector representation elsewhere
    // cannot leak into net decomposition.
    {
        std::vector<TopologyNode> connector_nodes{
            {"gnd1", {0, 0}, TopologyNodeType::ConductorEnd, true},
            {"j1-pin1", {10, 0}, TopologyNodeType::ConductorEnd, true},
            {"gnd2", {0, 20}, TopologyNodeType::ConductorEnd, true},
            {"j1-pin2", {10, 20}, TopologyNodeType::ConductorEnd, true}
        };
        std::vector<TopologyEdge> connector_edges{
            {"e1", "gnd1", "j1-pin1", "c1"},
            {"e2", "gnd2", "j1-pin2", "c2"}
        };

        EndpointCandidate gnd1 = endpoint(
            "GND1", "gnd1", EndpointKind::Ground, TerminalRole::GroundTerminal);
        EndpointCandidate pin1 = endpoint(
            "PIN1", "j1-pin1", EndpointKind::ConnectorTerminal,
            TerminalRole::ConnectorTerminal);
        pin1.component_id = "connector-j1";
        EndpointCandidate gnd2 = endpoint(
            "GND2", "gnd2", EndpointKind::Ground, TerminalRole::GroundTerminal);
        EndpointCandidate pin2 = endpoint(
            "PIN2", "j1-pin2", EndpointKind::ConnectorTerminal,
            TerminalRole::ConnectorTerminal);
        pin2.component_id = "connector-j1";

        const auto connector_result = ElectricalNetResolver().resolve(
            connector_nodes,
            connector_edges,
            {gnd1, pin1, gnd2, pin2},
            {
                segment("c1", {0, 0}, {10, 0}),
                segment("c2", {0, 20}, {10, 20})
            },
            {},
            "ap-diag-fix-002-connector-independence");

        assert(connector_result.nets.size() == 2);
        for (const auto& net : connector_result.nets) {
            assert(net.endpoint_ids.size() == 2);
            const bool is_pin1_net =
                std::find(
                    net.endpoint_ids.begin(), net.endpoint_ids.end(),
                    "PIN1") != net.endpoint_ids.end();
            const bool is_pin2_net =
                std::find(
                    net.endpoint_ids.begin(), net.endpoint_ids.end(),
                    "PIN2") != net.endpoint_ids.end();
            // Never both in the same net, and never neither.
            assert(is_pin1_net != is_pin2_net);
        }
    }

    return 0;
}

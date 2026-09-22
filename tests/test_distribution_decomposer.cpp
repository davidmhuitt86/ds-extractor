#include "eke_dx_wire/topology/distribution_decomposer.hpp"

#include <cassert>
#include <set>

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
    EndpointKind kind) {
    EndpointCandidate e;
    e.id = id;
    e.node_id = node;
    e.kind = kind;
    e.confidence = ConfidenceClass::High;
    return e;
}

int main() {
    {
        // Ground distribution:
        //             B
        //             |
        // Ground A --- S --- C
        std::vector<TopologyNode> nodes{
            {"a", {0, 0}, TopologyNodeType::ConductorEnd, true},
            {"s", {10, 0}, TopologyNodeType::Splice, true},
            {"b", {10, 10}, TopologyNodeType::ConductorEnd, true},
            {"c", {20, 0}, TopologyNodeType::ConductorEnd, true}
        };

        std::vector<TopologyEdge> edges{
            {"e1", "a", "s", "s1"},
            {"e2", "s", "b", "s2"},
            {"e3", "s", "c", "s3"}
        };

        const auto result = DistributionDecomposer().decompose(
            nodes,
            edges,
            {endpoint("G", "a", EndpointKind::Ground),
             endpoint("B", "b", EndpointKind::ComponentTerminal),
             endpoint("C", "c", EndpointKind::ComponentTerminal)},
            {segment("s1", {0, 0}, {10, 0}),
             segment("s2", {10, 0}, {10, 10}),
             segment("s3", {10, 0}, {20, 0})},
            "fixture");

        assert(result.nets.size() == 1);
        assert(result.nets.front().anchor_endpoint == "G");
        assert(result.nets.front().role == DistributionRole::Ground);
        assert(result.wires.size() == 2);

        for (const auto& wire : result.wires) {
            assert(wire.start_endpoint == "G");
            assert(wire.topology_edges.size() == 2);
        }

        // Shared conductor geometry is intentionally repeated in the logical
        // endpoint-to-endpoint traces.
        std::set<std::string> ids;
        for (const auto& wire : result.wires) {
            ids.insert(wire.id);
        }
        assert(ids.size() == 2);
    }

    {
        // An anchored direct connection is a valid electrical net even
        // without a splice. The ground endpoint supplies the net identity.
        std::vector<TopologyNode> nodes{
            {"g", {0, 0}, TopologyNodeType::ConductorEnd, true},
            {"a", {20, 0}, TopologyNodeType::ConductorEnd, true}
        };

        std::vector<TopologyEdge> edges{
            {"e1", "g", "a", "s1"}
        };

        const auto result = DistributionDecomposer().decompose(
            nodes,
            edges,
            {endpoint("G", "g", EndpointKind::Ground),
             endpoint("A", "a", EndpointKind::ComponentTerminal)},
            {segment("s1", {0, 0}, {20, 0})},
            "fixture-direct-ground");

        assert(result.nets.size() == 1);
        assert(result.nets.front().anchor_endpoint == "G");
        assert(result.nets.front().role == DistributionRole::Ground);
        assert(result.wires.size() == 1);
        assert(result.wires.front().start_endpoint == "G");
        assert(result.wires.front().end_endpoint == "A");
    }

    {
        // A two-terminal component that happens to contain a splice is still
        // an ordinary endpoint-to-endpoint wire, not a distribution net.
        std::vector<TopologyNode> nodes{
            {"a", {0, 0}, TopologyNodeType::ConductorEnd, true},
            {"s", {10, 0}, TopologyNodeType::Splice, true},
            {"b", {20, 0}, TopologyNodeType::ConductorEnd, true}
        };

        std::vector<TopologyEdge> edges{
            {"e1", "a", "s", "s1"},
            {"e2", "s", "b", "s2"}
        };

        const auto result = DistributionDecomposer().decompose(
            nodes, edges,
            {endpoint("A", "a", EndpointKind::ComponentTerminal),
             endpoint("B", "b", EndpointKind::ComponentTerminal)},
            {segment("s1", {0,0}, {10,0}),
             segment("s2", {10,0}, {20,0})},
            "fixture-two-terminal-splice");

        assert(result.nets.empty());
        assert(result.wires.empty());
    }

    {
        // Without a semantic source/anchor, do not invent A->B vs A->C.
        std::vector<TopologyNode> nodes{
            {"a", {0, 0}, TopologyNodeType::ConductorEnd, true},
            {"s", {10, 0}, TopologyNodeType::Splice, true},
            {"b", {20, 0}, TopologyNodeType::ConductorEnd, true},
            {"c", {10, 10}, TopologyNodeType::ConductorEnd, true}
        };

        std::vector<TopologyEdge> edges{
            {"e1", "a", "s", "s1"},
            {"e2", "s", "b", "s2"},
            {"e3", "s", "c", "s3"}
        };

        const auto result = DistributionDecomposer().decompose(
            nodes,
            edges,
            {endpoint("A", "a", EndpointKind::ComponentTerminal),
             endpoint("B", "b", EndpointKind::ComponentTerminal),
             endpoint("C", "c", EndpointKind::ComponentTerminal)},
            {segment("s1", {0,0}, {10,0}),
             segment("s2", {10,0}, {20,0}),
             segment("s3", {10,0}, {10,10})},
            "fixture");

        assert(result.nets.size() == 1);
        assert(result.nets.front().anchor_endpoint.empty());
        assert(result.wires.empty());
        assert(result.nets.front().confidence == ConfidenceClass::Unresolved);
    }

    {
        // Cycles are not a distribution tree and remain unresolved.
        std::vector<TopologyNode> nodes{
            {"a", {0, 0}, TopologyNodeType::ConductorEnd, true},
            {"s1", {10, 0}, TopologyNodeType::Splice, true},
            {"s2", {20, 0}, TopologyNodeType::Splice, true},
            {"b", {30, 0}, TopologyNodeType::ConductorEnd, true}
        };

        std::vector<TopologyEdge> edges{
            {"e1", "a", "s1", "s1"},
            {"e2", "s1", "s2", "s2"},
            {"e3", "s2", "a", "s3"},
            {"e4", "s2", "b", "s4"}
        };

        const auto result = DistributionDecomposer().decompose(
            nodes,
            edges,
            {endpoint("G", "a", EndpointKind::Ground),
             endpoint("B", "b", EndpointKind::ComponentTerminal)},
            {segment("s1", {0,0}, {10,0}),
             segment("s2", {10,0}, {20,0}),
             segment("s3", {20,0}, {0,0}),
             segment("s4", {20,0}, {30,0})},
            "fixture");

        assert(result.nets.size() == 1);
        assert(result.wires.empty());
        assert(result.nets.front().confidence == ConfidenceClass::Unresolved);
    }

    return 0;
}

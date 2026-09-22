#include "eke_dx_wire/topology/wire_reconstructor.hpp"

#include <cassert>

using namespace eke::dx::wire;

static ConductorSegment segment(const char* id, Point2D a, Point2D b) {
    ConductorSegment s;
    s.id = id;
    s.geometry = {a, b};
    return s;
}

static EndpointCandidate endpoint(const char* id, const char* node) {
    EndpointCandidate e;
    e.id = id;
    e.node_id = node;
    e.kind = EndpointKind::GeometricConductorEnd;
    e.confidence = ConfidenceClass::Medium;
    return e;
}

int main() {
    {
        std::vector<TopologyNode> nodes{
            {"a", {0, 0}, TopologyNodeType::ConductorEnd, true},
            {"m", {10, 0}, TopologyNodeType::Continuation, true},
            {"b", {20, 0}, TopologyNodeType::ConductorEnd, true}
        };

        std::vector<TopologyEdge> edges{
            {"e1", "a", "m", "s1"},
            {"e2", "m", "b", "s2"}
        };

        std::vector<ConductorSegment> conductors{
            segment("s1", {0, 0}, {10, 0}),
            segment("s2", {10, 0}, {20, 0})
        };

        const auto result = WireReconstructor().reconstruct(
            nodes, edges,
            {endpoint("A", "a"), endpoint("B", "b")},
            conductors, "fixture");

        assert(result.wires.size() == 1);
        assert(result.wires.front().start_endpoint == "A");
        assert(result.wires.front().end_endpoint == "B");
        assert(result.wires.front().topology_edges.size() == 2);
        assert(result.wires.front().conductor_segments.size() == 2);
    }

    {
        // A splice is not a wire endpoint. The three branches are therefore
        // intentionally left for the distribution/decomposition stage.
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

        const auto result = WireReconstructor().reconstruct(
            nodes, edges,
            {endpoint("A", "a"), endpoint("B", "b"), endpoint("C", "c")},
            {segment("s1", {0,0}, {10,0}),
             segment("s2", {10,0}, {20,0}),
             segment("s3", {10,0}, {10,10})},
            "fixture");

        assert(result.wires.empty());
        assert(result.unresolved_nodes.size() == 1);
        assert(result.unresolved_nodes.front() == "s");
    }

    return 0;
}

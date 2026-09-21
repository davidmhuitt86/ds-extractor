#include "eke_dx_wire/topology/endpoint_reconstructor.hpp"

#include <algorithm>
#include <cassert>

using namespace eke::dx::wire;

static TopologyNode node(const char* id, TopologyNodeType type) {
    TopologyNode n;
    n.id = id;
    n.type = type;
    return n;
}

static TopologyEdge edge(
    const char* id, const char* from, const char* to) {
    TopologyEdge e;
    e.id = id;
    e.from_node = from;
    e.to_node = to;
    return e;
}

int main() {
    {
        std::vector<TopologyNode> nodes{
            node("a", TopologyNodeType::ConductorEnd),
            node("b", TopologyNodeType::Continuation),
            node("c", TopologyNodeType::ConductorEnd)
        };

        std::vector<TopologyEdge> edges{
            edge("e1", "a", "b"),
            edge("e2", "b", "c")
        };

        const auto result =
            EndpointReconstructor().reconstruct(
                nodes, edges, "fixture");

        assert(result.candidates.size() == 2);
        assert(std::all_of(
            result.candidates.begin(),
            result.candidates.end(),
            [](const EndpointCandidate& candidate) {
                return candidate.kind ==
                           EndpointKind::GeometricConductorEnd &&
                       candidate.confidence == ConfidenceClass::Low &&
                       candidate.incident_edges.size() == 1;
            }));
    }

    {
        std::vector<TopologyNode> nodes{
            node("a", TopologyNodeType::ConductorEnd),
            node("j", TopologyNodeType::Junction),
            node("b", TopologyNodeType::ConductorEnd),
            node("c", TopologyNodeType::ConductorEnd)
        };

        std::vector<TopologyEdge> edges{
            edge("e1", "a", "j"),
            edge("e2", "j", "b"),
            edge("e3", "j", "c")
        };

        const auto result =
            EndpointReconstructor().reconstruct(
                nodes, edges, "fixture");

        // The junction is not an endpoint. All three geometric conductor
        // ends remain candidates; wire identity is deliberately deferred.
        assert(result.candidates.size() == 3);
        assert(std::none_of(
            result.candidates.begin(),
            result.candidates.end(),
            [](const EndpointCandidate& candidate) {
                return candidate.node_id == "j";
            }));
    }

    return 0;
}

#include "eke_dx_wire/topology/endpoint_reconstructor.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace eke::dx::wire {

EndpointArtifacts EndpointReconstructor::reconstruct(
    const std::vector<TopologyNode>& nodes,
    const std::vector<TopologyEdge>& edges,
    const std::string& source_id,
    int page) const {

    EndpointArtifacts result;

    std::vector<std::vector<std::string>> incident(nodes.size());

    for (const auto& edge : edges) {
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            if (nodes[i].id == edge.from_node ||
                nodes[i].id == edge.to_node) {
                incident[i].push_back(edge.id);
            }
        }
    }

    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];

        // A geometric conductor end is evidence of a possible electrical
        // endpoint, not proof of one. Component/connector/splice/ground
        // semantics require additional source evidence that is not yet
        // available at this stage.
        if (node.type != TopologyNodeType::ConductorEnd ||
            incident[i].size() != 1) {
            continue;
        }

        EndpointCandidate candidate;
        std::ostringstream canonical;
        canonical << source_id << ":" << page << ":" << node.id;
        candidate.id = stable_id("endpoint-candidate", canonical.str());
        candidate.node_id = node.id;
        candidate.kind = EndpointKind::GeometricConductorEnd;
        candidate.confidence = ConfidenceClass::Low;
        candidate.incident_edges = incident[i];

        result.candidates.push_back(std::move(candidate));
    }

    std::sort(
        result.candidates.begin(),
        result.candidates.end(),
        [](const EndpointCandidate& a, const EndpointCandidate& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

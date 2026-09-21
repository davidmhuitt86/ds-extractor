#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

enum class EndpointKind {
    GeometricConductorEnd,
    ComponentTerminal,
    ConnectorTerminal,
    Splice,
    Ground,
    ExternalConnection,
    Unresolved
};

struct EndpointCandidate {
    std::string id;
    std::string node_id;
    EndpointKind kind = EndpointKind::Unresolved;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    std::vector<std::string> incident_edges;
};

struct EndpointArtifacts {
    std::vector<EndpointCandidate> candidates;
};

class EndpointReconstructor {
public:
    [[nodiscard]] EndpointArtifacts reconstruct(
        const std::vector<TopologyNode>& nodes,
        const std::vector<TopologyEdge>& edges,
        const std::string& source_id,
        int page = 0) const;
};

} // namespace eke::dx::wire

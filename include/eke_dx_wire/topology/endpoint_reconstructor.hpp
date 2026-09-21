#pragma once

#include "eke_dx_wire/core/model.hpp"

namespace eke::dx::wire {

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

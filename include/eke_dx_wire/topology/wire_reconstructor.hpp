#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

struct WireReconstructionConfig {
    bool require_semantic_endpoints = true;
};

struct WireReconstructionArtifacts {
    std::vector<Wire> wires;
    std::vector<std::string> unresolved_nodes;
};

class WireReconstructor {
public:
    explicit WireReconstructor(WireReconstructionConfig config = {});

    [[nodiscard]] WireReconstructionArtifacts reconstruct(
        const std::vector<TopologyNode>& nodes,
        const std::vector<TopologyEdge>& edges,
        const std::vector<EndpointCandidate>& endpoints,
        const std::vector<ConductorSegment>& conductors,
        const std::string& source_id,
        int page = 0) const;

private:
    WireReconstructionConfig config_;
};

} // namespace eke::dx::wire

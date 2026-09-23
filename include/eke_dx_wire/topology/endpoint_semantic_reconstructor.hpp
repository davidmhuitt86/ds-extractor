#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/topology/terminal_semantic_resolver.hpp"

#include <vector>

namespace eke::dx::wire {

struct EndpointSemanticReconstructionArtifacts {
    std::vector<EndpointCandidate> endpoints;
    std::vector<EndpointSemanticReconstruction> reconstructions;
};

class EndpointSemanticReconstructor {
public:
    [[nodiscard]] EndpointSemanticReconstructionArtifacts reconstruct(
        const std::vector<EndpointCandidate>& endpoints,
        const std::vector<TerminalSemanticEvidence>& evidence) const;
};

} // namespace eke::dx::wire

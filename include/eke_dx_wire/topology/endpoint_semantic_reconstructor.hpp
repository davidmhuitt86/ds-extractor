#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/topology/terminal_semantic_resolver.hpp"

#include <vector>

namespace eke::dx::wire {

enum class EndpointSemanticReconstructionStatus {
    Resolved,
    Conflicted,
    Unresolved
};

struct EndpointSemanticReconstruction {
    std::string id;
    std::string endpoint_id;
    std::string component_id;
    EndpointKind endpoint_kind = EndpointKind::Unresolved;
    TerminalRole terminal_role = TerminalRole::Unknown;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    EndpointSemanticReconstructionStatus status =
        EndpointSemanticReconstructionStatus::Unresolved;
    std::vector<std::string> evidence_component_ids;
};

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

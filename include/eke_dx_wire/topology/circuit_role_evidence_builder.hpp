#pragma once

#include "eke_dx_wire/topology/circuit_role_resolver.hpp"

#include <vector>

namespace eke::dx::wire {

/**
 * AP-NETWORK-002: converts already-established endpoint semantics into
 * circuit-role evidence. This stage does not infer circuit function from
 * topology, endpoint count, geometry, or component class alone.
 */
class CircuitRoleEvidenceBuilder {
public:
    [[nodiscard]] std::vector<CircuitRoleEvidence> build(
        const std::vector<EndpointCandidate>& endpoints) const;
};

} // namespace eke::dx::wire

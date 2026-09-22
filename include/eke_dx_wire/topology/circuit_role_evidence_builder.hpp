#pragma once

#include "eke_dx_wire/topology/circuit_role_resolver.hpp"

#include <vector>

namespace eke::dx::wire {

/**
 * AP-WIRE-005: converts already-established endpoint semantics into
 * circuit-role evidence. Explicit endpoint role, terminal/function labels,
 * and other semantic annotations may contribute evidence. This stage does
 * not infer circuit function from topology shape, endpoint count, geometry,
 * component class, or wire color alone.
 */
class CircuitRoleEvidenceBuilder {
public:
    [[nodiscard]] std::vector<CircuitRoleEvidence> build(
        const std::vector<EndpointCandidate>& endpoints) const;
};

} // namespace eke::dx::wire

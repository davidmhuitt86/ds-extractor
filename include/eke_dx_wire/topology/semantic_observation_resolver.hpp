#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/topology/circuit_role_resolver.hpp"

#include <vector>

namespace eke::dx::wire {

/**
 * AP-WIRE-012: resolves recognized semantic text observations through
 * deterministic spatial associations into circuit-role evidence.
 *
 * Only role-bearing text semantics (Ground, PowerFeed, SharedFunctionFeed)
 * associated with an endpoint can cross this boundary. Component-only
 * associations and non-role text never become circuit-role evidence.
 *
 * The resolver is deliberately conservative:
 * - the closest endpoint association wins for a text observation;
 * - ties between distinct endpoints remain unresolved;
 * - conflicting roles at the same endpoint remain unresolved;
 * - evidence confidence is the lower of semantic and spatial confidence.
 */
class SemanticObservationResolver {
public:
    [[nodiscard]] std::vector<CircuitRoleEvidence> resolve(
        const std::vector<TextSemanticEvidence>& semantic_evidence,
        const std::vector<SemanticAssociation>& associations) const;
};

} // namespace eke::dx::wire

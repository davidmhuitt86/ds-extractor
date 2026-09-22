#include "eke_dx_wire/topology/circuit_role_evidence_builder.hpp"

#include <algorithm>

namespace eke::dx::wire {

std::vector<CircuitRoleEvidence>
CircuitRoleEvidenceBuilder::build(
    const std::vector<EndpointCandidate>& endpoints) const {

    std::vector<CircuitRoleEvidence> result;

    for (const auto& endpoint : endpoints) {
        DistributionRole role = DistributionRole::Unknown;

        if (endpoint.kind == EndpointKind::Ground ||
            endpoint.terminal_role == TerminalRole::GroundTerminal) {
            role = DistributionRole::Ground;
        } else if (endpoint.terminal_role == TerminalRole::PowerSource) {
            role = DistributionRole::PowerFeed;
        }

        if (role == DistributionRole::Unknown ||
            endpoint.confidence == ConfidenceClass::Unresolved) {
            continue;
        }

        CircuitRoleEvidence evidence;
        evidence.endpoint_id = endpoint.id;
        evidence.role = role;
        evidence.confidence = endpoint.confidence;
        evidence.source = "endpoint-semantic";
        result.push_back(std::move(evidence));
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const CircuitRoleEvidence& a, const CircuitRoleEvidence& b) {
            if (a.endpoint_id != b.endpoint_id) {
                return a.endpoint_id < b.endpoint_id;
            }
            if (a.role != b.role) {
                return static_cast<int>(a.role) < static_cast<int>(b.role);
            }
            return a.source < b.source;
        });

    return result;
}

} // namespace eke::dx::wire

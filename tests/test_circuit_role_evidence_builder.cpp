#include "eke_dx_wire/topology/circuit_role_evidence_builder.hpp"

#include <cassert>

using namespace eke::dx::wire;

static EndpointCandidate endpoint(
    const char* id,
    EndpointKind kind,
    TerminalRole role,
    ConfidenceClass confidence) {

    EndpointCandidate e;
    e.id = id;
    e.kind = kind;
    e.terminal_role = role;
    e.confidence = confidence;
    return e;
}

int main() {
    const auto result = CircuitRoleEvidenceBuilder().build({
        endpoint(
            "G",
            EndpointKind::Ground,
            TerminalRole::GroundTerminal,
            ConfidenceClass::High),
        endpoint(
            "P",
            EndpointKind::ExternalConnection,
            TerminalRole::PowerSource,
            ConfidenceClass::High),
        endpoint(
            "S",
            EndpointKind::ComponentTerminal,
            TerminalRole::ComponentTerminal,
            ConfidenceClass::High),
        endpoint(
            "U",
            EndpointKind::GeometricConductorEnd,
            TerminalRole::Unknown,
            ConfidenceClass::Low),
        endpoint(
            "R",
            EndpointKind::Ground,
            TerminalRole::GroundTerminal,
            ConfidenceClass::Unresolved)
    });

    assert(result.size() == 2);
    assert(result[0].endpoint_id == "G");
    assert(result[0].role == DistributionRole::Ground);
    assert(result[1].endpoint_id == "P");
    assert(result[1].role == DistributionRole::PowerFeed);
    assert(result[0].source == "endpoint-semantic");
    assert(result[1].source == "endpoint-semantic");

    return 0;
}

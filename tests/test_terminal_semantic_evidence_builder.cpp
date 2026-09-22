#include "eke_dx_wire/topology/terminal_semantic_evidence_builder.hpp"

#include <cassert>

using namespace eke::dx::wire;

int main() {
    TerminalCandidate component;
    component.endpoint_id = "endpoint-1";
    component.component_candidate_id = "component-1";
    component.kind = TerminalCandidateKind::ComponentBoundary;
    component.confidence = ConfidenceClass::High;

    TerminalCandidate connector;
    connector.endpoint_id = "endpoint-2";
    connector.component_candidate_id = "connector-1";
    connector.kind = TerminalCandidateKind::ConnectorBoundary;
    connector.confidence = ConfidenceClass::Medium;

    TerminalCandidate ground;
    ground.endpoint_id = "endpoint-3";
    ground.component_candidate_id = "ground-1";
    ground.kind = TerminalCandidateKind::GroundConnection;
    ground.confidence = ConfidenceClass::High;

    const auto result =
        TerminalSemanticEvidenceBuilder().build(
            {component, connector, ground});

    assert(result.size() == 3);
    assert(result[0].endpoint_id == "endpoint-1");
    assert(result[0].endpoint_kind == EndpointKind::ComponentTerminal);
    assert(result[0].role == TerminalRole::ComponentTerminal);
    assert(result[1].endpoint_kind == EndpointKind::ConnectorTerminal);
    assert(result[1].role == TerminalRole::ConnectorTerminal);
    assert(result[2].endpoint_kind == EndpointKind::Ground);
    assert(result[2].role == TerminalRole::GroundTerminal);
    assert(result[2].confidence == ConfidenceClass::High);
    return 0;
}

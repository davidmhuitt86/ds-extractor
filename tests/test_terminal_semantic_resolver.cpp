#include "eke_dx_wire/topology/terminal_semantic_resolver.hpp"

#include <cassert>

using namespace eke::dx::wire;

int main() {
    EndpointCandidate geometric;
    geometric.id = "ep-1";
    geometric.node_id = "node-1";
    geometric.kind = EndpointKind::GeometricConductorEnd;
    geometric.confidence = ConfidenceClass::Low;

    TerminalSemanticEvidence evidence;
    evidence.endpoint_id = "ep-1";
    evidence.endpoint_kind = EndpointKind::ComponentTerminal;
    evidence.role = TerminalRole::ComponentTerminal;
    evidence.confidence = ConfidenceClass::High;
    evidence.component_id = "headlight";
    evidence.terminal_name = "LOW";
    evidence.function_label = "Headlight Lo Beam";
    evidence.wire_color = "W";

    const auto result =
        TerminalSemanticResolver().resolve({geometric}, {evidence});

    assert(result.size() == 1);
    assert(result.front().kind == EndpointKind::ComponentTerminal);
    assert(result.front().terminal_role == TerminalRole::ComponentTerminal);
    assert(result.front().component_id == "headlight");
    assert(result.front().terminal_name == "LOW");
    assert(result.front().function_label == "Headlight Lo Beam");
    assert(result.front().wire_color == "W");
    assert(result.front().confidence == ConfidenceClass::High);

    EndpointCandidate unresolved;
    unresolved.id = "ep-2";
    unresolved.node_id = "node-2";
    unresolved.kind = EndpointKind::GeometricConductorEnd;
    unresolved.confidence = ConfidenceClass::High;

    TerminalSemanticEvidence weak;
    weak.endpoint_id = "ep-2";
    weak.endpoint_kind = EndpointKind::ComponentTerminal;
    weak.confidence = ConfidenceClass::Medium;

    const auto preserved =
        TerminalSemanticResolver().resolve({unresolved}, {weak});

    assert(preserved.front().kind == EndpointKind::GeometricConductorEnd);
    assert(preserved.front().confidence == ConfidenceClass::High);

    return 0;
}

#include "eke_dx_wire/topology/connector_terminal_model.hpp"

#include <cassert>
#include <cstddef>

using namespace eke::dx::wire;

int main() {
    ComponentCandidate connector_component;
    connector_component.id = "component-candidate-connector";
    connector_component.kind = ComponentCandidateKind::PrimitiveSymbol;
    connector_component.bounds = BoundingBox{10, 20, 30, 40};
    connector_component.confidence = ConfidenceClass::High;
    connector_component.semantic_labels = {"J1"};

    EndpointCandidate endpoint;
    endpoint.id = "endpoint-1";
    endpoint.position = Point2D{10, 30};
    endpoint.terminal_role = TerminalRole::ConnectorTerminal;
    endpoint.terminal_name = "1";
    endpoint.function_label = "IGN";
    endpoint.wire_color = "BLK";
    endpoint.confidence = ConfidenceClass::High;

    TerminalCandidate terminal;
    terminal.id = "terminal-candidate-endpoint-1:component-candidate-connector";
    terminal.endpoint_id = endpoint.id;
    terminal.component_candidate_id = connector_component.id;
    terminal.kind = TerminalCandidateKind::ConnectorBoundary;
    terminal.position = endpoint.position;
    terminal.confidence = ConfidenceClass::High;

    ConnectorTerminalModelBuilder builder;
    const auto result = builder.build(
        {connector_component},
        {terminal},
        {endpoint});

    assert(result.connectors.size() == 1);
    assert(result.connectors[0].id == "connector-component-candidate-connector");
    assert(result.connectors[0].semantic_labels.size() == 1);
    assert(result.connectors[0].semantic_labels[0] == "J1");

    assert(result.terminals.size() == 1);
    assert(result.terminals[0].connector_id == result.connectors[0].id);
    assert(result.terminals[0].endpoint_id == endpoint.id);
    assert(result.terminals[0].terminal_name == "1");
    assert(result.terminals[0].function_label == "IGN");
    assert(result.terminals[0].wire_color == "BLK");
    assert(result.terminals[0].status == ConnectorTerminalStatus::Resolved);

    TerminalCandidate non_connector = terminal;
    non_connector.kind = TerminalCandidateKind::ComponentBoundary;
    const auto excluded = builder.build(
        {connector_component},
        {non_connector},
        {endpoint});
    assert(excluded.connectors.empty());
    assert(excluded.terminals.empty());

    // AP-DIAG-FIX-002 / TEST 8: repeated recognition of the same source
    // evidence must produce identical connector/terminal ids, ownership,
    // and ordering - no part of this builder consults anything but its own
    // deterministic string-based id construction and a stable sort.
    {
        const auto first = builder.build(
            {connector_component}, {terminal}, {endpoint});
        const auto second = builder.build(
            {connector_component}, {terminal}, {endpoint});

        assert(first.connectors.size() == second.connectors.size());
        for (std::size_t i = 0; i < first.connectors.size(); ++i) {
            assert(first.connectors[i].id == second.connectors[i].id);
        }

        assert(first.terminals.size() == second.terminals.size());
        for (std::size_t i = 0; i < first.terminals.size(); ++i) {
            assert(first.terminals[i].id == second.terminals[i].id);
            assert(first.terminals[i].connector_id ==
                   second.terminals[i].connector_id);
            assert(first.terminals[i].endpoint_id ==
                   second.terminals[i].endpoint_id);
            assert(first.terminals[i].status == second.terminals[i].status);
        }
    }

    return 0;
}

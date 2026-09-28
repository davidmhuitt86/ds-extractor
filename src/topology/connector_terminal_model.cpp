#include "eke_dx_wire/topology/connector_terminal_model.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

namespace eke::dx::wire {
namespace {

const ComponentCandidate* find_component(
    const std::vector<ComponentCandidate>& components,
    const std::string& id) {

    const auto it = std::find_if(
        components.begin(),
        components.end(),
        [&id](const ComponentCandidate& component) {
            return component.id == id;
        });
    return it == components.end() ? nullptr : &*it;
}

const EndpointCandidate* find_endpoint(
    const std::vector<EndpointCandidate>& endpoints,
    const std::string& id) {

    const auto it = std::find_if(
        endpoints.begin(),
        endpoints.end(),
        [&id](const EndpointCandidate& endpoint) {
            return endpoint.id == id;
        });
    return it == endpoints.end() ? nullptr : &*it;
}

} // namespace

ConnectorModelArtifacts ConnectorTerminalModelBuilder::build(
    const std::vector<ComponentCandidate>& components,
    const std::vector<TerminalCandidate>& terminal_candidates,
    const std::vector<EndpointCandidate>& endpoints) const {

    ConnectorModelArtifacts result;
    std::unordered_map<std::string, std::size_t> connector_indices;

    for (const auto& candidate : terminal_candidates) {
        if (candidate.kind != TerminalCandidateKind::ConnectorBoundary ||
            candidate.component_candidate_id.empty()) {
            continue;
        }

        const ComponentCandidate* component =
            find_component(components, candidate.component_candidate_id);
        if (component == nullptr) {
            continue;
        }

        auto connector_it =
            connector_indices.find(component->id);
        if (connector_it == connector_indices.end()) {
            ConnectorCandidate connector;
            connector.id = "connector-" + component->id;
            connector.component_candidate_id = component->id;
            connector.bounds = component->bounds;
            connector.confidence = component->confidence;
            connector.semantic_labels = component->semantic_labels;

            const std::size_t index = result.connectors.size();
            result.connectors.push_back(std::move(connector));
            connector_indices.emplace(component->id, index);
            connector_it = connector_indices.find(component->id);
        }

        const EndpointCandidate* endpoint =
            find_endpoint(endpoints, candidate.endpoint_id);
        if (endpoint == nullptr) {
            continue;
        }

        ConnectorTerminal terminal;
        terminal.id = "connector-terminal-" + candidate.id;
        terminal.connector_id = result.connectors[connector_it->second].id;
        terminal.endpoint_id = endpoint->id;
        terminal.position = endpoint->position;
        terminal.terminal_name = endpoint->terminal_name;
        terminal.function_label = endpoint->function_label;
        terminal.wire_color = endpoint->wire_color;
        terminal.role = endpoint->terminal_role;
        terminal.confidence = candidate.confidence;
        terminal.status =
            endpoint->terminal_role == TerminalRole::ConnectorTerminal
                ? ConnectorTerminalStatus::Resolved
                : ConnectorTerminalStatus::Unresolved;

        result.terminals.push_back(std::move(terminal));
    }

    std::sort(
        result.connectors.begin(),
        result.connectors.end(),
        [](const ConnectorCandidate& a, const ConnectorCandidate& b) {
            return a.id < b.id;
        });

    std::sort(
        result.terminals.begin(),
        result.terminals.end(),
        [](const ConnectorTerminal& a, const ConnectorTerminal& b) {
            return a.id < b.id;
        });

    return result;
}

ConnectorModelArtifacts ConnectorTerminalModelBuilder::build_native(
    const std::vector<ConnectorCandidate>& connectors,
    const std::vector<ConnectorPin>& pins,
    const std::vector<ConnectorTerminalAssociationEvidence>& associations,
    const std::vector<EndpointCandidate>& endpoints) const {

    ConnectorModelArtifacts result;
    std::unordered_map<std::string, const ConnectorCandidate*> connectors_by_id;
    std::unordered_map<std::string, const ConnectorPin*> pins_by_id;
    std::unordered_map<std::string, const EndpointCandidate*> endpoints_by_id;

    for (const auto& connector : connectors)
        connectors_by_id.emplace(connector.id, &connector);
    for (const auto& pin : pins)
        pins_by_id.emplace(pin.id, &pin);
    for (const auto& endpoint : endpoints)
        endpoints_by_id.emplace(endpoint.id, &endpoint);

    result.connectors.assign(connectors.begin(), connectors.end());

    for (const auto& association : associations) {
        if (association.status != ConnectorTerminalAssociationStatus::Resolved)
            continue;

        const auto connector_it =
            connectors_by_id.find(association.connector_id);
        const auto pin_it = pins_by_id.find(association.pin_id);
        const auto endpoint_it =
            endpoints_by_id.find(association.endpoint_id);
        if (connector_it == connectors_by_id.end() ||
            pin_it == pins_by_id.end() ||
            endpoint_it == endpoints_by_id.end()) {
            continue;
        }

        const auto* endpoint = endpoint_it->second;
        // Never let a connector terminal override an independently resolved
        // component/ground boundary. The endpoint must still be geometric or
        // already connector-native.
        if (endpoint->kind != EndpointKind::GeometricConductorEnd &&
            endpoint->kind != EndpointKind::ConnectorTerminal) {
            continue;
        }

        ConnectorTerminal terminal;
        terminal.id = stable_id(
            "connector-terminal",
            association.id + ":" + endpoint->id);
        terminal.connector_id = connector_it->second->id;
        terminal.endpoint_id = endpoint->id;
        terminal.position = pin_it->second->position;
        terminal.terminal_name = endpoint->terminal_name;
        terminal.function_label = endpoint->function_label;
        terminal.wire_color = endpoint->wire_color;
        terminal.role = TerminalRole::ConnectorTerminal;
        terminal.confidence = association.confidence;
        terminal.status = ConnectorTerminalStatus::Resolved;
        result.terminals.push_back(std::move(terminal));
    }

    std::sort(
        result.connectors.begin(),
        result.connectors.end(),
        [](const ConnectorCandidate& a, const ConnectorCandidate& b) {
            return a.id < b.id;
        });
    std::sort(
        result.terminals.begin(),
        result.terminals.end(),
        [](const ConnectorTerminal& a, const ConnectorTerminal& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

#include "eke_dx_wire/topology/connector_terminal_model.hpp"

#include <algorithm>
#include <unordered_map>

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

} // namespace eke::dx::wire

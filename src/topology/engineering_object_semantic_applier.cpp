#include "eke_dx_wire/topology/engineering_object_semantic_applier.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>

namespace eke::dx::wire {
namespace {

int confidence_rank(ConfidenceClass value) {
    switch (value) {
    case ConfidenceClass::High: return 3;
    case ConfidenceClass::Medium: return 2;
    case ConfidenceClass::Low: return 1;
    case ConfidenceClass::Unresolved: return 0;
    }
    return 0;
}

bool assign_if_empty_or_same(
    std::string& destination,
    const std::string& value) {

    if (value.empty()) {
        return false;
    }

    if (destination.empty()) {
        destination = value;
        return true;
    }

    return destination == value;
}

void apply_endpoint_semantic(
    EndpointCandidate& endpoint,
    const EngineeringObjectSemanticResolution& resolution) {

    switch (resolution.semantic_kind) {
    case TextSemanticKind::TerminalLabel:
        assign_if_empty_or_same(
            endpoint.terminal_name,
            resolution.normalized_text);
        break;

    case TextSemanticKind::FunctionLabel:
        assign_if_empty_or_same(
            endpoint.function_label,
            resolution.normalized_text);
        break;

    case TextSemanticKind::WireColorLabel:
        assign_if_empty_or_same(
            endpoint.wire_color,
            resolution.normalized_text);
        break;

    default:
        // Circuit-role labels are consumed by the circuit-role evidence
        // boundary. Component/connector labels belong to their component
        // object and are not copied into endpoint fields.
        break;
    }
}

} // namespace

void EngineeringObjectSemanticApplier::apply(
    std::vector<ComponentCandidate>& components,
    std::vector<EndpointCandidate>& endpoints,
    const std::vector<EngineeringObjectSemanticResolution>& resolutions) const {

    std::unordered_map<std::string, ComponentCandidate*> component_by_id;
    component_by_id.reserve(components.size());
    for (auto& component : components) {
        component_by_id.emplace(component.id, &component);
    }

    std::unordered_map<std::string, EndpointCandidate*> endpoint_by_id;
    endpoint_by_id.reserve(endpoints.size());
    for (auto& endpoint : endpoints) {
        endpoint_by_id.emplace(endpoint.id, &endpoint);
    }

    for (const auto& resolution : resolutions) {
        if (resolution.target_id.empty() ||
            resolution.normalized_text.empty() ||
            resolution.confidence == ConfidenceClass::Unresolved) {
            continue;
        }

        if (resolution.target_kind == SemanticAssociationTargetKind::Component) {
            const auto component_it = component_by_id.find(resolution.target_id);
            if (component_it == component_by_id.end()) {
                continue;
            }

            if (resolution.semantic_kind != TextSemanticKind::ComponentLabel &&
                resolution.semantic_kind != TextSemanticKind::ConnectorLabel) {
                continue;
            }

            // ComponentCandidate currently has no mutable label field. Keep
            // the resolved observation as the authoritative semantic record;
            // component geometry itself remains untouched.
            continue;
        }

        const auto endpoint_it = endpoint_by_id.find(resolution.target_id);
        if (endpoint_it == endpoint_by_id.end()) {
            continue;
        }

        apply_endpoint_semantic(*endpoint_it->second, resolution);
    }

    std::sort(
        endpoints.begin(),
        endpoints.end(),
        [](const EndpointCandidate& a, const EndpointCandidate& b) {
            return a.id < b.id;
        });
}

} // namespace eke::dx::wire

#include "eke_dx_wire/topology/electrical_component_resolver.hpp"
#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace eke::dx::wire {

ElectricalComponentResolutionArtifacts ElectricalComponentResolver::resolve(
    const std::vector<ComponentCandidate>& components,
    const std::vector<SymbolFamilyResolution>& symbol_family_resolutions,
    const std::vector<ConnectorCandidate>& connectors,
    const std::vector<EndpointCandidate>& endpoints) const {

    ElectricalComponentResolutionArtifacts result;

    std::set<std::string> connector_owned_components;
    for (const auto& connector : connectors) {
        if (!connector.component_candidate_id.empty()) {
            connector_owned_components.insert(connector.component_candidate_id);
        }
    }

    std::map<std::string, const SymbolFamilyResolution*> family_by_component;
    for (const auto& resolution : symbol_family_resolutions) {
        family_by_component[resolution.component_id] = &resolution;
    }

    std::map<std::string, std::vector<std::string>> terminal_ids_by_component;
    for (const auto& endpoint : endpoints) {
        if (endpoint.kind == EndpointKind::ComponentTerminal &&
            !endpoint.component_id.empty()) {
            terminal_ids_by_component[endpoint.component_id].push_back(endpoint.id);
        }
    }

    for (const auto& component : components) {
        if (component.id.empty()) continue;

        ElectricalComponent electrical;
        electrical.component_candidate_id = component.id;

        if (component.kind == ComponentCandidateKind::DiagramFurniture) {
            electrical.status = ElectricalComponentResolutionStatus::Rejected;
            electrical.rejection_reason =
                ElectricalComponentRejectionReason::DiagramFurniture;
        } else if (component.kind == ComponentCandidateKind::ChassisGround) {
            electrical.status = ElectricalComponentResolutionStatus::Rejected;
            electrical.rejection_reason =
                ElectricalComponentRejectionReason::ChassisGroundReference;
        } else if (connector_owned_components.count(component.id)) {
            electrical.status = ElectricalComponentResolutionStatus::Rejected;
            electrical.rejection_reason =
                ElectricalComponentRejectionReason::ConnectorInterface;
        } else {
            auto terminal_it = terminal_ids_by_component.find(component.id);
            const bool has_terminal =
                terminal_it != terminal_ids_by_component.end() &&
                !terminal_it->second.empty();

            const auto family_it = family_by_component.find(component.id);
            const bool has_function_evidence =
                family_it != family_by_component.end() &&
                family_it->second->status == SymbolFamilyResolutionStatus::Resolved &&
                family_it->second->family != SymbolFamily::Unknown &&
                family_it->second->family != SymbolFamily::Ground;

            if (has_terminal && has_function_evidence) {
                electrical.status = ElectricalComponentResolutionStatus::Resolved;
                electrical.family = family_it->second->family;
                electrical.confidence = family_it->second->confidence;
                electrical.terminal_endpoint_ids = terminal_it->second;
                electrical.evidence_ids.push_back(family_it->second->id);
            } else {
                electrical.status = ElectricalComponentResolutionStatus::Unresolved;
                if (has_terminal) {
                    electrical.terminal_endpoint_ids = terminal_it->second;
                }
                // A resolved-but-excluded family (Ground/Unknown) or a
                // resolved family without terminal evidence is preserved
                // for explainability even though it did not promote the
                // candidate - never guessed into Resolved.
                if (family_it != family_by_component.end() &&
                    family_it->second->status == SymbolFamilyResolutionStatus::Resolved) {
                    electrical.family = family_it->second->family;
                    electrical.evidence_ids.push_back(family_it->second->id);
                }
            }
        }

        std::sort(
            electrical.terminal_endpoint_ids.begin(),
            electrical.terminal_endpoint_ids.end());

        electrical.id = stable_id("electrical-component", component.id);
        result.electrical_components.push_back(std::move(electrical));
    }

    std::sort(
        result.electrical_components.begin(), result.electrical_components.end(),
        [](const ElectricalComponent& a, const ElectricalComponent& b) {
            return a.component_candidate_id < b.component_candidate_id;
        });

    return result;
}

} // namespace eke::dx::wire

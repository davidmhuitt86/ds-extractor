#include "eke_dx_wire/topology/endpoint_semantic_reconstructor.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace eke::dx::wire {
namespace {

int confidence_rank(ConfidenceClass confidence) {
    switch (confidence) {
    case ConfidenceClass::High: return 3;
    case ConfidenceClass::Medium: return 2;
    case ConfidenceClass::Low: return 1;
    case ConfidenceClass::Unresolved: return 0;
    }
    return 0;
}

ConfidenceClass max_confidence(
    const std::vector<const TerminalSemanticEvidence*>& evidence) {

    ConfidenceClass result = ConfidenceClass::Unresolved;
    for (const auto* item : evidence) {
        if (confidence_rank(item->confidence) > confidence_rank(result))
            result = item->confidence;
    }
    return result;
}

template <typename T, typename Getter>
bool has_conflict(
    const std::vector<const TerminalSemanticEvidence*>& evidence,
    Getter getter) {

    std::set<T> values;
    for (const auto* item : evidence) {
        const T value = getter(*item);
        if (value != T {}) {
            values.insert(value);
        }
    }
    return values.size() > 1;
}

std::string single_component_id(
    const std::vector<const TerminalSemanticEvidence*>& evidence) {

    std::set<std::string> values;
    for (const auto* item : evidence) {
        if (!item->component_id.empty())
            values.insert(item->component_id);
    }
    return values.size() == 1 ? *values.begin() : std::string {};
}

EndpointKind single_endpoint_kind(
    const std::vector<const TerminalSemanticEvidence*>& evidence) {

    std::set<EndpointKind> values;
    for (const auto* item : evidence) {
        if (item->endpoint_kind != EndpointKind::Unresolved)
            values.insert(item->endpoint_kind);
    }
    return values.size() == 1 ? *values.begin() : EndpointKind::Unresolved;
}

TerminalRole single_terminal_role(
    const std::vector<const TerminalSemanticEvidence*>& evidence) {

    std::set<TerminalRole> values;
    for (const auto* item : evidence) {
        if (item->role != TerminalRole::Unknown)
            values.insert(item->role);
    }
    return values.size() == 1 ? *values.begin() : TerminalRole::Unknown;
}

} // namespace

EndpointSemanticReconstructionArtifacts
EndpointSemanticReconstructor::reconstruct(
    const std::vector<EndpointCandidate>& endpoints,
    const std::vector<TerminalSemanticEvidence>& evidence) const {

    std::map<std::string, std::vector<const TerminalSemanticEvidence*>> by_endpoint;
    for (const auto& item : evidence) {
        if (!item.endpoint_id.empty())
            by_endpoint[item.endpoint_id].push_back(&item);
    }

    EndpointSemanticReconstructionArtifacts result;
    result.endpoints = endpoints;
    result.reconstructions.reserve(endpoints.size());

    for (auto& endpoint : result.endpoints) {
        const auto it = by_endpoint.find(endpoint.id);

        EndpointSemanticReconstruction reconstruction;
        reconstruction.endpoint_id = endpoint.id;
        reconstruction.id = stable_id(
            "endpoint-semantic-reconstruction",
            endpoint.id + ":endpoint-semantic-reconstruction");

        if (it == by_endpoint.end() || it->second.empty()) {
            reconstruction.status =
                EndpointSemanticReconstructionStatus::Unresolved;
            result.reconstructions.push_back(std::move(reconstruction));
            continue;
        }

        const auto& items = it->second;

        for (const auto* item : items) {
            if (!item->component_id.empty())
                reconstruction.evidence_component_ids.push_back(
                    item->component_id);
        }

        std::sort(
            reconstruction.evidence_component_ids.begin(),
            reconstruction.evidence_component_ids.end());
        reconstruction.evidence_component_ids.erase(
            std::unique(
                reconstruction.evidence_component_ids.begin(),
                reconstruction.evidence_component_ids.end()),
            reconstruction.evidence_component_ids.end());

        const bool component_conflict =
            has_conflict<std::string>(
                items,
                [](const TerminalSemanticEvidence& item) {
                    return item.component_id;
                });

        const bool kind_conflict =
            has_conflict<EndpointKind>(
                items,
                [](const TerminalSemanticEvidence& item) {
                    return item.endpoint_kind;
                });

        const bool role_conflict =
            has_conflict<TerminalRole>(
                items,
                [](const TerminalSemanticEvidence& item) {
                    return item.role;
                });

        if (component_conflict || kind_conflict || role_conflict) {
            reconstruction.status =
                EndpointSemanticReconstructionStatus::Conflicted;
            reconstruction.confidence = ConfidenceClass::Unresolved;

            endpoint.kind = EndpointKind::GeometricConductorEnd;
            endpoint.terminal_role = TerminalRole::Unknown;
            endpoint.component_id.clear();
            endpoint.terminal_name.clear();
            endpoint.function_label.clear();
            endpoint.wire_color.clear();
            endpoint.confidence = ConfidenceClass::Unresolved;

            result.reconstructions.push_back(std::move(reconstruction));
            continue;
        }

        reconstruction.component_id = single_component_id(items);
        reconstruction.endpoint_kind = single_endpoint_kind(items);
        reconstruction.terminal_role = single_terminal_role(items);
        reconstruction.confidence = max_confidence(items);
        reconstruction.status =
            EndpointSemanticReconstructionStatus::Resolved;

        if (!reconstruction.component_id.empty())
            endpoint.component_id = reconstruction.component_id;
        if (reconstruction.endpoint_kind != EndpointKind::Unresolved)
            endpoint.kind = reconstruction.endpoint_kind;
        if (reconstruction.terminal_role != TerminalRole::Unknown)
            endpoint.terminal_role = reconstruction.terminal_role;
        if (reconstruction.confidence != ConfidenceClass::Unresolved)
            endpoint.confidence = reconstruction.confidence;

        result.reconstructions.push_back(std::move(reconstruction));
    }

    std::sort(
        result.endpoints.begin(),
        result.endpoints.end(),
        [](const EndpointCandidate& a, const EndpointCandidate& b) {
            return a.id < b.id;
        });

    std::sort(
        result.reconstructions.begin(),
        result.reconstructions.end(),
        [](const EndpointSemanticReconstruction& a,
           const EndpointSemanticReconstruction& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

#include "eke_dx_wire/topology/component_identity_evidence_builder.hpp"

#include <algorithm>
#include <map>
#include <string>

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

ComponentIdentityEvidenceKind kind_for(TextSemanticKind kind) {
    return kind == TextSemanticKind::ConnectorLabel
        ? ComponentIdentityEvidenceKind::ConnectorLabel
        : ComponentIdentityEvidenceKind::ComponentLabel;
}

std::string make_id(
    const EngineeringObjectSemanticResolution& resolution) {
    return "component-identity-evidence-" + resolution.id;
}

} // namespace

std::vector<ComponentIdentityEvidence>
ComponentIdentityEvidenceBuilder::build(
    const std::vector<EngineeringObjectSemanticResolution>& resolutions) const {

    // This boundary records identity-bearing label evidence only. It does
    // not assert that the label is the component's canonical identity and
    // does not consult a component registry.
    std::map<
        std::tuple<std::string, ComponentIdentityEvidenceKind, std::string>,
        ComponentIdentityEvidence> strongest;

    for (const auto& resolution : resolutions) {
        if (resolution.target_kind != SemanticAssociationTargetKind::Component ||
            resolution.target_id.empty() ||
            resolution.normalized_text.empty() ||
            resolution.confidence == ConfidenceClass::Unresolved) {
            continue;
        }

        if (resolution.semantic_kind != TextSemanticKind::ComponentLabel &&
            resolution.semantic_kind != TextSemanticKind::ConnectorLabel) {
            continue;
        }

        ComponentIdentityEvidence evidence{
            make_id(resolution),
            resolution.target_id,
            kind_for(resolution.semantic_kind),
            resolution.raw_text,
            resolution.normalized_text,
            resolution.confidence,
            resolution.distance,
            resolution.source
        };

        const auto key = std::make_tuple(
            evidence.component_id,
            evidence.kind,
            evidence.normalized_text);

        const auto it = strongest.find(key);
        if (it == strongest.end() ||
            confidence_rank(evidence.confidence) >
                confidence_rank(it->second.confidence) ||
            (confidence_rank(evidence.confidence) ==
                 confidence_rank(it->second.confidence) &&
             evidence.distance < it->second.distance)) {
            strongest[key] = std::move(evidence);
        }
    }

    std::vector<ComponentIdentityEvidence> result;
    result.reserve(strongest.size());
    for (auto& [key, evidence] : strongest) {
        result.push_back(std::move(evidence));
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const auto& a, const auto& b) {
            if (a.component_id != b.component_id)
                return a.component_id < b.component_id;
            if (a.kind != b.kind)
                return static_cast<int>(a.kind) < static_cast<int>(b.kind);
            if (a.normalized_text != b.normalized_text)
                return a.normalized_text < b.normalized_text;
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

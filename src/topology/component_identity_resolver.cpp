#include "eke_dx_wire/topology/component_identity_resolver.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

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

ConfidenceClass confidence_from_rank(int rank) {
    if (rank >= 3) return ConfidenceClass::High;
    if (rank == 2) return ConfidenceClass::Medium;
    if (rank == 1) return ConfidenceClass::Low;
    return ConfidenceClass::Unresolved;
}

} // namespace

std::vector<ComponentIdentityResolution>
ComponentIdentityResolver::resolve(
    const std::vector<ComponentIdentityEvidence>& evidence) const {

    std::map<std::string, std::vector<const ComponentIdentityEvidence*>> grouped;

    for (const auto& item : evidence) {
        if (item.component_id.empty() ||
            item.normalized_text.empty() ||
            item.confidence == ConfidenceClass::Unresolved) {
            continue;
        }

        grouped[item.component_id].push_back(&item);
    }

    std::vector<ComponentIdentityResolution> result;
    result.reserve(grouped.size());

    for (const auto& [component_id, items] : grouped) {
        std::set<std::string> identities;
        int strongest_rank = 0;
        std::string strongest_identity;
        std::vector<std::string> evidence_ids;

        for (const auto* item : items) {
            identities.insert(item->normalized_text);
            evidence_ids.push_back(item->id);

            const int rank = confidence_rank(item->confidence);
            if (rank > strongest_rank ||
                (rank == strongest_rank &&
                 (strongest_identity.empty() ||
                  item->normalized_text < strongest_identity))) {
                strongest_rank = rank;
                strongest_identity = item->normalized_text;
            }
        }

        std::sort(evidence_ids.begin(), evidence_ids.end());

        ComponentIdentityResolution resolution;
        resolution.id = "component-identity-resolution-" + component_id;
        resolution.component_id = component_id;
        resolution.evidence_ids = std::move(evidence_ids);

        if (identities.size() == 1) {
            resolution.identity = *identities.begin();
            resolution.confidence = confidence_from_rank(strongest_rank);
            resolution.status = ComponentIdentityResolutionStatus::Resolved;
        } else if (!identities.empty()) {
            resolution.confidence = ConfidenceClass::Unresolved;
            resolution.status = ComponentIdentityResolutionStatus::Conflicted;
        }

        result.push_back(std::move(resolution));
    }

    return result;
}

} // namespace eke::dx::wire

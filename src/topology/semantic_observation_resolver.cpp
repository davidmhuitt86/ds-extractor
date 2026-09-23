#include "eke_dx_wire/topology/semantic_observation_resolver.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eke::dx::wire {
namespace {

struct ObservationCandidate {
    std::string endpoint_id;
    DistributionRole role = DistributionRole::Unknown;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    double distance = 0.0;
    std::string source;
};

int confidence_rank(ConfidenceClass value) {
    switch (value) {
    case ConfidenceClass::High: return 3;
    case ConfidenceClass::Medium: return 2;
    case ConfidenceClass::Low: return 1;
    case ConfidenceClass::Unresolved: return 0;
    }
    return 0;
}

ConfidenceClass minimum_confidence(
    ConfidenceClass a,
    ConfidenceClass b) {

    return confidence_rank(a) <= confidence_rank(b) ? a : b;
}

bool role_for_text(
    TextSemanticKind kind,
    DistributionRole& role) {

    switch (kind) {
    case TextSemanticKind::GroundLabel:
        role = DistributionRole::Ground;
        return true;
    case TextSemanticKind::PowerFeedLabel:
        role = DistributionRole::PowerFeed;
        return true;
    case TextSemanticKind::SharedFunctionFeedLabel:
        role = DistributionRole::SharedFunctionFeed;
        return true;
    default:
        return false;
    }
}

} // namespace

std::vector<CircuitRoleEvidence>
SemanticObservationResolver::resolve(
    const std::vector<TextSemanticEvidence>& semantic_evidence,
    const std::vector<SemanticAssociation>& associations) const {

    std::unordered_map<std::string, std::vector<const SemanticAssociation*>>
        endpoint_associations;

    for (const auto& association : associations) {
        if (association.text_region_id.empty() ||
            association.target_id.empty() ||
            association.target_kind !=
                SemanticAssociationTargetKind::Endpoint ||
            association.relation !=
                SemanticAssociationRelation::LabelToEndpoint ||
            association.confidence == ConfidenceClass::Unresolved) {
            continue;
        }

        endpoint_associations[association.text_region_id].push_back(
            &association);
    }

    std::vector<ObservationCandidate> observations;

    for (const auto& semantic : semantic_evidence) {
        DistributionRole role = DistributionRole::Unknown;
        if (!role_for_text(semantic.kind, role) ||
            semantic.text_region_id.empty() ||
            semantic.confidence == ConfidenceClass::Unresolved) {
            continue;
        }

        const auto association_it =
            endpoint_associations.find(semantic.text_region_id);
        if (association_it == endpoint_associations.end() ||
            association_it->second.empty()) {
            continue;
        }

        const auto& candidates = association_it->second;
        double best_distance = candidates.front()->distance;
        for (const auto* candidate : candidates) {
            best_distance = std::min(best_distance, candidate->distance);
        }

        std::set<std::string> best_endpoints;
        for (const auto* candidate : candidates) {
            if (candidate->distance == best_distance) {
                best_endpoints.insert(candidate->target_id);
            }
        }

        // A label equidistant from multiple endpoints cannot safely be
        // promoted to one endpoint by ordering alone.
        if (best_endpoints.size() != 1) {
            continue;
        }

        const std::string& endpoint_id = *best_endpoints.begin();
        const auto best_association = std::find_if(
            candidates.begin(),
            candidates.end(),
            [&](const SemanticAssociation* candidate) {
                return candidate->target_id == endpoint_id &&
                    candidate->distance == best_distance;
            });

        if (best_association == candidates.end()) {
            continue;
        }

        const ConfidenceClass confidence = minimum_confidence(
            semantic.confidence,
            (*best_association)->confidence);

        observations.push_back({
            endpoint_id,
            role,
            confidence,
            best_distance,
            "text-semantic-association:" + semantic.source
        });
    }

    // Resolve all text observations landing on the same endpoint. Stronger
    // evidence is preferred; equally strong conflicting roles remain
    // unresolved rather than being selected by ordering.
    std::map<std::string, std::vector<ObservationCandidate>> by_endpoint;
    for (auto& observation : observations) {
        by_endpoint[observation.endpoint_id].push_back(
            std::move(observation));
    }

    std::vector<CircuitRoleEvidence> result;

    for (auto& [endpoint_id, candidates] : by_endpoint) {
        int best_rank = -1;
        for (const auto& candidate : candidates) {
            best_rank = std::max(
                best_rank,
                confidence_rank(candidate.confidence));
        }

        std::set<DistributionRole> best_roles;
        for (const auto& candidate : candidates) {
            if (confidence_rank(candidate.confidence) == best_rank) {
                best_roles.insert(candidate.role);
            }
        }

        if (best_roles.size() != 1) {
            continue;
        }

        const DistributionRole role = *best_roles.begin();

        const auto best = std::find_if(
            candidates.begin(),
            candidates.end(),
            [&](const ObservationCandidate& candidate) {
                return candidate.role == role &&
                    confidence_rank(candidate.confidence) == best_rank;
            });

        if (best == candidates.end()) {
            continue;
        }

        result.push_back({
            endpoint_id,
            role,
            best->confidence,
            best->source
        });
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const CircuitRoleEvidence& a, const CircuitRoleEvidence& b) {
            if (a.endpoint_id != b.endpoint_id) {
                return a.endpoint_id < b.endpoint_id;
            }
            if (a.role != b.role) {
                return static_cast<int>(a.role) <
                    static_cast<int>(b.role);
            }
            return a.source < b.source;
        });

    return result;
}

} // namespace eke::dx::wire

#include "eke_dx_wire/topology/engineering_object_semantic_resolver.hpp"

#include <algorithm>
#include <map>
#include <set>
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

ConfidenceClass minimum_confidence(
    ConfidenceClass a,
    ConfidenceClass b) {

    return confidence_rank(a) <= confidence_rank(b) ? a : b;
}

std::string resolution_id(
    const TextSemanticEvidence& evidence,
    const SemanticAssociation& association) {

    return "semantic-resolution-" +
        evidence.id + "-" + association.target_id;
}

} // namespace

std::vector<EngineeringObjectSemanticResolution>
EngineeringObjectSemanticResolver::resolve(
    const std::vector<TextSemanticEvidence>& semantic_evidence,
    const std::vector<SemanticAssociation>& associations) const {

    std::unordered_map<std::string, std::vector<const SemanticAssociation*>>
        by_text_region;

    for (const auto& association : associations) {
        if (association.text_region_id.empty() ||
            association.target_id.empty() ||
            association.confidence == ConfidenceClass::Unresolved) {
            continue;
        }

        by_text_region[association.text_region_id].push_back(&association);
    }

    std::vector<EngineeringObjectSemanticResolution> candidates;

    for (const auto& evidence : semantic_evidence) {
        if (evidence.id.empty() ||
            evidence.text_region_id.empty() ||
            evidence.kind == TextSemanticKind::Unknown ||
            evidence.confidence == ConfidenceClass::Unresolved) {
            continue;
        }

        const auto it = by_text_region.find(evidence.text_region_id);
        if (it == by_text_region.end() || it->second.empty()) {
            continue;
        }

        double best_distance = it->second.front()->distance;
        for (const auto* association : it->second) {
            best_distance = std::min(best_distance, association->distance);
        }

        std::set<std::string> best_targets;
        for (const auto* association : it->second) {
            if (association->distance == best_distance) {
                best_targets.insert(
                    association->target_kind == SemanticAssociationTargetKind::Component
                        ? "component:" + association->target_id
                        : "endpoint:" + association->target_id);
            }
        }

        // A semantic label equidistant from distinct engineering objects
        // remains unresolved rather than being assigned by iteration order.
        if (best_targets.size() != 1) {
            continue;
        }

        const auto* best_association = *std::min_element(
            it->second.begin(),
            it->second.end(),
            [&](const SemanticAssociation* a, const SemanticAssociation* b) {
                const double da = a->distance;
                const double db = b->distance;
                if (da != db) {
                    return da < db;
                }
                return a->target_id < b->target_id;
            });

        if (best_association->distance != best_distance) {
            continue;
        }

        candidates.push_back({
            resolution_id(evidence, *best_association),
            evidence.text_region_id,
            best_association->target_id,
            best_association->target_kind,
            evidence.kind,
            evidence.raw_text,
            evidence.normalized_text,
            minimum_confidence(
                evidence.confidence,
                best_association->confidence),
            best_association->distance,
            "semantic-object-association:" + evidence.source
        });
    }

    // Resolve repeated observations of the same semantic category on the
    // same object. Stronger evidence wins. Equal-strength conflicting text
    // remains unresolved instead of being selected by ordering alone.
    struct Key {
        SemanticAssociationTargetKind target_kind;
        std::string target_id;
        TextSemanticKind semantic_kind;

        bool operator<(const Key& other) const {
            if (target_kind != other.target_kind) {
                return static_cast<int>(target_kind) <
                    static_cast<int>(other.target_kind);
            }
            if (target_id != other.target_id) {
                return target_id < other.target_id;
            }
            return static_cast<int>(semantic_kind) <
                static_cast<int>(other.semantic_kind);
        }
    };

    std::map<Key, std::vector<EngineeringObjectSemanticResolution>> grouped;
    for (auto& candidate : candidates) {
        grouped[{
            candidate.target_kind,
            candidate.target_id,
            candidate.semantic_kind
        }].push_back(std::move(candidate));
    }

    std::vector<EngineeringObjectSemanticResolution> result;

    for (auto& [key, values] : grouped) {
        int best_rank = -1;
        for (const auto& value : values) {
            best_rank = std::max(
                best_rank,
                confidence_rank(value.confidence));
        }

        std::set<std::string> best_texts;
        for (const auto& value : values) {
            if (confidence_rank(value.confidence) == best_rank) {
                best_texts.insert(value.normalized_text);
            }
        }

        if (best_texts.size() != 1) {
            continue;
        }

        auto best = std::min_element(
            values.begin(),
            values.end(),
            [&](const auto& a, const auto& b) {
                const int ar = confidence_rank(a.confidence);
                const int br = confidence_rank(b.confidence);
                if (ar != br) {
                    return ar > br;
                }
                if (a.distance != b.distance) {
                    return a.distance < b.distance;
                }
                return a.id < b.id;
            });

        result.push_back(*best);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const auto& a, const auto& b) {
            if (a.target_kind != b.target_kind) {
                return static_cast<int>(a.target_kind) <
                    static_cast<int>(b.target_kind);
            }
            if (a.target_id != b.target_id) {
                return a.target_id < b.target_id;
            }
            if (a.semantic_kind != b.semantic_kind) {
                return static_cast<int>(a.semantic_kind) <
                    static_cast<int>(b.semantic_kind);
            }
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

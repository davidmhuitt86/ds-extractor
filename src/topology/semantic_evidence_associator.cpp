#include "eke_dx_wire/topology/semantic_evidence_associator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace eke::dx::wire {
namespace {

double point_to_box_distance(
    const Point2D& point,
    const BoundingBox& box) {

    const double left = static_cast<double>(box.x);
    const double right = static_cast<double>(box.x + box.width);
    const double top = static_cast<double>(box.y);
    const double bottom = static_cast<double>(box.y + box.height);

    const double dx =
        point.x < left ? left - point.x :
        point.x > right ? point.x - right : 0.0;
    const double dy =
        point.y < top ? top - point.y :
        point.y > bottom ? point.y - bottom : 0.0;

    return std::hypot(dx, dy);
}

double box_to_box_distance(
    const BoundingBox& a,
    const BoundingBox& b) {

    const double a_left = static_cast<double>(a.x);
    const double a_right = static_cast<double>(a.x + a.width);
    const double a_top = static_cast<double>(a.y);
    const double a_bottom = static_cast<double>(a.y + a.height);

    const double b_left = static_cast<double>(b.x);
    const double b_right = static_cast<double>(b.x + b.width);
    const double b_top = static_cast<double>(b.y);
    const double b_bottom = static_cast<double>(b.y + b.height);

    const double dx =
        a_right < b_left ? b_left - a_right :
        b_right < a_left ? a_left - b_right : 0.0;
    const double dy =
        a_bottom < b_top ? b_top - a_bottom :
        b_bottom < a_top ? a_top - b_bottom : 0.0;

    return std::hypot(dx, dy);
}

ConfidenceClass distance_confidence(
    double distance,
    double text_confidence,
    const SemanticEvidenceAssociationConfig& config) {

    ConfidenceClass geometric = ConfidenceClass::Unresolved;
    if (distance <= config.high_confidence_distance) {
        geometric = ConfidenceClass::High;
    } else if (distance <= config.medium_confidence_distance) {
        geometric = ConfidenceClass::Medium;
    } else if (distance <= config.maximum_association_distance) {
        geometric = ConfidenceClass::Low;
    }

    if (geometric == ConfidenceClass::Unresolved) {
        return geometric;
    }

    ConfidenceClass text_class = ConfidenceClass::Low;
    if (text_confidence >= 0.85) {
        text_class = ConfidenceClass::High;
    } else if (text_confidence >= 0.70) {
        text_class = ConfidenceClass::Medium;
    }

    if (geometric == ConfidenceClass::High &&
        text_class == ConfidenceClass::High) {
        return ConfidenceClass::High;
    }
    if (geometric == ConfidenceClass::High ||
        (geometric == ConfidenceClass::Medium &&
         text_class != ConfidenceClass::Low)) {
        return ConfidenceClass::Medium;
    }
    return ConfidenceClass::Low;
}

} // namespace

SemanticEvidenceAssociator::SemanticEvidenceAssociator(
    SemanticEvidenceAssociationConfig config)
    : config_(config) {}

std::vector<SemanticAssociation>
SemanticEvidenceAssociator::associate(
    const std::vector<TextRegion>& text_regions,
    const std::vector<ComponentCandidate>& components,
    const std::vector<EndpointCandidate>& endpoints) const {

    std::vector<SemanticAssociation> result;

    for (const auto& text : text_regions) {
        if (text.id.empty() ||
            text.confidence <= 0.0) {
            continue;
        }

        // A text region is evidence, not a semantic label. This stage records
        // only spatial relationships; it never interprets the text contents
        // or assigns a circuit role.
        for (const auto& component : components) {
            if (component.id.empty()) {
                continue;
            }

            const double distance =
                box_to_box_distance(text.bounds, component.bounds);
            if (distance > config_.maximum_association_distance) {
                continue;
            }

            SemanticAssociation association;
            association.id =
                text.id + ":component:" + component.id;
            association.text_region_id = text.id;
            association.target_id = component.id;
            association.target_kind =
                SemanticAssociationTargetKind::Component;
            association.relation =
                SemanticAssociationRelation::LabelToComponent;
            association.distance = distance;
            association.confidence = distance_confidence(
                distance,
                text.confidence,
                config_);
            result.push_back(std::move(association));
        }

        for (const auto& endpoint : endpoints) {
            if (endpoint.id.empty()) {
                continue;
            }

            const double distance =
                point_to_box_distance(endpoint.position, text.bounds);
            if (distance > config_.maximum_association_distance) {
                continue;
            }

            SemanticAssociation association;
            association.id =
                text.id + ":endpoint:" + endpoint.id;
            association.text_region_id = text.id;
            association.target_id = endpoint.id;
            association.target_kind =
                SemanticAssociationTargetKind::Endpoint;
            association.relation =
                SemanticAssociationRelation::LabelToEndpoint;
            association.distance = distance;
            association.confidence = distance_confidence(
                distance,
                text.confidence,
                config_);
            result.push_back(std::move(association));
        }
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const SemanticAssociation& a, const SemanticAssociation& b) {
            if (a.text_region_id != b.text_region_id) {
                return a.text_region_id < b.text_region_id;
            }
            if (a.distance != b.distance) {
                return a.distance < b.distance;
            }
            if (a.target_kind != b.target_kind) {
                return static_cast<int>(a.target_kind) <
                    static_cast<int>(b.target_kind);
            }
            return a.target_id < b.target_id;
        });

    return result;
}

} // namespace eke::dx::wire

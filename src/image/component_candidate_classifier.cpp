#include "eke_dx_wire/image/component_candidate_classifier.hpp"

#include <algorithm>

namespace eke::dx::wire {

namespace {

ComponentCandidateKind classify_kind(const ShapeRegion& shape) {
    switch (shape.kind) {
    case ShapeKind::ChassisGround:
        return ComponentCandidateKind::ChassisGround;
    case ShapeKind::Circle:
        return ComponentCandidateKind::CircularSymbol;
    case ShapeKind::Rectangle:
        return shape.role == ShapeRole::Enclosure
            ? ComponentCandidateKind::Enclosure
            : ComponentCandidateKind::PrimitiveSymbol;
    default:
        return ComponentCandidateKind::Unknown;
    }
}

ConfidenceClass classify_confidence(const ShapeRegion& shape) {
    if (shape.confidence >= 0.90) {
        return ConfidenceClass::High;
    }
    if (shape.confidence >= 0.70) {
        return ConfidenceClass::Medium;
    }
    if (shape.confidence > 0.0) {
        return ConfidenceClass::Low;
    }
    return ConfidenceClass::Unresolved;
}

std::string make_candidate_id(const ShapeRegion& shape) {
    return "component-candidate-" + shape.id;
}

} // namespace

std::vector<ComponentCandidate> ComponentCandidateClassifier::classify(
    const ShapeDetectionArtifacts& shapes) const {

    std::vector<ComponentCandidate> result;
    result.reserve(shapes.regions.size());

    for (const auto& shape : shapes.regions) {
        if (shape.id.empty()) {
            continue;
        }

        ComponentCandidate candidate;
        candidate.id = make_candidate_id(shape);
        candidate.kind = classify_kind(shape);
        candidate.shape_ids.push_back(shape.id);
        candidate.bounds = shape.bounds;
        candidate.confidence = classify_confidence(shape);
        candidate.circle_probe_evidence = shape.circle_probe_evidence;
        candidate.circle_probe_distance = shape.circle_probe_distance;
        candidate.circle_probe_top = shape.circle_probe_top;
        candidate.circle_probe_bottom = shape.circle_probe_bottom;
        candidate.circle_probe_left = shape.circle_probe_left;
        candidate.circle_probe_right = shape.circle_probe_right;
        candidate.circle_probe_weakest_side = shape.circle_probe_weakest_side;
        candidate.circle_circularity = shape.circle_circularity;
        candidate.circle_aspect_ratio = shape.circle_aspect_ratio;
        candidate.circle_radius = shape.circle_radius;
        candidate.circle_edge_support = shape.circle_edge_support;
        candidate.circle_interior_density = shape.circle_interior_density;
        candidate.circle_probe_corner_top_left = shape.circle_probe_corner_top_left;
        candidate.circle_probe_corner_top_right = shape.circle_probe_corner_top_right;
        candidate.circle_probe_corner_bottom_left = shape.circle_probe_corner_bottom_left;
        candidate.circle_probe_corner_bottom_right = shape.circle_probe_corner_bottom_right;
        candidate.circle_probe_run_top_left = shape.circle_probe_run_top_left;
        candidate.circle_probe_run_top_right = shape.circle_probe_run_top_right;
        candidate.circle_probe_run_bottom_left = shape.circle_probe_run_bottom_left;
        candidate.circle_probe_run_bottom_right = shape.circle_probe_run_bottom_right;
        candidate.circle_probe_run_left_top = shape.circle_probe_run_left_top;
        candidate.circle_probe_run_left_bottom = shape.circle_probe_run_left_bottom;
        candidate.circle_probe_run_right_top = shape.circle_probe_run_right_top;
        candidate.circle_probe_run_right_bottom = shape.circle_probe_run_right_bottom;
        candidate.circle_local_density_3x3 = shape.circle_local_density_3x3;
        candidate.circle_local_density_7x7 = shape.circle_local_density_7x7;
        candidate.circle_local_ring_density = shape.circle_local_ring_density;
        candidate.circle_probe_long_run_count = shape.circle_probe_long_run_count;
        candidate.circle_probe_max_run_fraction = shape.circle_probe_max_run_fraction;
        candidate.circle_local_horizontal_line_density = shape.circle_local_horizontal_line_density;
        candidate.circle_local_vertical_line_density = shape.circle_local_vertical_line_density;
        result.push_back(std::move(candidate));
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const ComponentCandidate& a, const ComponentCandidate& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

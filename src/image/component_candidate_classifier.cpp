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

#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/image/shape_detector.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

enum class ComponentCandidateKind {
    Enclosure,
    CircularSymbol,
    ChassisGround,
    PrimitiveSymbol,
    Unknown
};

struct ComponentCandidate {
    std::string id;
    ComponentCandidateKind kind = ComponentCandidateKind::Unknown;
    std::vector<std::string> shape_ids;
    BoundingBox bounds {};
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
};

class ComponentCandidateClassifier {
public:
    [[nodiscard]] std::vector<ComponentCandidate> classify(
        const ShapeDetectionArtifacts& shapes) const;
};

} // namespace eke::dx::wire

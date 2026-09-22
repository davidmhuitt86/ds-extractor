#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/image/shape_detector.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

class ComponentCandidateClassifier {
public:
    [[nodiscard]] std::vector<ComponentCandidate> classify(
        const ShapeDetectionArtifacts& shapes) const;
};

} // namespace eke::dx::wire

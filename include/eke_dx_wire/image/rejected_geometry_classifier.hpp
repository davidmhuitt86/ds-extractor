#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

struct GeometryClassificationConfig {
    double component_proximity = 8.0;
    double text_proximity = 6.0;
};

class RejectedGeometryClassifier {
public:
    explicit RejectedGeometryClassifier(
        GeometryClassificationConfig config = {});

    void classify(
        std::vector<RejectedGeometryEvidence>& rejected,
        const std::vector<ComponentCandidate>& components,
        const std::vector<TextRegion>& text_regions) const;

private:
    GeometryClassificationConfig config_;
};

} // namespace eke::dx::wire

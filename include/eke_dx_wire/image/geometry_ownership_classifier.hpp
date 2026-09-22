#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/image/text_region_detector.hpp"

#include <vector>

namespace eke::dx::wire {

struct GeometryOwnershipConfig {
    // A candidate is considered owned by a graphical object only when a
    // substantial portion of its geometry lies inside that object's bounds.
    // Endpoint proximity alone is deliberately insufficient because a real
    // conductor commonly terminates at a component or connector.
    double component_overlap_fraction = 0.50;
    double text_overlap_fraction = 0.25;
};

struct GeometryOwnershipArtifacts {
    std::vector<ConductorSegment> conductor_candidates;
    std::vector<RejectedGeometryEvidence> rejected;
};

class GeometryOwnershipClassifier {
public:
    explicit GeometryOwnershipClassifier(
        GeometryOwnershipConfig config = {});

    [[nodiscard]] GeometryOwnershipArtifacts classify(
        const std::vector<ConductorSegment>& candidates,
        const std::vector<ComponentCandidate>& components,
        const std::vector<TextRegion>& text_regions) const;

private:
    GeometryOwnershipConfig config_;
};

} // namespace eke::dx::wire

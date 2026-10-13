#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/image/shape_detector.hpp"
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

    // AP-DIAG-FIX-001: a candidate whose two ends lie on the same edge of a
    // component's own established bounds, within this many pixels, is that
    // component's own boundary/housing outline rather than a conductor - a
    // real lead terminates AT a boundary point, it does not run coincident
    // with the boundary line itself. Sized to the observed gap between a
    // drawn outline stroke and the component's own measured bounds (~1px)
    // plus stroke width, not a generic proximity threshold.
    double component_boundary_tolerance_px = 2.0;
};

// Effective connector bounds are used only when conductor geometry provides
// independent evidence of a complete rectilinear housing frame. Otherwise the
// detector's original bounds remain authoritative.
struct ConnectorBoundaryResolution {
    std::string connector_region_id;
    BoundingBox detected_bounds {};
    BoundingBox effective_bounds {};
    bool reconstructed_from_rectangular_strokes = false;
};

struct GeometryOwnershipArtifacts {
    std::vector<ConductorSegment> conductor_candidates;
    std::vector<RejectedGeometryEvidence> rejected;
    std::vector<ConnectorBoundaryResolution> connector_boundaries;
};

class GeometryOwnershipClassifier {
public:
    explicit GeometryOwnershipClassifier(
        GeometryOwnershipConfig config = {});

    [[nodiscard]] GeometryOwnershipArtifacts classify(
        const std::vector<ConductorSegment>& candidates,
        const std::vector<ComponentCandidate>& components,
        const std::vector<TextRegion>& text_regions,
        const std::vector<ShapeRegion>& connector_regions = {}) const;

private:
    GeometryOwnershipConfig config_;
};

} // namespace eke::dx::wire

#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace eke::dx::wire {

struct SymbolGeometryExtractorConfig {
    // Binary ink threshold (same convention as ShapeDetector).
    int ink_threshold = 180;

    // Pixels zeroed at every edge of a component's cropped region before
    // connected-component analysis. This is what excludes the component's
    // own outer boundary stroke (already modeled as its ShapeRegion) from
    // being rediscovered as an "internal" primitive. Real leads that
    // extend to/through the boundary are still detected - see
    // SymbolPrimitiveKind::TerminalLead.
    int boundary_margin = 2;

    // Connected-component blobs smaller than this are treated as noise
    // and never become a SymbolPrimitive (AP-WIRE-023 must not report a
    // false-positive geometry explosion).
    int min_primitive_area = 5;

    // A blob is Line-like when its long/short bounding-box ratio is at
    // least this and its short side is no thicker than line_max_thickness.
    double line_aspect_threshold = 3.0;
    int line_max_thickness = 3;

    // A blob is Circle-like when its fill ratio (area / bbox area) is at
    // least circle_fill_ratio, its bounding box is roughly square (aspect
    // <= circle_max_aspect), and its contour circularity
    // (4*pi*area/perimeter^2) is at least circle_min_circularity.
    double circle_fill_ratio = 0.70;
    double circle_max_aspect = 1.30;
    double circle_min_circularity = 0.75;

    // A blob is Rectangle-like when its fill ratio is at least
    // rect_fill_ratio and its aspect ratio is no more than rect_max_aspect
    // (beyond that, an elongated filled blob is more likely a thick line).
    double rect_fill_ratio = 0.75;
    double rect_max_aspect = 2.5;

    // A blob that touches the cropped region's boundary margin is a
    // TerminalLead candidate only when it is also elongated by at least
    // this aspect ratio; otherwise it is left Unknown rather than forced
    // into a lead interpretation it does not support.
    double lead_min_aspect = 2.0;
};

struct SymbolGeometryExtractionArtifacts {
    std::vector<ComponentSymbolGeometry> geometries;
    std::vector<SymbolPrimitive> primitives;
};

// AP-WIRE-023: extracts deterministic internal geometric primitives from
// each real (non-DiagramFurniture) ComponentCandidate's already-detected
// bounding region. This stage does not assign symbol-family identity, does
// not create EndpointCandidates, and does not mutate topology, wires, or
// electrical nets. DiagramFurniture candidates never enter this path.
class SymbolGeometryExtractor {
public:
    explicit SymbolGeometryExtractor(SymbolGeometryExtractorConfig config = {});

    [[nodiscard]] SymbolGeometryExtractionArtifacts extract(
        const cv::Mat& normalized,
        const std::vector<ComponentCandidate>& components,
        const std::string& source_id,
        int page) const;

private:
    SymbolGeometryExtractorConfig config_;
};

} // namespace eke::dx::wire

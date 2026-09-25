#include "eke_dx_wire/image/symbol_geometry_extractor.hpp"
#include "eke_dx_wire/core/ids.hpp"

#include <opencv2/geometry/2d.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace eke::dx::wire {
namespace {

std::string primitive_kind_token(SymbolPrimitiveKind kind) {
    switch (kind) {
    case SymbolPrimitiveKind::Line: return "line";
    case SymbolPrimitiveKind::Circle: return "circle";
    case SymbolPrimitiveKind::Rectangle: return "rectangle";
    case SymbolPrimitiveKind::TerminalLead: return "terminal_lead";
    case SymbolPrimitiveKind::Unknown: return "unknown";
    }
    return "unknown";
}

struct ClassifiedBlob {
    BoundingBox bounds {};
    double area = 0.0;
    SymbolPrimitiveKind kind = SymbolPrimitiveKind::Unknown;
    ConfidenceClass confidence = ConfidenceClass::Low;
};

ClassifiedBlob classify_blob(
    const cv::Mat& blob_mask,
    const cv::Rect& local_bounds,
    double area,
    bool touches_margin,
    const SymbolGeometryExtractorConfig& config) {

    const int w = local_bounds.width;
    const int h = local_bounds.height;
    const int long_side = std::max(w, h);
    const int short_side = std::max(1, std::min(w, h));
    const double aspect = double(long_side) / double(short_side);
    const double fill_ratio = area / double(std::max(1, w * h));

    ClassifiedBlob result;
    result.area = area;

    // Circle: compact, roughly square bounding box, high fill, and a
    // contour circularity check so a filled square is not mistaken for a
    // circle merely because its bounding box is square.
    if (fill_ratio >= config.circle_fill_ratio &&
        aspect <= config.circle_max_aspect) {
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(blob_mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        double best_circularity = 0.0;
        for (const auto& contour : contours) {
            const double perimeter = cv::arcLength(contour, true);
            if (perimeter <= 0.0) continue;
            const double contour_area = cv::contourArea(contour);
            const double circularity =
                4.0 * CV_PI * contour_area / (perimeter * perimeter);
            best_circularity = std::max(best_circularity, circularity);
        }
        if (best_circularity >= config.circle_min_circularity) {
            result.kind = SymbolPrimitiveKind::Circle;
            result.confidence =
                best_circularity >= 0.85 ? ConfidenceClass::High : ConfidenceClass::Medium;
            return result;
        }
    }

    // Line / TerminalLead: thin and elongated.
    if (aspect >= config.line_aspect_threshold &&
        short_side <= config.line_max_thickness) {
        if (touches_margin && aspect >= config.lead_min_aspect) {
            result.kind = SymbolPrimitiveKind::TerminalLead;
            result.confidence =
                aspect >= config.line_aspect_threshold * 1.5
                    ? ConfidenceClass::Medium
                    : ConfidenceClass::Low;
        } else {
            result.kind = SymbolPrimitiveKind::Line;
            result.confidence =
                short_side <= 2 ? ConfidenceClass::High : ConfidenceClass::Medium;
        }
        return result;
    }

    // Rectangle: filled, bounded aspect, not already claimed as a circle.
    if (fill_ratio >= config.rect_fill_ratio && aspect <= config.rect_max_aspect) {
        result.kind = SymbolPrimitiveKind::Rectangle;
        result.confidence =
            fill_ratio >= 0.9 ? ConfidenceClass::High : ConfidenceClass::Medium;
        return result;
    }

    // A thin-but-elongated blob touching the margin that did not qualify
    // above (e.g. slightly thicker than line_max_thickness) is still
    // reasonable lead evidence rather than forced Unknown.
    if (touches_margin && aspect >= config.lead_min_aspect) {
        result.kind = SymbolPrimitiveKind::TerminalLead;
        result.confidence = ConfidenceClass::Low;
        return result;
    }

    result.kind = SymbolPrimitiveKind::Unknown;
    result.confidence = ConfidenceClass::Low;
    return result;
}

// Perpendicular distance from `point` to the finite segment `segment`,
// clamping the projection to the segment's own span - a point beyond
// either endpoint is measured to that endpoint, never treated as "on"
// an infinite extension of the line.
double point_to_segment_distance(const Point2D& point, const Segment2D& segment) {
    const double dx = segment.b.x - segment.a.x;
    const double dy = segment.b.y - segment.a.y;
    const double length_sq = dx * dx + dy * dy;
    if (length_sq <= 1e-9) {
        return distance(point, segment.a);
    }
    double t = ((point.x - segment.a.x) * dx + (point.y - segment.a.y) * dy) / length_sq;
    t = std::clamp(t, 0.0, 1.0);
    const Point2D closest{segment.a.x + t * dx, segment.a.y + t * dy};
    return distance(point, closest);
}

// AP-WIRE-FIX-001: a blob is already-accounted-for external conductor
// ink - not a genuine internal component primitive - when its entire
// bounding box lies within an existing ConductorSegment's own drawn
// stroke width. Requiring all four corners within tolerance of the same
// segment keeps this a narrow "is this the same stroke" test: a
// component's own distinct internal geometry that merely sits near, but
// is not aligned with, a conductor's centerline is not excluded.
bool blob_is_existing_conductor_ink(
    const BoundingBox& bounds,
    const std::vector<ConductorSegment>& conductor_segments,
    double slack_px) {

    const Point2D corners[4] = {
        {double(bounds.x), double(bounds.y)},
        {double(bounds.x + bounds.width), double(bounds.y)},
        {double(bounds.x), double(bounds.y + bounds.height)},
        {double(bounds.x + bounds.width), double(bounds.y + bounds.height)},
    };

    for (const auto& segment : conductor_segments) {
        const double tolerance = segment.thickness_px / 2.0 + slack_px;
        bool all_corners_covered = true;
        for (const auto& corner : corners) {
            if (point_to_segment_distance(corner, segment.geometry) > tolerance) {
                all_corners_covered = false;
                break;
            }
        }
        if (all_corners_covered) {
            return true;
        }
    }
    return false;
}

} // namespace

SymbolGeometryExtractor::SymbolGeometryExtractor(SymbolGeometryExtractorConfig config)
    : config_(config) {}

SymbolGeometryExtractionArtifacts SymbolGeometryExtractor::extract(
    const cv::Mat& normalized,
    const std::vector<ComponentCandidate>& components,
    const std::vector<ConductorSegment>& conductor_segments,
    const std::string& source_id,
    int page) const {

    SymbolGeometryExtractionArtifacts artifacts;
    if (normalized.empty()) {
        return artifacts;
    }

    cv::Mat gray;
    if (normalized.channels() > 1) {
        cv::cvtColor(normalized, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = normalized;
    }

    for (const auto& component : components) {
        // AP-WIRE-023 section 6: DiagramFurniture must never enter the
        // electrical symbol-geometry extraction path.
        if (component.kind == ComponentCandidateKind::DiagramFurniture) {
            continue;
        }
        if (component.id.empty()) {
            continue;
        }

        cv::Rect roi(
            component.bounds.x, component.bounds.y,
            component.bounds.width, component.bounds.height);
        roi &= cv::Rect(0, 0, gray.cols, gray.rows);

        ComponentSymbolGeometry geometry;
        geometry.id = "component-symbol-geometry-" + component.id;
        geometry.component_id = component.id;

        if (roi.width <= 0 || roi.height <= 0) {
            geometry.confidence = ConfidenceClass::Unresolved;
            artifacts.geometries.push_back(std::move(geometry));
            continue;
        }

        cv::Mat region = gray(roi);
        cv::Mat binary;
        cv::threshold(region, binary, config_.ink_threshold, 255, cv::THRESH_BINARY_INV);

        // Exclude the component's own outer boundary stroke: it is already
        // modeled as the candidate's ShapeRegion, not internal geometry.
        const int margin = std::min(
            config_.boundary_margin,
            std::min(binary.cols, binary.rows) / 2);
        if (margin > 0) {
            binary(cv::Rect(0, 0, binary.cols, margin)).setTo(0);
            binary(cv::Rect(0, binary.rows - margin, binary.cols, margin)).setTo(0);
            binary(cv::Rect(0, 0, margin, binary.rows)).setTo(0);
            binary(cv::Rect(binary.cols - margin, 0, margin, binary.rows)).setTo(0);
        }

        cv::Mat labels, stats, centroids;
        const int component_count =
            cv::connectedComponentsWithStats(binary, labels, stats, centroids, 8, CV_32S);

        std::vector<std::pair<Point2D, ClassifiedBlob>> blobs;
        for (int label = 1; label < component_count; ++label) {
            const int area = stats.at<int>(label, cv::CC_STAT_AREA);
            if (area < config_.min_primitive_area) {
                continue;
            }

            const cv::Rect local_bounds(
                stats.at<int>(label, cv::CC_STAT_LEFT),
                stats.at<int>(label, cv::CC_STAT_TOP),
                stats.at<int>(label, cv::CC_STAT_WIDTH),
                stats.at<int>(label, cv::CC_STAT_HEIGHT));

            const bool touches_margin =
                margin > 0 &&
                (local_bounds.x <= margin ||
                 local_bounds.y <= margin ||
                 local_bounds.x + local_bounds.width >= binary.cols - margin ||
                 local_bounds.y + local_bounds.height >= binary.rows - margin);

            cv::Mat blob_mask = (labels(local_bounds) == label);

            ClassifiedBlob blob = classify_blob(
                blob_mask, local_bounds, double(area), touches_margin, config_);

            blob.bounds = BoundingBox{
                roi.x + local_bounds.x, roi.y + local_bounds.y,
                local_bounds.width, local_bounds.height};

            // AP-WIRE-FIX-001: this component's bounding box can overlap
            // a neighboring component's box (both real, adjacent
            // symbols). A blob that is actually an existing conductor's
            // own drawn stroke - most commonly a wire's exit lead
            // already correctly excluded as ITS OWN component's
            // boundary - must not be re-attributed as internal geometry
            // of a different, merely-overlapping component. Evidence
            // that already exists (the normalized ConductorSegment) is
            // what makes this exclusion, not a guess about ownership.
            if (blob_is_existing_conductor_ink(
                    blob.bounds, conductor_segments,
                    config_.conductor_exclusion_slack_px)) {
                continue;
            }

            blobs.emplace_back(
                Point2D{double(local_bounds.y), double(local_bounds.x)}, blob);
        }

        // Deterministic within-component ordering: top-to-bottom,
        // left-to-right. Global ordering across components is deterministic
        // by iterating `components` in the caller-provided order.
        std::sort(
            blobs.begin(), blobs.end(),
            [](const auto& a, const auto& b) {
                if (a.first.x != b.first.x) return a.first.x < b.first.x;
                return a.first.y < b.first.y;
            });

        bool any_high = false;
        bool any_medium = false;
        bool any_low = false;

        for (const auto& [_, blob] : blobs) {
            std::ostringstream signature;
            signature << component.id << ':' << blob.bounds.x << ','
                      << blob.bounds.y << ',' << blob.bounds.width << ','
                      << blob.bounds.height << ',' << primitive_kind_token(blob.kind);

            SymbolPrimitive primitive;
            primitive.id = stable_id("symbol-primitive", signature.str());
            primitive.component_id = component.id;
            primitive.kind = blob.kind;
            primitive.bounds = blob.bounds;
            primitive.area = blob.area;
            primitive.confidence = blob.confidence;
            primitive.provenance.source_id = source_id;
            primitive.provenance.page = page;
            primitive.provenance.source_region = blob.bounds;
            primitive.provenance.stage = "symbol_geometry_extractor";

            switch (primitive.confidence) {
            case ConfidenceClass::High: any_high = true; break;
            case ConfidenceClass::Medium: any_medium = true; break;
            case ConfidenceClass::Low: any_low = true; break;
            case ConfidenceClass::Unresolved: break;
            }

            geometry.primitive_ids.push_back(primitive.id);
            artifacts.primitives.push_back(std::move(primitive));
        }

        geometry.confidence =
            geometry.primitive_ids.empty() ? ConfidenceClass::Unresolved
            : any_high ? ConfidenceClass::High
            : any_medium ? ConfidenceClass::Medium
            : any_low ? ConfidenceClass::Low
            : ConfidenceClass::Unresolved;

        artifacts.geometries.push_back(std::move(geometry));
    }

    std::sort(
        artifacts.geometries.begin(), artifacts.geometries.end(),
        [](const auto& a, const auto& b) { return a.id < b.id; });
    std::sort(
        artifacts.primitives.begin(), artifacts.primitives.end(),
        [](const auto& a, const auto& b) { return a.id < b.id; });

    return artifacts;
}

} // namespace eke::dx::wire

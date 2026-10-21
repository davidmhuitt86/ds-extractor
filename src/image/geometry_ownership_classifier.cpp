#include "eke_dx_wire/image/geometry_ownership_classifier.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace eke::dx::wire {
namespace {

double point_rect_distance(
    const Point2D& point,
    const BoundingBox& box) {

    const double left = static_cast<double>(box.x);
    const double right = left + box.width;
    const double top = static_cast<double>(box.y);
    const double bottom = top + box.height;

    const double dx =
        point.x < left ? left - point.x :
        point.x > right ? point.x - right : 0.0;
    const double dy =
        point.y < top ? top - point.y :
        point.y > bottom ? point.y - bottom : 0.0;

    return std::hypot(dx, dy);
}

// Returns the length of the portion of a segment contained by an axis-aligned
// rectangle. This is a parametric clipping operation, so long segments are
// evaluated across their full geometry rather than only at endpoints or the
// midpoint.
double segment_inside_length(
    const Segment2D& segment,
    const BoundingBox& box) {

    const double dx = segment.b.x - segment.a.x;
    const double dy = segment.b.y - segment.a.y;
    const double length = std::hypot(dx, dy);

    if (length <= 1e-12)
        return 0.0;

    const double left = static_cast<double>(box.x);
    const double right = left + box.width;
    const double top = static_cast<double>(box.y);
    const double bottom = top + box.height;

    double t0 = 0.0;
    double t1 = 1.0;

    const auto clip = [&](double p, double q) {
        if (std::abs(p) <= 1e-12)
            return q >= 0.0;

        const double r = q / p;
        if (p < 0.0) {
            if (r > t1)
                return false;
            if (r > t0)
                t0 = r;
        } else {
            if (r < t0)
                return false;
            if (r < t1)
                t1 = r;
        }
        return true;
    };

    if (!clip(-dx, segment.a.x - left) ||
        !clip(dx, right - segment.a.x) ||
        !clip(-dy, segment.a.y - top) ||
        !clip(dy, bottom - segment.a.y)) {
        return 0.0;
    }

    if (t1 <= t0)
        return 0.0;

    return (t1 - t0) * length;
}

bool point_strictly_inside_rect(
    const Point2D& point,
    const BoundingBox& box) {

    constexpr double epsilon = 1e-9;
    const double left = static_cast<double>(box.x);
    const double right = left + box.width;
    const double top = static_cast<double>(box.y);
    const double bottom = top + box.height;

    // Boundary contact is deliberately not treated as graphical ownership.
    // A conductor that terminates at an object's boundary is exactly the
    // interaction we need to preserve for downstream terminal semantics.
    return point.x > left + epsilon &&
           point.x < right - epsilon &&
           point.y > top + epsilon &&
           point.y < bottom - epsilon;
}

bool both_endpoints_strictly_inside(
    const Segment2D& segment,
    const BoundingBox& box) {
    return point_strictly_inside_rect(segment.a, box) &&
           point_strictly_inside_rect(segment.b, box);
}

// AP-DIAG-FIX-001: true when this axis-aligned segment runs ALONG one edge
// of the given component's own established bounds - i.e. it is (a run of)
// that component's own boundary/housing outline, not a conductor. This is
// deliberately narrower than "near" or "inside" the box: a real lead that
// terminates at (or passes through) the boundary approaches it from a
// different axis and only touches it at one point, so it never satisfies
// this test. A segment must be near-horizontal or near-vertical, coincide
// with one specific edge's coordinate within tolerance, and lie within
// that edge's span (with the same tolerance) to qualify.
bool segment_traces_component_boundary(
    const Segment2D& segment,
    const BoundingBox& box,
    double tolerance) {

    const double left = static_cast<double>(box.x);
    const double right = left + box.width;
    const double top = static_cast<double>(box.y);
    const double bottom = top + box.height;

    const bool near_horizontal =
        std::abs(segment.a.y - segment.b.y) <= tolerance;
    const bool near_vertical =
        std::abs(segment.a.x - segment.b.x) <= tolerance;

    // A diagonal segment is never a rectilinear housing-outline stroke.
    if (near_horizontal == near_vertical)
        return near_horizontal &&
            segment.length() <= tolerance; // degenerate point only
    if (near_horizontal) {
        const double y = (segment.a.y + segment.b.y) * 0.5;
        if (std::abs(y - top) > tolerance && std::abs(y - bottom) > tolerance)
            return false;

        const double x_min = (std::min)(segment.a.x, segment.b.x);
        const double x_max = (std::max)(segment.a.x, segment.b.x);
        return x_min >= left - tolerance && x_max <= right + tolerance;
    }

    const double x = (segment.a.x + segment.b.x) * 0.5;
    if (std::abs(x - left) > tolerance && std::abs(x - right) > tolerance)
        return false;

    const double y_min = (std::min)(segment.a.y, segment.b.y);
    const double y_max = (std::max)(segment.a.y, segment.b.y);
    return y_min >= top - tolerance && y_max <= bottom + tolerance;
}

const ComponentCandidate* component_whose_boundary_is_traced(
    const Segment2D& geometry,
    const std::vector<ComponentCandidate>& components,
    double tolerance) {

    const ComponentCandidate* result = nullptr;
    for (const auto& component : components) {
        if (!segment_traces_component_boundary(
                geometry, component.bounds, tolerance)) {
            continue;
        }
        if (result == nullptr || component.id < result->id) {
            result = &component;
        }
    }
    return result;
}

double overlap_fraction(
    const Segment2D& segment,
    const BoundingBox& box) {

    const double length = segment.length();
    if (length <= 1e-12)
        return point_rect_distance(segment.a, box) <= 1e-9 ? 1.0 : 0.0;

    return segment_inside_length(segment, box) / length;
}

struct VerticalStrokeObservation {
    double x = 0.0;
    double top = 0.0;
    double bottom = 0.0;
    std::string id;
};

bool horizontal_cap_matches(
    const ConductorSegment& candidate,
    double x_a,
    double x_b,
    double y,
    double tolerance) {

    const double dx = candidate.geometry.b.x - candidate.geometry.a.x;
    const double dy = candidate.geometry.b.y - candidate.geometry.a.y;
    if (std::abs(dy) > tolerance ||
        std::abs(dx) <= 2.0 * tolerance) {
        return false;
    }

    const double x_min = (std::min)(
        candidate.geometry.a.x, candidate.geometry.b.x);
    const double x_max = (std::max)(
        candidate.geometry.a.x, candidate.geometry.b.x);
    const double expected_min = (std::min)(x_a, x_b);
    const double expected_max = (std::max)(x_a, x_b);
    const double center_y =
        (candidate.geometry.a.y + candidate.geometry.b.y) * 0.5;

    // A cap must span both observed vertical strokes and terminate near them.
    // A long conductor that merely crosses the same y coordinate is not a cap.
    return std::abs(center_y - y) <= tolerance &&
           x_min >= expected_min - tolerance &&
           x_min <= expected_min + tolerance &&
           x_max >= expected_max - tolerance &&
           x_max <= expected_max + tolerance;
}

bool has_horizontal_cap(
    const std::vector<ConductorSegment>& candidates,
    double x_a,
    double x_b,
    double y,
    double tolerance,
    double& best_y_error) {

    bool found = false;
    best_y_error = std::numeric_limits<double>::max();

    for (const auto& candidate : candidates) {
        const double dy =
            std::abs(candidate.geometry.a.y - candidate.geometry.b.y);
        if (dy > tolerance)
            continue;

        const double center_y =
            (candidate.geometry.a.y + candidate.geometry.b.y) * 0.5;
        if (!horizontal_cap_matches(
                candidate, x_a, x_b, y, tolerance)) {
            continue;
        }

        const double error = std::abs(center_y - y);
        if (!found || error < best_y_error) {
            found = true;
            best_y_error = error;
        }
    }

    return found;
}

ConnectorBoundaryResolution resolve_connector_boundary(
    const ShapeRegion& region,
    const std::vector<ConductorSegment>& candidates,
    double tolerance) {

    ConnectorBoundaryResolution result;
    result.connector_region_id = region.id;
    result.detected_bounds = region.bounds;
    result.effective_bounds = region.bounds;

    if (region.kind != ShapeKind::ConnectorBody ||
        region.bounds.width <= 0 || region.bounds.height <= 0) {
        return result;
    }

    const double left = static_cast<double>(region.bounds.x);
    const double right =
        static_cast<double>(region.bounds.x + region.bounds.width);
    const double top = static_cast<double>(region.bounds.y);
    const double bottom =
        static_cast<double>(region.bounds.y + region.bounds.height);

    std::vector<VerticalStrokeObservation> verticals;
    for (const auto& candidate : candidates) {
        const double dx =
            std::abs(candidate.geometry.a.x - candidate.geometry.b.x);
        const double dy =
            std::abs(candidate.geometry.a.y - candidate.geometry.b.y);
        if (dx > tolerance || dy < (std::max)(8.0, 2.0 * tolerance))
            continue;

        const double x =
            (candidate.geometry.a.x + candidate.geometry.b.x) * 0.5;
        const double y_min = (std::min)(
            candidate.geometry.a.y, candidate.geometry.b.y);
        const double y_max = (std::max)(
            candidate.geometry.a.y, candidate.geometry.b.y);

        // A candidate stroke must span the detected connector region's height.
        // This excludes nearby vertical conductors that do not frame the body.
        if (y_min > top + tolerance || y_max < bottom - tolerance)
            continue;

        verticals.push_back({x, y_min, y_max, candidate.id});
    }

    std::sort(
        verticals.begin(), verticals.end(),
        [](const VerticalStrokeObservation& a,
           const VerticalStrokeObservation& b) {
            return std::tie(a.x, a.top, a.bottom, a.id) <
                   std::tie(b.x, b.top, b.bottom, b.id);
        });

    double best_score = std::numeric_limits<double>::max();
    bool found_frame = false;
    double best_left = left;
    double best_right = right;
    double best_top = top;
    double best_bottom = bottom;

    // Reconstruct an outer connector envelope only when:
    //   1. one long vertical stroke lies along a detected bbox side;
    //   2. a parallel stroke extends beyond the opposite bbox side;
    //   3. the two strokes have matching vertical extent; and
    //   4. separate horizontal strokes connect those same verticals at both
    //      ends. This is direct frame evidence, not a proximity mask.
    const double max_extension =
        (std::max)(12.0, 1.5 * static_cast<double>(region.bounds.width));

    for (const auto& anchor : verticals) {
        const bool anchor_near_left =
            std::abs(anchor.x - left) <= tolerance;
        const bool anchor_near_right =
            std::abs(anchor.x - right) <= tolerance;
        if (anchor_near_left == anchor_near_right)
            continue;

        for (const auto& outer : verticals) {
            if (std::abs(anchor.x - outer.x) <= 2.0 * tolerance)
                continue;

            const bool outer_is_opposite =
                anchor_near_right
                    ? outer.x < left - tolerance
                    : outer.x > right + tolerance;
            if (!outer_is_opposite)
                continue;

            const double extension =
                anchor_near_right ? left - outer.x : outer.x - right;
            if (extension > max_extension)
                continue;

            if (std::abs(anchor.top - outer.top) > tolerance ||
                std::abs(anchor.bottom - outer.bottom) > tolerance) {
                continue;
            }

            const double frame_top = (anchor.top + outer.top) * 0.5;
            const double frame_bottom =
                (anchor.bottom + outer.bottom) * 0.5;
            double top_cap_error = 0.0;
            double bottom_cap_error = 0.0;
            if (!has_horizontal_cap(
                    candidates, anchor.x, outer.x, frame_top,
                    tolerance, top_cap_error) ||
                !has_horizontal_cap(
                    candidates, anchor.x, outer.x, frame_bottom,
                    tolerance, bottom_cap_error)) {
                continue;
            }

            const double score =
                std::abs(anchor.top - outer.top) +
                std::abs(anchor.bottom - outer.bottom) +
                top_cap_error + bottom_cap_error +
                extension * 0.001;
            if (score >= best_score)
                continue;

            best_score = score;
            found_frame = true;
            best_left = (std::min)(
                left, (std::min)(anchor.x, outer.x));
            best_right = (std::max)(
                right, (std::max)(anchor.x, outer.x));
            best_top = (std::min)(
                top, (std::min)(anchor.top, outer.top));
            best_bottom = (std::max)(
                bottom, (std::max)(anchor.bottom, outer.bottom));
        }
    }

    if (!found_frame)
        return result;

    const int resolved_left = static_cast<int>(std::floor(best_left));
    const int resolved_right = static_cast<int>(std::ceil(best_right));
    const int resolved_top = static_cast<int>(std::floor(best_top));
    const int resolved_bottom = static_cast<int>(std::ceil(best_bottom));

    result.effective_bounds = {
        resolved_left,
        resolved_top,
        resolved_right - resolved_left,
        resolved_bottom - resolved_top};
    result.reconstructed_from_rectangular_strokes = true;
    return result;
}

RejectedGeometryClass rejected_class_for_component(
    ComponentCandidateKind kind) {

    return kind == ComponentCandidateKind::PrimitiveSymbol
        ? RejectedGeometryClass::ConnectorAssociated
        : RejectedGeometryClass::ComponentAssociated;
}

const ComponentCandidate* nearest_component(
    const Segment2D& geometry,
    const std::vector<ComponentCandidate>& components,
    double& best_distance) {

    const ComponentCandidate* result = nullptr;
    best_distance = std::numeric_limits<double>::max();

    const Point2D midpoint{
        (geometry.a.x + geometry.b.x) * 0.5,
        (geometry.a.y + geometry.b.y) * 0.5};

    for (const auto& component : components) {
        const double distance = std::min({
            point_rect_distance(geometry.a, component.bounds),
            point_rect_distance(geometry.b, component.bounds),
            point_rect_distance(midpoint, component.bounds)
        });

        if (distance < best_distance ||
            (distance == best_distance &&
             result != nullptr && component.id < result->id)) {
            best_distance = distance;
            result = &component;
        }
    }

    return result;
}

const TextRegion* nearest_text(
    const Segment2D& geometry,
    const std::vector<TextRegion>& text_regions,
    double& best_distance) {

    const TextRegion* result = nullptr;
    best_distance = std::numeric_limits<double>::max();

    const Point2D midpoint{
        (geometry.a.x + geometry.b.x) * 0.5,
        (geometry.a.y + geometry.b.y) * 0.5};

    for (const auto& text : text_regions) {
        const double distance = std::min({
            point_rect_distance(geometry.a, text.bounds),
            point_rect_distance(geometry.b, text.bounds),
            point_rect_distance(midpoint, text.bounds)
        });

        if (distance < best_distance ||
            (distance == best_distance &&
             result != nullptr && text.id < result->id)) {
            best_distance = distance;
            result = &text;
        }
    }

    return result;
}

} // namespace

GeometryOwnershipClassifier::GeometryOwnershipClassifier(
    GeometryOwnershipConfig config)
    : config_(config) {}

GeometryOwnershipArtifacts GeometryOwnershipClassifier::classify(
    const std::vector<ConductorSegment>& candidates,
    const std::vector<ComponentCandidate>& components,
    const std::vector<TextRegion>& text_regions,
    const std::vector<ShapeRegion>& connector_regions) const {

    GeometryOwnershipArtifacts result;

    result.connector_boundaries.reserve(connector_regions.size());
    for (const auto& region : connector_regions) {
        if (region.kind != ShapeKind::ConnectorBody)
            continue;
        result.connector_boundaries.push_back(
            resolve_connector_boundary(
                region, candidates,
                config_.component_boundary_tolerance_px));
    }
    std::sort(
        result.connector_boundaries.begin(),
        result.connector_boundaries.end(),
        [](const ConnectorBoundaryResolution& a,
           const ConnectorBoundaryResolution& b) {
            return a.connector_region_id < b.connector_region_id;
        });

    for (const auto& candidate : candidates) {
        const ComponentCandidate* owner_component = nullptr;
        double best_component_overlap = 0.0;

        for (const auto& component : components) {
            const double fraction =
                overlap_fraction(candidate.geometry, component.bounds);

            if (fraction > best_component_overlap ||
                (fraction == best_component_overlap &&
                 fraction > 0.0 &&
                 owner_component != nullptr &&
                 component.id < owner_component->id)) {
                best_component_overlap = fraction;
                owner_component = &component;
            }
        }

        const TextRegion* owner_text = nullptr;
        double best_text_overlap = 0.0;

        for (const auto& text : text_regions) {
            const double fraction =
                overlap_fraction(candidate.geometry, text.bounds);

            if (fraction > best_text_overlap ||
                (fraction == best_text_overlap &&
                 fraction > 0.0 &&
                 owner_text != nullptr &&
                 text.id < owner_text->id)) {
                best_text_overlap = fraction;
                owner_text = &text;
            }
        }

        // AP-GEOMETRY-007: overlap alone is not sufficient to establish
        // graphical ownership. A conductor may cross an object's bounding
        // box, enter an object and terminate at its boundary, or be detected
        // from geometry that partially overlaps an object. Those are all
        // conductor/object interactions, not proof that the entire line-like
        // geometry belongs to the object.
        //
        // Ownership is therefore limited to geometry whose two true geometric
        // ends lie strictly inside the same object bounds. Boundary contact
        // remains conductor evidence so terminal-location and semantic stages
        // can interpret the attachment later.
        const bool component_geometry_internal =
            owner_component != nullptr &&
            both_endpoints_strictly_inside(
                candidate.geometry,
                owner_component->bounds);

        const bool text_geometry_internal =
            owner_text != nullptr &&
            both_endpoints_strictly_inside(
                candidate.geometry,
                owner_text->bounds);

        const bool component_owned =
            component_geometry_internal &&
            best_component_overlap >= config_.component_overlap_fraction;

        const bool text_owned =
            text_geometry_internal &&
            best_text_overlap >= config_.text_overlap_fraction;

        // When both object classes overlap the geometry, retain the more
        // strongly supported ownership. Text wins only when its overlap is
        // strictly greater; ties remain component-associated.
        if (component_owned || text_owned) {
            RejectedGeometryEvidence evidence;
            evidence.id = candidate.id;
            evidence.geometry = candidate.geometry;
            evidence.provenance = candidate.provenance;

            if (component_owned &&
                (!text_owned ||
                 best_component_overlap >= best_text_overlap)) {
                evidence.classification =
                    rejected_class_for_component(owner_component->kind);
                evidence.associated_object_id = owner_component->id;
                evidence.reason =
                    "graphical_object_ownership_component_overlap";
                evidence.measurement = best_component_overlap;
            } else {
                evidence.classification =
                    RejectedGeometryClass::TextAssociated;
                evidence.associated_object_id = owner_text->id;
                evidence.reason =
                    "graphical_object_ownership_text_overlap";
                evidence.measurement = best_text_overlap;
            }

            result.rejected.push_back(std::move(evidence));
            continue;
        }

        // AP-DIAG-FIX-001: a component's own boundary/housing outline is
        // drawn ink, but it is not a conductor - the checks above never
        // catch it, because it is deliberately excluded from "inside" (it
        // runs along the boundary, not inside it) and it commonly falls
        // outside the shape detector's own measured bounds by a pixel or
        // two (the stroke sits astride the measured edge). This is an
        // independent, narrower signal: the candidate's own two ends must
        // lie on the same edge of some component's established bounds.
        if (const ComponentCandidate* boundary_owner =
                component_whose_boundary_is_traced(
                    candidate.geometry, components,
                    config_.component_boundary_tolerance_px)) {
            RejectedGeometryEvidence evidence;
            evidence.id = candidate.id;
            evidence.geometry = candidate.geometry;
            evidence.provenance = candidate.provenance;
            evidence.classification = rejected_class_for_component(
                boundary_owner->kind);
            evidence.associated_object_id = boundary_owner->id;
            evidence.reason = "graphical_object_ownership_component_boundary";
            evidence.measurement = config_.component_boundary_tolerance_px;

            result.rejected.push_back(std::move(evidence));
            continue;
        }

        // A connector's contour bounds can omit a notched side that is still
        // part of the drawn housing. Only a boundary reconstructed from two
        // matching vertical strokes and two connecting horizontal caps is
        // strong enough to suppress that housing ink. Real leads that meet
        // the outer boundary perpendicularly do not trace the rectangle edge.
        bool connector_boundary_owned = false;
        for (const auto& boundary : result.connector_boundaries) {
            if (!boundary.reconstructed_from_rectangular_strokes ||
                !segment_traces_component_boundary(
                    candidate.geometry, boundary.effective_bounds,
                    config_.component_boundary_tolerance_px)) {
                continue;
            }

            RejectedGeometryEvidence evidence;
            evidence.id = candidate.id;
            evidence.geometry = candidate.geometry;
            evidence.provenance = candidate.provenance;
            evidence.classification = RejectedGeometryClass::ConnectorAssociated;
            evidence.associated_object_id = boundary.connector_region_id;
            evidence.reason =
                "graphical_object_ownership_connector_boundary";
            evidence.measurement =
                config_.component_boundary_tolerance_px;
            result.rejected.push_back(std::move(evidence));
            connector_boundary_owned = true;
            break;
        }

        if (connector_boundary_owned)
            continue;

        result.conductor_candidates.push_back(candidate);
    }

    return result;
}

} // namespace eke::dx::wire

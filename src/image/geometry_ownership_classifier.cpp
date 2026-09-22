#include "eke_dx_wire/image/geometry_ownership_classifier.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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

double overlap_fraction(
    const Segment2D& segment,
    const BoundingBox& box) {

    const double length = segment.length();
    if (length <= 1e-12)
        return point_rect_distance(segment.a, box) <= 1e-9 ? 1.0 : 0.0;

    return segment_inside_length(segment, box) / length;
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
    const std::vector<TextRegion>& text_regions) const {

    GeometryOwnershipArtifacts result;

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

        result.conductor_candidates.push_back(candidate);
    }

    return result;
}

} // namespace eke::dx::wire

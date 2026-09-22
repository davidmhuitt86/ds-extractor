#include "eke_dx_wire/image/rejected_geometry_classifier.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace eke::dx::wire {
namespace {

double point_rect_distance(
    Point2D point,
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

double point_segment_distance(
    Point2D point,
    Point2D a,
    Point2D b) {

    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double length_sq = dx * dx + dy * dy;
    if (length_sq <= 1e-12) {
        return std::hypot(point.x - a.x, point.y - a.y);
    }

    const double t = std::clamp(
        ((point.x - a.x) * dx + (point.y - a.y) * dy) / length_sq,
        0.0,
        1.0);

    const double px = a.x + t * dx;
    const double py = a.y + t * dy;
    return std::hypot(point.x - px, point.y - py);
}

double segment_box_distance(
    const Segment2D& segment,
    const BoundingBox& box) {

    const double left = static_cast<double>(box.x);
    const double right = left + box.width;
    const double top = static_cast<double>(box.y);
    const double bottom = top + box.height;

    // Zero is returned when the conductor crosses or terminates inside
    // the candidate bounds. Otherwise compare the conductor against all
    // four rectangle edges. This avoids missing a nearby object when a
    // long rejected segment passes alongside its middle.
    const Point2D corners[] = {
        {left, top}, {right, top}, {right, bottom}, {left, bottom}};

    double best = std::numeric_limits<double>::max();

    for (const auto& corner : corners) {
        best = std::min(
            best,
            point_segment_distance(corner, segment.a, segment.b));
    }

    best = std::min(
        best,
        point_rect_distance(segment.a, box));
    best = std::min(
        best,
        point_rect_distance(segment.b, box));

    const Point2D midpoint{
        (segment.a.x + segment.b.x) * 0.5,
        (segment.a.y + segment.b.y) * 0.5};
    best = std::min(best, point_rect_distance(midpoint, box));

    return best;
}

double segment_length(const Segment2D& s) {
    return s.length();
}

} // namespace

RejectedGeometryClassifier::RejectedGeometryClassifier(
    GeometryClassificationConfig config)
    : config_(config) {}

void RejectedGeometryClassifier::classify(
    std::vector<RejectedGeometryEvidence>& rejected,
    const std::vector<ComponentCandidate>& components,
    const std::vector<TextRegion>& text_regions) const {

    for (auto& evidence : rejected) {
        // Preserve ownership decisions made by AP-GEOMETRY-006. This stage
        // only fills unresolved association evidence from earlier filters.
        if (evidence.classification != RejectedGeometryClass::Unresolved)
            continue;

        double best_component = std::numeric_limits<double>::max();
        std::string component_id;

        for (const auto& component : components) {
            const double d =
                segment_box_distance(evidence.geometry, component.bounds);

            if (d < best_component) {
                best_component = d;
                component_id = component.id;
            }
        }

        double best_text = std::numeric_limits<double>::max();
        std::string text_id;

        for (const auto& text : text_regions) {
            const double d =
                segment_box_distance(evidence.geometry, text.bounds);

            if (d < best_text) {
                best_text = d;
                text_id = text.id;
            }
        }

        // Prefer the closest independently detected graphical object.
        // This is association evidence, not semantic proof that the rejected
        // geometry belongs to that object.
        if (best_component <= config_.component_proximity &&
            best_component <= best_text) {
            const auto component_it = std::find_if(
                components.begin(),
                components.end(),
                [&](const ComponentCandidate& candidate) {
                    return candidate.id == component_id;
                });

            if (component_it != components.end() &&
                component_it->kind == ComponentCandidateKind::PrimitiveSymbol) {
                evidence.classification =
                    RejectedGeometryClass::ConnectorAssociated;
            } else {
                evidence.classification =
                    RejectedGeometryClass::ComponentAssociated;
            }
            evidence.associated_object_id = component_id;
        } else if (best_text <= config_.text_proximity) {
            evidence.classification =
                RejectedGeometryClass::TextAssociated;
            evidence.associated_object_id = text_id;
        } else {
            evidence.classification =
                RejectedGeometryClass::Unresolved;
            evidence.associated_object_id.clear();
        }
    }
}

} // namespace eke::dx::wire

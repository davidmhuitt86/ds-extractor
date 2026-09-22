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

double segment_box_distance(
    const Segment2D& segment,
    const BoundingBox& box) {

    // The detector currently produces horizontal/vertical conductors.
    // Endpoint and midpoint tests deliberately keep this classifier
    // conservative while covering attachment geometry.
    const Point2D midpoint{
        (segment.a.x + segment.b.x) * 0.5,
        (segment.a.y + segment.b.y) * 0.5};

    return std::min({
        point_rect_distance(segment.a, box),
        point_rect_distance(segment.b, box),
        point_rect_distance(midpoint, box)});
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
            evidence.classification =
                RejectedGeometryClass::ComponentAssociated;
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

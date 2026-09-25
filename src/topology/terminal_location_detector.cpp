#include "eke_dx_wire/topology/terminal_location_detector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace eke::dx::wire {
namespace {

bool point_inside_rect(
    const Point2D& point,
    const BoundingBox& box) {

    const double left = static_cast<double>(box.x);
    const double right = static_cast<double>(box.x + box.width);
    const double top = static_cast<double>(box.y);
    const double bottom = static_cast<double>(box.y + box.height);

    return point.x >= left && point.x <= right &&
           point.y >= top && point.y <= bottom;
}

double point_to_rect_boundary(
    const Point2D& point,
    const BoundingBox& box) {

    const double left = static_cast<double>(box.x);
    const double right = static_cast<double>(box.x + box.width);
    const double top = static_cast<double>(box.y);
    const double bottom = static_cast<double>(box.y + box.height);

    const bool inside = point_inside_rect(point, box);

    if (inside) {
        const double dl = point.x - left;
        const double dr = right - point.x;
        const double dt = point.y - top;
        const double db = bottom - point.y;
        return (std::min)({dl, dr, dt, db});
    }

    const double dx =
        point.x < left ? left - point.x :
        point.x > right ? point.x - right : 0.0;
    const double dy =
        point.y < top ? top - point.y :
        point.y > bottom ? point.y - bottom : 0.0;

    return std::sqrt(dx * dx + dy * dy);
}

double attachment_distance(
    const Point2D& point,
    const ComponentCandidate& component,
    const TerminalLocationConfig& config) {

    // A terminal may be represented by a conductor endpoint landing inside
    // the detected symbol/enclosure rather than exactly on its outer box.
    // Treat that as a semantic attachment when explicitly enabled.
    if (config.allow_interior_attachment &&
        point_inside_rect(point, component.bounds)) {
        return 0.0;
    }

    return point_to_rect_boundary(point, component.bounds);
}

double point_segment_distance(
    const Point2D& point,
    const Segment2D& segment) {

    const double dx = segment.b.x - segment.a.x;
    const double dy = segment.b.y - segment.a.y;
    const double length_sq = dx * dx + dy * dy;

    if (length_sq <= 1e-12) {
        return std::hypot(
            point.x - segment.a.x,
            point.y - segment.a.y);
    }

    const double t = std::clamp(
        ((point.x - segment.a.x) * dx +
         (point.y - segment.a.y) * dy) /
            length_sq,
        0.0,
        1.0);

    const Point2D projection{
        segment.a.x + t * dx,
        segment.a.y + t * dy};

    return std::hypot(
        point.x - projection.x,
        point.y - projection.y);
}

ConfidenceClass confidence_for_distance(
    double distance,
    const TerminalLocationConfig& config) {

    if (distance <= config.high_confidence_distance)
        return ConfidenceClass::High;
    if (distance <= config.medium_confidence_distance)
        return ConfidenceClass::Medium;
    if (distance <= config.boundary_tolerance)
        return ConfidenceClass::Low;
    return ConfidenceClass::Unresolved;
}

// AP-DIAG-FIX-005: geometric proximity to a component's bounding box is not
// sufficient by itself to establish ComponentTerminal identity - the
// candidate component must have positive engineering evidence connecting
// it to real symbol/connector geometry. This reuses the two ownership
// evidence structures already present in the architecture rather than
// inventing a new one: an owned SymbolPrimitive (any kind - a component may
// legitimately own primitives never classified TerminalLead), or an
// explicit component/connector-associated RejectedGeometryEvidence entry
// (geometry independently attributed to this component's boundary by
// GeometryOwnershipClassifier, for symbols whose terminal geometry was not
// captured as a SymbolPrimitive at all). A component with neither is not
// distinguishable from unrelated geometry (e.g. an annotation glyph) that
// happens to be classified as a component candidate.
bool has_ownership_evidence(
    const ComponentCandidate& component,
    const std::vector<SymbolPrimitive>& symbol_primitives,
    const std::vector<RejectedGeometryEvidence>& rejected_geometry) {

    for (const auto& primitive : symbol_primitives) {
        if (primitive.component_id == component.id)
            return true;
    }

    for (const auto& evidence : rejected_geometry) {
        const bool associated =
            (evidence.classification ==
                 RejectedGeometryClass::ComponentAssociated ||
             evidence.classification ==
                 RejectedGeometryClass::ConnectorAssociated) &&
            evidence.associated_object_id == component.id;
        if (associated)
            return true;
    }

    return false;
}

TerminalCandidateKind kind_for_component(ComponentCandidateKind kind) {
    switch (kind) {
    case ComponentCandidateKind::ChassisGround:
        return TerminalCandidateKind::GroundConnection;
    case ComponentCandidateKind::Enclosure:
        return TerminalCandidateKind::ComponentBoundary;
    case ComponentCandidateKind::CircularSymbol:
        return TerminalCandidateKind::ComponentBoundary;
    case ComponentCandidateKind::PrimitiveSymbol:
        return TerminalCandidateKind::ConnectorBoundary;
    default:
        return TerminalCandidateKind::Unknown;
    }
}

} // namespace

TerminalLocationDetector::TerminalLocationDetector(
    TerminalLocationConfig config)
    : config_(config) {}

TerminalLocationArtifacts TerminalLocationDetector::detect(
    const std::vector<ComponentCandidate>& components,
    const std::vector<EndpointCandidate>& endpoints,
    const std::vector<RejectedGeometryEvidence>& rejected_geometry,
    const std::vector<SymbolPrimitive>& symbol_primitives) const {

    TerminalLocationArtifacts result;

    for (const auto& endpoint : endpoints) {
        double best_distance = std::numeric_limits<double>::max();
        const ComponentCandidate* best_component = nullptr;

        for (const auto& component : components) {
            if (!has_ownership_evidence(
                    component, symbol_primitives, rejected_geometry))
                continue;

            double distance =
                attachment_distance(endpoint.position, component, config_);

            // Rejected geometry that was independently associated with this
            // component/connector is additional terminal evidence. This is
            // especially important when a connector body or internal symbol
            // geometry is not represented by the primitive's outer bounds.
            for (const auto& evidence : rejected_geometry) {
                const bool associated =
                    (evidence.classification ==
                         RejectedGeometryClass::ComponentAssociated ||
                     evidence.classification ==
                         RejectedGeometryClass::ConnectorAssociated) &&
                    evidence.associated_object_id == component.id;

                if (!associated)
                    continue;

                distance = (std::min)(
                    distance,
                    point_segment_distance(
                        endpoint.position,
                        evidence.geometry));
            }

            if (config_.require_component_boundary_proximity &&
                distance > config_.boundary_tolerance)
                continue;

            if (distance < best_distance ||
                (distance == best_distance &&
                 best_component != nullptr &&
                 component.id < best_component->id)) {
                best_distance = distance;
                best_component = &component;
            }
        }

        if (best_component == nullptr) {
            continue;
        }

        TerminalCandidate candidate;
        std::ostringstream canonical;
        canonical << endpoint.id << ":" << best_component->id;
        candidate.id = "terminal-candidate-" + canonical.str();
        candidate.endpoint_id = endpoint.id;
        candidate.component_candidate_id = best_component->id;
        candidate.kind = kind_for_component(best_component->kind);
        candidate.position = endpoint.position;
        candidate.distance_to_component = best_distance;
        candidate.confidence =
            confidence_for_distance(best_distance, config_);

        if (candidate.confidence == ConfidenceClass::Unresolved)
            continue;

        result.candidates.push_back(std::move(candidate));
    }

    std::sort(
        result.candidates.begin(),
        result.candidates.end(),
        [](const TerminalCandidate& a, const TerminalCandidate& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

#include "eke_dx_wire/topology/terminal_location_detector.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace eke::dx::wire {
namespace {

double point_to_rect_boundary(
    const Point2D& point,
    const BoundingBox& box) {

    const double left = static_cast<double>(box.x);
    const double right = static_cast<double>(box.x + box.width);
    const double top = static_cast<double>(box.y);
    const double bottom = static_cast<double>(box.y + box.height);

    const bool inside =
        point.x >= left && point.x <= right &&
        point.y >= top && point.y <= bottom;

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
    const std::vector<EndpointCandidate>& endpoints) const {

    TerminalLocationArtifacts result;

    for (const auto& endpoint : endpoints) {
        double best_distance = std::numeric_limits<double>::max();
        const ComponentCandidate* best_component = nullptr;

        for (const auto& component : components) {
            const double distance =
                point_to_rect_boundary(endpoint.position, component.bounds);

            if (distance > config_.boundary_tolerance)
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

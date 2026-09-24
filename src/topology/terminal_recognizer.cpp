#include "eke_dx_wire/topology/terminal_recognizer.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace eke::dx::wire {
namespace {

double point_to_box(
    const Point2D& point,
    const BoundingBox& box) {

    const double left = static_cast<double>(box.x);
    const double right = static_cast<double>(box.x + box.width);
    const double top = static_cast<double>(box.y);
    const double bottom = static_cast<double>(box.y + box.height);

    const double dx =
        point.x < left ? left - point.x :
        point.x > right ? point.x - right : 0.0;
    const double dy =
        point.y < top ? top - point.y :
        point.y > bottom ? point.y - bottom : 0.0;

    return std::hypot(dx, dy);
}

Point2D closest_point_on_box(
    const Point2D& point,
    const BoundingBox& box) {

    const double left = static_cast<double>(box.x);
    const double right = static_cast<double>(box.x + box.width);
    const double top = static_cast<double>(box.y);
    const double bottom = static_cast<double>(box.y + box.height);

    return {
        std::clamp(point.x, left, right),
        std::clamp(point.y, top, bottom)
    };
}

double confidence_rank(ConfidenceClass value) {
    switch (value) {
    case ConfidenceClass::High: return 3.0;
    case ConfidenceClass::Medium: return 2.0;
    case ConfidenceClass::Low: return 1.0;
    case ConfidenceClass::Unresolved: return 0.0;
    }
    return 0.0;
}

ConfidenceClass combine_confidence(
    ConfidenceClass geometry,
    double distance,
    double max_distance) {

    ConfidenceClass distance_confidence = ConfidenceClass::Unresolved;
    if (distance <= 2.0) distance_confidence = ConfidenceClass::High;
    else if (distance <= 4.0) distance_confidence = ConfidenceClass::Medium;
    else if (distance <= max_distance) distance_confidence = ConfidenceClass::Low;

    return confidence_rank(geometry) < confidence_rank(distance_confidence)
        ? geometry
        : distance_confidence;
}

TerminalCandidateKind terminal_kind(ComponentCandidateKind kind) {
    switch (kind) {
    case ComponentCandidateKind::ChassisGround:
        return TerminalCandidateKind::GroundConnection;
    case ComponentCandidateKind::Enclosure:
    case ComponentCandidateKind::CircularSymbol:
        return TerminalCandidateKind::ComponentBoundary;
    case ComponentCandidateKind::PrimitiveSymbol:
        return TerminalCandidateKind::ConnectorBoundary;
    case ComponentCandidateKind::DiagramFurniture:
    case ComponentCandidateKind::Unknown:
        return TerminalCandidateKind::Unknown;
    }
    return TerminalCandidateKind::Unknown;
}

const TopologyNode* find_node(
    const std::vector<TopologyNode>& nodes,
    const std::string& id) {

    const auto it = std::find_if(
        nodes.begin(), nodes.end(),
        [&](const TopologyNode& node) { return node.id == id; });
    return it == nodes.end() ? nullptr : &*it;
}

double alignment_to_component(
    const EndpointCandidate& endpoint,
    const ComponentCandidate& component,
    const std::vector<TopologyNode>& nodes,
    const std::vector<TopologyEdge>& edges) {

    const Point2D boundary = closest_point_on_box(endpoint.position, component.bounds);
    const double vx = boundary.x - endpoint.position.x;
    const double vy = boundary.y - endpoint.position.y;
    const double target_length = std::hypot(vx, vy);
    if (target_length <= 1e-9)
        return 1.0;

    const double target_x = vx / target_length;
    const double target_y = vy / target_length;

    double best = -1.0;
    for (const auto& edge : edges) {
        if (std::find(
                endpoint.incident_edges.begin(),
                endpoint.incident_edges.end(),
                edge.id) == endpoint.incident_edges.end())
            continue;

        const TopologyNode* other = nullptr;
        if (edge.from_node == endpoint.node_id)
            other = find_node(nodes, edge.to_node);
        else if (edge.to_node == endpoint.node_id)
            other = find_node(nodes, edge.from_node);
        if (!other)
            continue;

        const double dx = other->position.x - endpoint.position.x;
        const double dy = other->position.y - endpoint.position.y;
        const double length = std::hypot(dx, dy);
        if (length <= 1e-9)
            continue;

        // The conductor must point toward the component boundary. A wire
        // approaching from the opposite direction is not terminal evidence.
        const double cosine =
            (dx * target_x + dy * target_y) / length;
        best = std::max(best, cosine);
    }

    return best;
}

} // namespace

TerminalRecognizer::TerminalRecognizer(TerminalRecognitionConfig config)
    : config_(config) {}

TerminalRecognitionArtifacts TerminalRecognizer::recognize(
    const std::vector<ComponentCandidate>& components,
    const std::vector<ComponentSymbolGeometry>& geometries,
    const std::vector<SymbolPrimitive>& primitives,
    const std::vector<EndpointCandidate>& endpoints,
    const std::vector<TopologyNode>& nodes,
    const std::vector<TopologyEdge>& edges,
    const std::vector<TerminalCandidate>& existing_candidates) const {

    TerminalRecognitionArtifacts result;

    (void)geometries;

    std::map<std::string, std::vector<const SymbolPrimitive*>> primitives_by_component;
    for (const auto& primitive : primitives)
        primitives_by_component[primitive.component_id].push_back(&primitive);

    std::set<std::string> existing_pairs;
    for (const auto& candidate : existing_candidates) {
        if (!candidate.endpoint_id.empty() && !candidate.component_candidate_id.empty())
            existing_pairs.insert(
                candidate.endpoint_id + ":" + candidate.component_candidate_id);
    }

    for (const auto& component : components) {
        if (component.kind == ComponentCandidateKind::DiagramFurniture ||
            component.id.empty())
            continue;

        const auto primitive_it = primitives_by_component.find(component.id);
        const std::vector<const SymbolPrimitive*> empty;
        const auto& owned_primitives =
            primitive_it == primitives_by_component.end() ? empty : primitive_it->second;

        // First consume AP-WIRE-023 TerminalLead evidence. A lead is only
        // evidence for associating an already-existing endpoint; it never
        // creates one.
        bool has_terminal_lead = false;
        for (const auto* primitive : owned_primitives) {
            if (primitive->kind == SymbolPrimitiveKind::TerminalLead)
                has_terminal_lead = true;
            if (primitive->kind != SymbolPrimitiveKind::TerminalLead)
                continue;

            const EndpointCandidate* best = nullptr;
            double best_distance = std::numeric_limits<double>::max();
            bool tie = false;

            for (const auto& endpoint : endpoints) {
                const double distance = point_to_box(endpoint.position, primitive->bounds);
                if (distance > config_.terminal_lead_max_distance)
                    continue;

                if (distance < best_distance - 1e-9) {
                    best = &endpoint;
                    best_distance = distance;
                    tie = false;
                } else if (std::abs(distance - best_distance) <= 1e-9) {
                    tie = true;
                }
            }

            if (!best || tie)
                continue;

            const std::string pair = best->id + ":" + component.id;
            if (existing_pairs.find(pair) != existing_pairs.end())
                continue;

            TerminalCandidate candidate;
            candidate.id = stable_id(
                "terminal-recognition",
                pair + ":" + primitive->id);
            candidate.endpoint_id = best->id;
            candidate.component_candidate_id = component.id;
            candidate.kind = terminal_kind(component.kind);
            candidate.position = best->position;
            candidate.distance_to_component = best_distance;
            candidate.confidence = combine_confidence(
                primitive->confidence,
                best_distance,
                config_.terminal_lead_max_distance);

            if (candidate.kind == TerminalCandidateKind::Unknown ||
                candidate.confidence == ConfidenceClass::Unresolved)
                continue;

            result.candidates.push_back(std::move(candidate));
            existing_pairs.insert(pair);
        }

        // For symbols with no usable internal lead, use the existing
        // component boundary plus conductor-direction evidence. This is
        // deliberately conservative: the endpoint must be within the
        // bounded extension distance and its incident conductor must point
        // toward the component. This addresses small source gaps without
        // inventing an endpoint or treating arbitrary nearby wires as pins.
        if (has_terminal_lead)
            continue;

        for (const auto& endpoint : endpoints) {
            const std::string pair = endpoint.id + ":" + component.id;
            if (existing_pairs.find(pair) != existing_pairs.end())
                continue;

            const double distance = point_to_box(endpoint.position, component.bounds);
            if (distance <= 0.0 || distance > config_.aligned_boundary_max_distance)
                continue;

            const double alignment =
                alignment_to_component(endpoint, component, nodes, edges);
            if (alignment < config_.minimum_alignment_cosine)
                continue;

            TerminalCandidate candidate;
            candidate.id = stable_id(
                "terminal-recognition",
                pair + ":boundary-alignment");
            candidate.endpoint_id = endpoint.id;
            candidate.component_candidate_id = component.id;
            candidate.kind = terminal_kind(component.kind);
            candidate.position = endpoint.position;
            candidate.distance_to_component = distance;
            candidate.confidence =
                distance <= 4.0 ? ConfidenceClass::Medium : ConfidenceClass::Low;

            if (candidate.kind == TerminalCandidateKind::Unknown)
                continue;

            result.candidates.push_back(std::move(candidate));
            existing_pairs.insert(pair);
        }
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

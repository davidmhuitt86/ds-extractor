#pragma once

#include "eke_dx_wire/core/geometry.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

enum class ConfidenceClass { High, Medium, Low, Unresolved };

enum class TopologyNodeType {
    ConductorEnd,
    Continuation,
    Junction,
    Crossing,
    ComponentBoundary,
    Unresolved
};

struct Provenance {
    std::string source_id;
    int page = 0;
    BoundingBox source_region {};
    std::string stage;
};

struct ConductorSegment {
    std::string id;
    Segment2D geometry {};
    double thickness_px = 0.0;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    Provenance provenance {};
    bool heavy_cable = false;
};

struct TopologyNode {
    std::string id;
    Point2D position {};
    TopologyNodeType type = TopologyNodeType::Unresolved;
};

struct TopologyEdge {
    std::string id;
    std::string from_node;
    std::string to_node;
    std::string conductor_segment;
};

struct Wire {
    std::string id;
    std::string start_endpoint;
    std::string end_endpoint;
    std::vector<std::string> topology_edges;
    std::vector<std::string> conductor_segments;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    bool heavy_cable = false;
};

struct WireModel {
    std::string source_id;
    int page = 0;
    int image_width = 0;
    int image_height = 0;

    // Observable conductor geometry. These are NOT wire identities.
    std::vector<ConductorSegment> conductor_segments;

    // Connectivity graph, independent of SVG rendering.
    std::vector<TopologyNode> nodes;
    std::vector<TopologyEdge> edges;

    // Endpoint-to-endpoint engineering objects derived from topology.
    std::vector<Wire> wires;
};

} // namespace eke::dx::wire

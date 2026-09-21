#pragma once

#include "eke_dx_wire/core/geometry.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace eke::dx::wire {

enum class ConfidenceClass {
    High,
    Medium,
    Low,
    Unresolved
};

enum class TopologyNodeType {
    Endpoint,
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

struct WireSegment {
    std::string id;
    Segment2D geometry {};
    double thickness_px = 0.0;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    Provenance provenance {};
    bool heavy_cable = false;
};

struct WirePath {
    std::string id;
    std::vector<Point2D> points;
    double thickness_px = 0.0;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
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
    std::string wire_path;
};

struct WireModel {
    std::string source_id;
    int page = 0;
    int image_width = 0;
    int image_height = 0;

    std::vector<WireSegment> segments;
    std::vector<WirePath> paths;
    std::vector<TopologyNode> nodes;
    std::vector<TopologyEdge> edges;
};

} // namespace eke::dx::wire

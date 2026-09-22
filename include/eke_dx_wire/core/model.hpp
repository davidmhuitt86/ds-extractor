#pragma once

#include "eke_dx_wire/core/geometry.hpp"
#include "eke_dx_wire/image/text_region_detector.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

enum class ConfidenceClass { High, Medium, Low, Unresolved };

enum class TopologyNodeType {
    ConductorEnd,
    Continuation,
    Junction,
    Splice,
    Crossing,
    ComponentBoundary,
    Unresolved
};

enum class EndpointKind {
    GeometricConductorEnd,
    ComponentTerminal,
    ConnectorTerminal,
    Splice,
    Ground,
    ExternalConnection,
    Unresolved
};

enum class TerminalRole {
    Unknown,
    ComponentTerminal,
    ConnectorTerminal,
    GroundTerminal,
    PowerSource,
    ExternalConnection
};

enum class DistributionRole {
    Unknown,
    Ground,
    PowerFeed,
    SharedFunctionFeed
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
    std::vector<Provenance> provenance_history;
    bool heavy_cable = false;
};

struct TopologyNode {
    std::string id;
    Point2D position {};
    TopologyNodeType type = TopologyNodeType::Unresolved;
    bool electrically_connective = true;
};

struct TopologyEdge {
    std::string id;
    std::string from_node;
    std::string to_node;
    std::string conductor_segment;
};

struct EndpointEvidence {
    BoundingBox source_region {};
    int local_ink_pixels = 0;
    int local_pixel_count = 0;
    double local_ink_density = 0.0;
    int forward_ink_pixels = 0;
    int forward_pixel_count = 0;
    double forward_ink_density = 0.0;
    int transverse_ink_pixels = 0;
    int transverse_pixel_count = 0;
    double transverse_ink_density = 0.0;
    bool near_image_boundary = false;
};

struct EndpointCandidate {
    std::string id;
    std::string node_id;
    Point2D position {};
    EndpointKind kind = EndpointKind::Unresolved;
    TerminalRole terminal_role = TerminalRole::Unknown;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    std::vector<std::string> incident_edges;
    EndpointEvidence evidence {};

    // Semantic attachment fields are populated only when independent
    // component/terminal evidence exists. Pixel geometry alone must not
    // invent component identity or terminal function.
    std::string component_id;
    std::string terminal_name;
    std::string function_label;
    std::string wire_color;
};


enum class ComponentCandidateKind {
    Enclosure,
    CircularSymbol,
    ChassisGround,
    PrimitiveSymbol,
    Unknown
};

struct ComponentCandidate {
    std::string id;
    ComponentCandidateKind kind = ComponentCandidateKind::Unknown;
    std::vector<std::string> shape_ids;
    BoundingBox bounds {};
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
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

struct ElectricalNet {
    std::string id;
    std::vector<std::string> endpoint_ids;
    std::vector<std::string> splice_node_ids;
    std::vector<std::string> topology_edges;
    DistributionRole role = DistributionRole::Unknown;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    std::string anchor_endpoint;
};

struct WireModel {
    std::string source_id;
    int page = 0;
    int image_width = 0;
    int image_height = 0;

    std::vector<ComponentCandidate> component_candidates;
    std::vector<TextRegion> text_regions;
    std::vector<ConductorSegment> conductor_segments;
    std::vector<TopologyNode> nodes;
    std::vector<TopologyEdge> edges;
    std::vector<EndpointCandidate> endpoint_candidates;
    std::vector<ElectricalNet> electrical_nets;
    std::vector<Wire> wires;
};

} // namespace eke::dx::wire

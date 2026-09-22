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

enum class RejectedGeometryClass {
    ComponentAssociated,
    ConnectorAssociated,
    TextAssociated,
    Unresolved
};

struct RejectedGeometryEvidence {
    std::string id;
    Segment2D geometry {};
    std::string reason;
    double measurement = 0.0;
    Provenance provenance {};
    RejectedGeometryClass classification = RejectedGeometryClass::Unresolved;
    std::string associated_object_id;
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


enum class TerminalCandidateKind {
    ComponentBoundary,
    ConnectorBoundary,
    GroundConnection,
    Unknown
};

struct TerminalCandidate {
    std::string id;
    std::string endpoint_id;
    std::string component_candidate_id;
    TerminalCandidateKind kind = TerminalCandidateKind::Unknown;
    Point2D position {};
    double distance_to_component = 0.0;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
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

enum class WireValidationSeverity {
    Warning,
    Error
};

struct WireValidationIssue {
    WireValidationSeverity severity = WireValidationSeverity::Error;
    std::string code;
    std::string object_id;
    std::string detail;
};

struct WireValidationReport {
    bool valid = true;
    std::size_t wires_checked = 0;
    std::size_t valid_wires = 0;
    std::size_t electrical_nets_checked = 0;
    std::vector<WireValidationIssue> issues;
};

struct ExtractionAudit {
    // Geometry
    std::size_t conductor_segments = 0;

    // Topology
    std::size_t topology_nodes = 0;
    std::size_t topology_edges = 0;
    std::size_t conductor_end_nodes = 0;
    std::size_t continuation_nodes = 0;
    std::size_t junction_nodes = 0;
    std::size_t splice_nodes = 0;
    std::size_t crossing_nodes = 0;
    std::size_t component_boundary_nodes = 0;
    std::size_t unresolved_nodes = 0;

    // Endpoint candidates
    std::size_t endpoint_candidates = 0;
    std::size_t geometric_endpoints = 0;
    std::size_t component_terminals = 0;
    std::size_t connector_terminals = 0;
    std::size_t ground_endpoints = 0;
    std::size_t external_connections = 0;
    std::size_t splice_endpoints = 0;
    std::size_t unresolved_endpoints = 0;

    // Components / shapes
    std::size_t shapes = 0;
    std::size_t enclosure_shapes = 0;
    std::size_t circular_shapes = 0;
    std::size_t chassis_ground_shapes = 0;
    std::size_t primitive_shapes = 0;
    std::size_t unknown_shapes = 0;

    // Wires
    std::size_t wires = 0;
    std::size_t heavy_cable_wires = 0;
    std::size_t unresolved_wires = 0;

    // Electrical nets
    std::size_t electrical_nets = 0;
    std::size_t ground_nets = 0;
    std::size_t power_feed_nets = 0;
    std::size_t shared_function_feed_nets = 0;
    std::size_t unresolved_nets = 0;

    // Validation
    std::size_t validation_errors = 0;
    std::size_t validation_warnings = 0;
    std::size_t valid_wires = 0;

    // Pipeline evidence
    std::size_t gaps_bridged = 0;
};

enum class SemanticAssociationTargetKind {
    Component,
    Endpoint
};

enum class SemanticAssociationRelation {
    LabelToComponent,
    LabelToEndpoint
};

struct SemanticAssociation {
    std::string id;
    std::string text_region_id;
    std::string target_id;
    SemanticAssociationTargetKind target_kind =
        SemanticAssociationTargetKind::Endpoint;
    SemanticAssociationRelation relation =
        SemanticAssociationRelation::LabelToEndpoint;
    double distance = 0.0;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
};

struct WireModel {
    std::string source_id;
    int page = 0;
    int image_width = 0;
    int image_height = 0;

    std::vector<ComponentCandidate> component_candidates;
    std::vector<TextRegion> text_regions;
    std::vector<SemanticAssociation> semantic_associations;
    std::vector<TerminalCandidate> terminal_candidates;
    std::vector<ConductorSegment> conductor_segments;
    std::vector<RejectedGeometryEvidence> rejected_geometry;
    std::vector<TopologyNode> nodes;
    std::vector<TopologyEdge> edges;
    std::vector<EndpointCandidate> endpoint_candidates;
    std::vector<ElectricalNet> electrical_nets;
    std::vector<Wire> wires;
    WireValidationReport wire_validation;
    ExtractionAudit audit;
};

} // namespace eke::dx::wire

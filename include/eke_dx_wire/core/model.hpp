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

struct ConnectorCandidate {
    std::string id;
    std::string component_candidate_id;
    BoundingBox bounds {};
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    std::vector<std::string> semantic_labels;
};

enum class ConnectorTerminalStatus {
    Resolved,
    Unresolved,
    Conflicted
};

struct ConnectorTerminal {
    std::string id;
    std::string connector_id;
    std::string endpoint_id;
    Point2D position {};
    std::string terminal_name;
    std::string function_label;
    std::string wire_color;
    TerminalRole role = TerminalRole::ConnectorTerminal;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    ConnectorTerminalStatus status = ConnectorTerminalStatus::Unresolved;
};

enum class ComponentCandidateKind {
    Enclosure,
    CircularSymbol,
    ChassisGround,
    PrimitiveSymbol,
    Unknown
};

enum class ComponentSymbolKind {
    Enclosure,
    CircularSymbol,
    ChassisGround,
    PrimitiveSymbol,
    Unknown
};

// `Recognized` is reserved for a symbol whose internal visual geometry was
// actually classified against a known electrical-symbol family (switch,
// relay, diode, motor, etc.). No current stage produces it.
//
// `GeometricallyClassified` is what ComponentSymbolRecognizer currently
// produces: the component's ComponentCandidateKind (a coarse geometric
// bucket - enclosure/circular/chassis-ground/primitive) was carried across
// the model boundary unchanged. It is not evidence that the specific
// symbol was identified, only that it was not Unknown-shaped.
enum class ComponentSymbolRecognitionStatus {
    Recognized,
    GeometricallyClassified,
    Unresolved,
    Conflicted
};

struct ComponentSymbolRecognition {
    std::string id;
    std::string component_id;
    ComponentSymbolKind symbol_kind = ComponentSymbolKind::Unknown;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    ComponentSymbolRecognitionStatus status =
        ComponentSymbolRecognitionStatus::Unresolved;
    std::vector<std::string> shape_ids;
};

struct ComponentCandidate {
    std::string id;
    ComponentCandidateKind kind = ComponentCandidateKind::Unknown;
    std::vector<std::string> shape_ids;
    BoundingBox bounds {};
    ConfidenceClass confidence = ConfidenceClass::Unresolved;

    // Resolved human-readable labels are semantic enrichment. They do not
    // establish geometry, topology, or component identity by themselves.
    std::vector<std::string> semantic_labels;
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

struct WireValidationIssueSummary {
    std::string code;
    std::size_t count = 0;
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
    std::size_t component_symbol_recognitions = 0;
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
    std::vector<WireValidationIssueSummary> validation_warning_summaries;

    // Pipeline evidence
    std::size_t gaps_bridged = 0;
};

enum class TextSemanticKind {
    GroundLabel,
    PowerFeedLabel,
    SharedFunctionFeedLabel,
    ComponentLabel,
    ConnectorLabel,
    TerminalLabel,
    WireColorLabel,
    FunctionLabel,
    Unknown
};

struct TextRecognitionEvidence {
    std::string text_region_id;
    std::string raw_text;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;

    // Recognition provenance is preserved at the evidence boundary so
    // downstream semantic interpretation never has to infer its source.
    std::string provider;
};

struct TextSemanticEvidence {
    std::string id;
    std::string text_region_id;
    std::string raw_text;
    std::string normalized_text;
    TextSemanticKind kind = TextSemanticKind::Unknown;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    std::string source;
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

struct EngineeringObjectSemanticResolution {
    std::string id;
    std::string text_region_id;
    std::string target_id;
    SemanticAssociationTargetKind target_kind =
        SemanticAssociationTargetKind::Endpoint;
    TextSemanticKind semantic_kind = TextSemanticKind::Unknown;
    std::string raw_text;
    std::string normalized_text;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    double distance = 0.0;
    std::string source;
};


enum class ComponentIdentityEvidenceKind {
    ComponentLabel,
    ConnectorLabel
};

enum class ComponentIdentityResolutionStatus {
    Resolved,
    Conflicted,
    Unresolved
};

struct ComponentIdentityResolution {
    std::string id;
    std::string component_id;
    std::string identity;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    ComponentIdentityResolutionStatus status =
        ComponentIdentityResolutionStatus::Unresolved;
    std::vector<std::string> evidence_ids;
};

struct ComponentIdentityEvidence {
    std::string id;
    std::string component_id;
    ComponentIdentityEvidenceKind kind = ComponentIdentityEvidenceKind::ComponentLabel;
    std::string raw_text;
    std::string normalized_text;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    double distance = 0.0;
    std::string source;
};

enum class ComponentIdentityCanonicalizationStatus {
    Resolved,
    NotFound,
    Conflicted
};

struct ComponentIdentityCanonicalization {
    std::string id;
    std::string component_id;
    std::string source_resolution_id;
    std::string source_identity;
    std::string canonical_id;
    std::string canonical_name;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    ComponentIdentityCanonicalizationStatus status =
        ComponentIdentityCanonicalizationStatus::NotFound;
};

enum class EndpointSemanticReconstructionStatus {
    Resolved,
    Conflicted,
    Unresolved
};

struct EndpointSemanticReconstruction {
    std::string id;
    std::string endpoint_id;
    std::string component_id;
    EndpointKind endpoint_kind = EndpointKind::Unresolved;
    TerminalRole terminal_role = TerminalRole::Unknown;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    EndpointSemanticReconstructionStatus status =
        EndpointSemanticReconstructionStatus::Unresolved;
    std::vector<std::string> evidence_component_ids;
};

struct WireModel {
    std::string source_id;
    int page = 0;
    int image_width = 0;
    int image_height = 0;

    std::vector<ComponentCandidate> component_candidates;
    std::vector<ComponentSymbolRecognition> component_symbol_recognitions;
    std::vector<ConnectorCandidate> connector_candidates;
    std::vector<ConnectorTerminal> connector_terminals;
    std::vector<TextRegion> text_regions;
    std::vector<TextRecognitionEvidence> text_recognition_evidence;
    std::vector<SemanticAssociation> semantic_associations;
    std::vector<TextSemanticEvidence> text_semantic_evidence;
    std::vector<EngineeringObjectSemanticResolution> engineering_object_semantics;
    std::vector<ComponentIdentityEvidence> component_identity_evidence;
    std::vector<ComponentIdentityResolution> component_identity_resolutions;
    std::vector<ComponentIdentityCanonicalization> component_identity_canonicalizations;
    std::vector<TerminalCandidate> terminal_candidates;
    std::vector<EndpointSemanticReconstruction> endpoint_semantic_reconstructions;
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

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
    // Non-circuit diagram content (legend/color-key tables,
    // switch-continuity charts, title blocks) drawn with the same small
    // circle/rectangle primitives as real symbols. Re-tagged from an
    // initial CircularSymbol/PrimitiveSymbol classification by
    // DiagramFurnitureClassifier based on grid arrangement, not asserted
    // at shape-detection time.
    DiagramFurniture,
    Unknown
};

enum class ComponentSymbolKind {
    Enclosure,
    CircularSymbol,
    ChassisGround,
    PrimitiveSymbol,
    DiagramFurniture,
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

// AP-WIRE-023: geometry observed INSIDE an already-detected real
// ComponentCandidate's bounding region, excluding the candidate's own
// outer boundary stroke (which ShapeDetector/ComponentCandidateClassifier
// already model as the candidate's shape). This is geometric evidence
// only - it must never be treated as engineering terminal identity or
// symbol-family identity. That interpretation belongs to a later stage.
enum class SymbolPrimitiveKind {
    Line,
    Circle,
    Rectangle,
    // A line-like blob that touches the component's own boundary margin,
    // i.e. it appears to reach toward/through the symbol's outline rather
    // than remain fully internal. This is geometric shape only - it is
    // not an EndpointCandidate and must not be treated as one.
    TerminalLead,
    Unknown
};

struct SymbolPrimitive {
    std::string id;
    std::string component_id;
    SymbolPrimitiveKind kind = SymbolPrimitiveKind::Unknown;
    BoundingBox bounds {};
    double area = 0.0;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    Provenance provenance {};
};

struct ComponentSymbolGeometry {
    std::string id;
    std::string component_id;
    std::vector<std::string> primitive_ids;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
};

// AP-WIRE-031: physical Wire identity status, independent of the
// pre-existing geometric `confidence` field above. Resolved only when
// explicit evidence establishes the endpoint-to-endpoint physical
// conductor identity; Conflicted when independent evidence establishes
// incompatible physical identity interpretations; Unresolved when
// evidence is simply insufficient. Never a synonym for "not enough
// information" collapsed into Conflicted - see AP-WIRE-031 Sec 18.
enum class WireIdentityStatus {
    Resolved,
    Unresolved,
    Conflicted
};

struct Wire {
    std::string id;
    std::string start_endpoint;
    std::string end_endpoint;
    std::vector<std::string> topology_edges;
    std::vector<std::string> conductor_segments;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    bool heavy_cable = false;

    // AP-WIRE-031: physical Wire identity resolution. Defaults to
    // Unresolved for any Wire constructed without going through
    // PhysicalWireIdentityReconstructor's explicit assignment.
    WireIdentityStatus identity_status = WireIdentityStatus::Unresolved;
    // Ids of existing evidence objects actually used to establish this
    // Wire's physical identity (ConductorBoundaryResolution ids for its
    // two endpoints, and/or ConductorSegment ids whose sharing across a
    // distribution node justified crossing it). Never populated with an
    // invented or arbitrary id - see AP-WIRE-031 Sec 17.
    std::vector<std::string> identity_evidence_ids;
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

// AP-WIRE-025: semantic completion layer over an already-reconstructed
// Wire. This is a read-only projection of existing evidence - it never
// mutates Wire/topology/endpoint/electrical-net identity. Resolved only
// when explicit evidence supports it; Conflicted when independent
// evidence disagrees; Unresolved when evidence is absent. A field is
// never populated by guessing from geometry, proximity, or page layout.
enum class WireSemanticStatus {
    Resolved,
    Unresolved,
    Conflicted
};

struct WireSemanticResolution {
    std::string id;
    std::string wire_id;

    // Explicit WireColorLabel evidence from the wire's own two endpoints
    // (EndpointCandidate::wire_color). Resolved+High when both endpoints
    // agree, Resolved+Medium when only one endpoint carries evidence,
    // Conflicted when they disagree, Unresolved when neither has evidence.
    std::string wire_color;
    WireSemanticStatus wire_color_status = WireSemanticStatus::Unresolved;
    ConfidenceClass wire_color_confidence = ConfidenceClass::Unresolved;

    // Explicit FunctionLabel evidence, same reinforcement/conflict rule as
    // wire_color. Never derived from ElectricalNet::role - net role and
    // wire function are kept as distinct evidence domains.
    std::string function_label;
    WireSemanticStatus function_status = WireSemanticStatus::Unresolved;
    ConfidenceClass function_confidence = ConfidenceClass::Unresolved;

    // Component/terminal association at each fixed wire endpoint, read
    // directly from EndpointSemanticReconstruction (AP-WIRE-019). An
    // endpoint whose reconstruction is Conflicted (including the
    // AP-WIRE-024 boundary/alignment fallback case) is reported
    // Conflicted here too, never silently treated as Resolved.
    std::string start_component_id;
    std::string start_terminal_name;
    WireSemanticStatus start_component_status = WireSemanticStatus::Unresolved;

    std::string end_component_id;
    std::string end_terminal_name;
    WireSemanticStatus end_component_status = WireSemanticStatus::Unresolved;

    // Connector-terminal association at each endpoint, read from
    // ConnectorTerminal (AP-WIRE-020). Only a Resolved ConnectorTerminal
    // status is used; Unresolved/Conflicted connector terminals are not
    // treated as authoritative.
    std::string start_connector_id;
    std::string start_connector_terminal_name;
    WireSemanticStatus start_connector_status = WireSemanticStatus::Unresolved;

    std::string end_connector_id;
    std::string end_connector_terminal_name;
    WireSemanticStatus end_connector_status = WireSemanticStatus::Unresolved;

    // Electrical-net association: which ElectricalNet (if any) the wire's
    // endpoints belong to. Resolved+High when both endpoints agree on one
    // net, Resolved+Medium when only one endpoint is net-resolved,
    // Conflicted when the two endpoints resolve to different nets
    // (a genuine cross-stage inconsistency worth surfacing, not hiding),
    // Unresolved when neither endpoint belongs to any net.
    std::string electrical_net_id;
    WireSemanticStatus electrical_net_status = WireSemanticStatus::Unresolved;
    ConfidenceClass electrical_net_confidence = ConfidenceClass::Unresolved;
};

struct WireSemanticCoverage {
    std::size_t total = 0;

    std::size_t wire_color_resolved = 0;
    std::size_t wire_color_conflicted = 0;
    std::size_t wire_color_unresolved = 0;

    std::size_t function_resolved = 0;
    std::size_t function_conflicted = 0;
    std::size_t function_unresolved = 0;

    std::size_t component_association_resolved = 0;
    std::size_t component_association_conflicted = 0;
    std::size_t component_association_unresolved = 0;

    std::size_t connector_association_resolved = 0;

    std::size_t electrical_net_resolved = 0;
    std::size_t electrical_net_conflicted = 0;
    std::size_t electrical_net_unresolved = 0;

    // A wire with zero Resolved fields across every category above.
    std::size_t fully_unresolved = 0;
};

// AP-WIRE-026A: engineering symbol-family recognition. This is an
// explicit semantic interpretation of already-established AP-WIRE-023
// symbol geometry (plus, where independently available, component
// identity/label evidence and future recognition-provider observations).
// It is NOT component identity, NOT canonical identity, NOT symbol
// geometry, and NOT terminal recognition - those remain owned by their
// existing APs and are only referenced here.
//
// The taxonomy below is deliberately small. Only families with a
// currently-defensible recognition rule are included. See
// docs/AP-WIRE-026A_Symbol_Family_Recognition.md for the exact evidence
// rule behind each one. Unknown is always preferred over a speculative
// family with no supporting rule.
enum class SymbolFamily {
    Ground,
    Lamp,
    Switch,
    Relay,
    Motor,
    Diode,
    Alternator,
    Battery,
    Solenoid,
    Coil,
    Unknown
};

enum class SymbolFamilyEvidenceKind {
    // The component's own ComponentCandidateKind came from a purpose-
    // built geometric detector for a specific engineering symbol (today:
    // only ShapeDetector's chassis-ground bar pattern), not a generic
    // shape bucket. This is categorically stronger than resemblance to a
    // generic circle/rectangle primitive.
    PurposeBuiltGeometricClassification,
    // A keyword match against already-resolved component identity text
    // (ComponentCandidate.semantic_labels /
    // ComponentIdentityCanonicalization.canonical_name) combined with a
    // geometrically compatible ComponentCandidateKind. Label alone is
    // never sufficient - see the recognizer's evidence rules.
    LabelKeywordWithCompatibleGeometry,
    // An external SymbolRecognitionProvider observation (e.g. a future
    // vision-based recognizer). Never used as the sole basis for
    // Resolved status without at least one other independent evidence
    // source agreeing, so an unsupported single provider guess cannot
    // resolve a family by itself.
    ProviderObservation
};

struct SymbolFamilyEvidence {
    std::string id;
    std::string component_id;
    SymbolFamily family = SymbolFamily::Unknown;
    SymbolFamilyEvidenceKind kind =
        SymbolFamilyEvidenceKind::LabelKeywordWithCompatibleGeometry;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    std::string source;
    std::string detail;
};

enum class SymbolFamilyResolutionStatus {
    Resolved,
    Unresolved,
    Conflicted
};

struct SymbolFamilyResolution {
    std::string id;
    std::string component_id;
    SymbolFamily family = SymbolFamily::Unknown;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    SymbolFamilyResolutionStatus status =
        SymbolFamilyResolutionStatus::Unresolved;
    std::vector<std::string> evidence_ids;
    // Reference only - AP-WIRE-023's geometry is never duplicated here.
    std::string source_symbol_geometry_id;
};

struct SymbolFamilyCoverage {
    std::size_t total = 0;
    std::size_t resolved = 0;
    std::size_t unresolved = 0;
    std::size_t conflicted = 0;
    // Per-family resolved counts, in taxonomy declaration order.
    std::size_t ground_resolved = 0;
    std::size_t lamp_resolved = 0;
    std::size_t switch_resolved = 0;
    std::size_t relay_resolved = 0;
    std::size_t motor_resolved = 0;
    std::size_t diode_resolved = 0;
    std::size_t alternator_resolved = 0;
    std::size_t battery_resolved = 0;
    std::size_t solenoid_resolved = 0;
    std::size_t coil_resolved = 0;
};

// AP-WIRE-030: conductor-boundary / terminal-resolution evidence and
// resolution. This is a read-only semantic layer over already-produced
// evidence (TerminalCandidate from AP-WIRE-024, EndpointSemanticReconstruction
// from AP-WIRE-019, ConnectorTerminal from AP-WIRE-020, and EndpointCandidate
// itself). It never mutates topology, creates/deletes edges, creates
// components, fabricates a terminal identifier, or reconstructs a Wire.
// Component association and terminal identity are independently tracked
// statuses for the same endpoint - see docs/AP-WIRE-030_Conductor_Boundary_
// and_Terminal_Resolution.md Sec 15. No new boundary-kind enum is
// introduced: boundary_kind below reuses the existing EndpointKind values.
enum class ConductorBoundaryEvidenceKind {
    // A TerminalCandidate (AP-WIRE-024) associating this endpoint with a
    // component/connector/ground boundary via geometry.
    TerminalCandidateEvidence,
    // The already-resolved EndpointSemanticReconstruction (AP-WIRE-019)
    // for this endpoint - consulted, never re-derived from scratch.
    EndpointSemanticReconstructionEvidence,
    // A ConnectorTerminal (AP-WIRE-020) referencing this endpoint - its
    // own ConnectorTerminalStatus is adopted directly, never re-decided.
    ConnectorTerminalEvidence,
    // EndpointCandidate.kind already carrying Ground/ExternalConnection
    // (itself ultimately derived from one of the evidence kinds above,
    // surfaced here only for traceability of the final endpoint state).
    GroundEndpointEvidence,
    ExternalConnectionEvidence
};

struct ConductorBoundaryEvidence {
    std::string id;
    std::string endpoint_id;
    ConductorBoundaryEvidenceKind kind =
        ConductorBoundaryEvidenceKind::TerminalCandidateEvidence;
    // TerminalCandidate.id / EndpointSemanticReconstruction.id /
    // ConnectorTerminal.id / EndpointCandidate.id, depending on kind.
    std::string source_object_id;
    EndpointKind suggested_boundary = EndpointKind::Unresolved;
    std::string component_id;
    std::string connector_id;
    // Only ever populated when the evidence itself independently carries a
    // specific terminal/pin identifier (e.g. a resolved ConnectorTerminal's
    // terminal_name). TerminalCandidate carries no such identifier and
    // never populates this field - never invented.
    std::string terminal_identifier;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
};

enum class ConductorBoundaryStatus {
    Resolved,
    Unresolved,
    Conflicted
};

struct ConductorBoundaryResolution {
    std::string id;
    std::string endpoint_id;

    // Overall boundary classification (AP-WIRE-030 Sec 9's six-category
    // taxonomy, expressed via the existing EndpointKind values only -
    // Splice/Junction/Crossing/Continuation are never valid values here,
    // per AP-WIRE-029's critical invariant).
    EndpointKind boundary_kind = EndpointKind::GeometricConductorEnd;
    ConductorBoundaryStatus boundary_status =
        ConductorBoundaryStatus::Unresolved;
    ConfidenceClass boundary_confidence = ConfidenceClass::Unresolved;

    // Component association - independent of terminal identity (Sec 15).
    std::string component_id;
    ConductorBoundaryStatus component_status =
        ConductorBoundaryStatus::Unresolved;

    // Terminal identity - independent of component association (Sec 10,
    // 15). Never populated by proximity, shape, symbol family, wire
    // color, or generic text - only by evidence that independently
    // establishes a specific terminal (Sec 9, 17).
    std::string terminal_identifier;
    ConductorBoundaryStatus terminal_status =
        ConductorBoundaryStatus::Unresolved;

    // Connector association - independent of connector-terminal/pin
    // identity (Sec 11). Always Unresolved on a baseline with 0
    // connectors; never fabricated to look otherwise.
    std::string connector_id;
    ConductorBoundaryStatus connector_status =
        ConductorBoundaryStatus::Unresolved;
    std::string connector_terminal_identifier;
    ConductorBoundaryStatus connector_terminal_status =
        ConductorBoundaryStatus::Unresolved;

    // Ground / external boundary status (Sec 12, 13). Independent of
    // component/connector/terminal status above.
    ConductorBoundaryStatus ground_status =
        ConductorBoundaryStatus::Unresolved;
    ConductorBoundaryStatus external_status =
        ConductorBoundaryStatus::Unresolved;

    // Provenance (Sec 21): the ConductorBoundaryEvidence ids that support
    // this resolution.
    std::vector<std::string> evidence_ids;
    // Competing values preserved for a Conflicted status - never
    // collapsed to a winner (Sec 16, 22).
    std::vector<std::string> conflicting_component_ids;
    std::vector<std::string> conflicting_terminal_identifiers;
};

struct ConductorBoundaryCoverage {
    std::size_t total = 0;

    std::size_t boundary_resolved = 0;
    std::size_t boundary_unresolved = 0;
    std::size_t boundary_conflicted = 0;

    std::size_t component_resolved = 0;
    std::size_t component_unresolved = 0;
    std::size_t component_conflicted = 0;

    std::size_t terminal_resolved = 0;
    std::size_t terminal_unresolved = 0;
    std::size_t terminal_conflicted = 0;

    std::size_t connector_resolved = 0;
    std::size_t connector_unresolved = 0;
    std::size_t connector_conflicted = 0;

    std::size_t connector_terminal_resolved = 0;
    std::size_t connector_terminal_unresolved = 0;
    std::size_t connector_terminal_conflicted = 0;

    std::size_t ground_resolved = 0;
    std::size_t external_resolved = 0;
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
    std::size_t diagram_furniture_shapes = 0;
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

    // AP-WIRE-023: internal symbol geometry
    std::size_t components_with_symbol_geometry = 0;
    std::size_t components_without_symbol_geometry = 0;
    std::size_t symbol_primitives = 0;
    std::size_t symbol_primitive_lines = 0;
    std::size_t symbol_primitive_circles = 0;
    std::size_t symbol_primitive_rectangles = 0;
    std::size_t symbol_primitive_terminal_leads = 0;
    std::size_t symbol_primitive_unknown = 0;

    // AP-WIRE-025: wire semantic resolution
    WireSemanticCoverage wire_semantics {};

    // AP-WIRE-026A: symbol-family recognition
    SymbolFamilyCoverage symbol_families {};

    // AP-WIRE-030: conductor-boundary / terminal resolution
    ConductorBoundaryCoverage conductor_boundaries {};
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
    std::vector<ComponentSymbolGeometry> component_symbol_geometries;
    std::vector<SymbolPrimitive> symbol_primitives;
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
    std::vector<WireSemanticResolution> wire_semantics;
    std::vector<SymbolFamilyEvidence> symbol_family_evidence;
    std::vector<SymbolFamilyResolution> symbol_family_resolutions;
    std::vector<ConductorBoundaryEvidence> conductor_boundary_evidence;
    std::vector<ConductorBoundaryResolution> conductor_boundary_resolutions;
    WireValidationReport wire_validation;
    ExtractionAudit audit;
};

} // namespace eke::dx::wire

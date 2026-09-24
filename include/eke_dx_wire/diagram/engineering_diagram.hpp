#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

// AP-WIRE-026: the unified engineering-model boundary between extraction
// (AP-WIRE-022 through AP-WIRE-025) and future rendering/export stages
// (AP-WIRE-027+). EngineeringDiagram is a READ-ONLY, REFERENCE-PRESERVING
// composition over an existing WireModel: every object below carries the
// same deterministic IDs already established by earlier stages. Nothing
// here re-derives geometry, topology, identity, or semantics - it only
// joins already-resolved evidence into renderer-convenient views and
// validates that the cross-references between them are consistent.
//
// Coordinate system contract: every position/bounds value in this
// structure is in the same coordinate system as the source WireModel -
// normalized source/page pixel coordinates (origin top-left, no axis
// inversion, no scaling, no translation). A renderer that needs a
// different coordinate system (e.g. an auto-laid-out schematic canvas,
// as shown in a Diagram Studio reference rendering) performs that
// transform itself; EngineeringDiagram does not attempt page-layout or
// auto-routing. See docs/AP-WIRE-026_Unified_Engineering_Diagram.md.
inline constexpr const char* kEngineeringDiagramCoordinateSystem =
    "source_page_pixels";

enum class DiagramObjectStatus {
    Resolved,
    Unresolved,
    Conflicted
};

struct DiagramComponent {
    std::string component_id;
    ComponentCandidateKind kind = ComponentCandidateKind::Unknown;
    BoundingBox bounds {};
    ConfidenceClass geometry_confidence = ConfidenceClass::Unresolved;

    // Semantic enrichment already established by AP-WIRE-015/016/017/018.
    // A component label is not canonical identity; canonical_name is only
    // populated when ComponentIdentityCanonicalizationStatus::Resolved.
    std::vector<std::string> semantic_labels;
    std::string canonical_name;
    DiagramObjectStatus identity_status = DiagramObjectStatus::Unresolved;
    std::vector<std::string> identity_evidence_ids;

    // AP-WIRE-023: symbol geometry is evidence about what is drawn, not
    // identity. Empty when the component (e.g. DiagramFurniture, or a
    // real component with no internal geometry) has none.
    std::string symbol_geometry_id;

    // AP-WIRE-026A: reference only - see SymbolFamilyResolution for the
    // resolved family, status, confidence, and evidence. Never inlined
    // here, mirroring how wire semantics are referenced from DiagramWire.
    std::string symbol_family_resolution_id;

    // AP-WIRE-024: TerminalCandidate ids already associated with this
    // component (regardless of resolution status downstream).
    std::vector<std::string> terminal_candidate_ids;

    // Endpoints whose EndpointSemanticReconstruction resolved to this
    // component (never populated from a Conflicted/Unresolved
    // reconstruction - identity is never fabricated here).
    std::vector<std::string> endpoint_ids;
};

struct DiagramConnector {
    std::string connector_id;
    std::string component_id;
    BoundingBox bounds {};
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    std::vector<std::string> semantic_labels;
    std::vector<std::string> connector_terminal_ids;
};

struct DiagramWire {
    std::string wire_id;
    std::string start_endpoint_id;
    std::string end_endpoint_id;
    std::vector<std::string> topology_edge_ids;
    std::vector<std::string> conductor_segment_ids;
    ConfidenceClass geometry_confidence = ConfidenceClass::Unresolved;
    bool heavy_cable = false;

    // AP-WIRE-025 reference. Never inlined/duplicated - a renderer that
    // needs the resolved color/function/associations reads the
    // WireSemanticResolution this id points to.
    std::string wire_semantic_resolution_id;
};

// A splice/junction node per the standing wire-identity rule: it is NOT a
// wire endpoint. incident_wire_ids lists every Wire whose topology_edges
// touch this node, so a renderer can identify a shared-conductor branch
// point without re-deriving it from raw topology edges itself.
struct DiagramSplice {
    std::string node_id;
    Point2D position {};
    TopologyNodeType type = TopologyNodeType::Unresolved;
    std::vector<std::string> incident_wire_ids;
};

// A DiagramLabel represents every detected TextRegion, resolved or not.
// "Unknown text must remain representable" (AP spec) - a region with no
// recognition evidence still appears here with an empty raw_text and
// Unresolved status, rather than being silently dropped. An "annotation"
// in the reference-image sense is a DiagramLabel whose classification
// never resolved past TextSemanticKind::Unknown; no separate annotation
// model was introduced since no distinct evidence source justifies one.
struct DiagramLabel {
    std::string text_region_id;
    std::string raw_text;
    std::string normalized_text;
    TextSemanticKind kind = TextSemanticKind::Unknown;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    DiagramObjectStatus status = DiagramObjectStatus::Unresolved;
    std::string provider;
    std::string associated_object_id;
    SemanticAssociationTargetKind associated_object_kind =
        SemanticAssociationTargetKind::Endpoint;
};

struct DiagramElectricalNet {
    std::string net_id;
    std::vector<std::string> endpoint_ids;
    std::vector<std::string> splice_node_ids;
    std::vector<std::string> topology_edge_ids;

    // Derived, read-only: wires whose start or end endpoint belongs to
    // this net. This is the explicit physical-layer/electrical-layer
    // distinction the AP spec requires - a net is not a wire and a wire
    // is not a net, but a renderer needs to be able to cross-reference
    // them without re-deriving the relationship.
    std::vector<std::string> wire_ids;

    DistributionRole role = DistributionRole::Unknown;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
};

struct DiagramRelationshipIssue {
    std::string category;
    std::string code;
    std::string object_id;
    std::string detail;
};

struct DiagramValidationReport {
    std::size_t valid_references = 0;
    std::size_t invalid_references = 0;
    std::size_t duplicate_relationships = 0;
    std::size_t orphan_objects = 0;
    std::vector<DiagramRelationshipIssue> issues;
};

struct EngineeringDiagram {
    std::string source_id;
    int page = 0;
    int image_width = 0;
    int image_height = 0;

    std::vector<DiagramComponent> components;
    std::vector<DiagramConnector> connectors;
    std::vector<DiagramWire> wires;
    std::vector<DiagramSplice> splices;

    // TopologyNode ids of type Crossing: a visual line-crossing with no
    // electrical significance. Exposed for renderer awareness only (a
    // renderer may want to draw a hop/jog at a crossing) - it carries no
    // additional engineering semantics beyond the node's own position.
    std::vector<std::string> crossing_node_ids;

    std::vector<DiagramLabel> labels;
    std::vector<DiagramElectricalNet> electrical_nets;

    DiagramValidationReport validation;
};

} // namespace eke::dx::wire

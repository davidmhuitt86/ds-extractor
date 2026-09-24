#include "eke_dx_wire/diagram/engineering_diagram_builder.hpp"

#include <algorithm>
#include <cassert>
#include <string>

using namespace eke::dx::wire;

namespace {

ComponentCandidate make_component(
    const std::string& id,
    ComponentCandidateKind kind = ComponentCandidateKind::CircularSymbol) {
    ComponentCandidate c;
    c.id = id;
    c.kind = kind;
    c.bounds = BoundingBox{0, 0, 10, 10};
    return c;
}

EndpointCandidate make_endpoint(const std::string& id, const std::string& component_id = {}) {
    EndpointCandidate e;
    e.id = id;
    e.kind = EndpointKind::GeometricConductorEnd;
    e.component_id = component_id;
    return e;
}

Wire make_wire(const std::string& id, const std::string& start, const std::string& end) {
    Wire w;
    w.id = id;
    w.start_endpoint = start;
    w.end_endpoint = end;
    return w;
}

TopologyNode make_node(const std::string& id, TopologyNodeType type) {
    TopologyNode n;
    n.id = id;
    n.type = type;
    return n;
}

const DiagramComponent* find_component(
    const std::vector<DiagramComponent>& components, const std::string& id) {
    for (const auto& c : components) {
        if (c.component_id == id) return &c;
    }
    return nullptr;
}

bool has_issue(const DiagramValidationReport& report, const std::string& code) {
    return std::any_of(
        report.issues.begin(), report.issues.end(),
        [&](const DiagramRelationshipIssue& issue) { return issue.code == code; });
}

} // namespace

int main() {
    EngineeringDiagramBuilder builder;

    // 1. Empty engineering diagram: no crash, everything zeroed.
    {
        WireModel model;
        const auto diagram = builder.build(model);
        assert(diagram.components.empty());
        assert(diagram.connectors.empty());
        assert(diagram.wires.empty());
        assert(diagram.splices.empty());
        assert(diagram.labels.empty());
        assert(diagram.electrical_nets.empty());
        assert(diagram.validation.valid_references == 0);
        assert(diagram.validation.invalid_references == 0);
    }

    // 2. Component: basic passthrough fields.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        const auto diagram = builder.build(model);
        assert(diagram.components.size() == 1);
        assert(diagram.components[0].component_id == "comp-1");
        assert(diagram.components[0].kind == ComponentCandidateKind::CircularSymbol);
    }

    // 3. Component identity: Resolved canonicalization is reflected.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        ComponentIdentityResolution resolution;
        resolution.id = "resolution-1";
        resolution.component_id = "comp-1";
        resolution.evidence_ids = {"evidence-1"};
        model.component_identity_resolutions = {resolution};
        ComponentIdentityCanonicalization canonicalization;
        canonicalization.id = "canon-1";
        canonicalization.component_id = "comp-1";
        canonicalization.source_resolution_id = "resolution-1";
        canonicalization.canonical_name = "Regulator/Rectifier";
        canonicalization.status = ComponentIdentityCanonicalizationStatus::Resolved;
        model.component_identity_canonicalizations = {canonicalization};

        const auto diagram = builder.build(model);
        const auto* c = find_component(diagram.components, "comp-1");
        assert(c->canonical_name == "Regulator/Rectifier");
        assert(c->identity_status == DiagramObjectStatus::Resolved);
        assert(!c->identity_evidence_ids.empty());
    }

    // 4. Component symbol geometry: referenced by id, not duplicated.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        ComponentSymbolGeometry geometry;
        geometry.id = "geom-1";
        geometry.component_id = "comp-1";
        model.component_symbol_geometries = {geometry};

        const auto diagram = builder.build(model);
        assert(find_component(diagram.components, "comp-1")->symbol_geometry_id == "geom-1");
    }

    // 5. Symbol primitive: referenced correctly, validated against its
    // parent component.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        SymbolPrimitive primitive;
        primitive.id = "prim-1";
        primitive.component_id = "comp-1";
        model.symbol_primitives = {primitive};

        const auto diagram = builder.build(model);
        assert(!has_issue(diagram.validation, "PRIMITIVE-COMPONENT-MISSING"));
    }

    // 6. Endpoint: represented via wire start/end references.
    {
        WireModel model;
        model.endpoint_candidates = {make_endpoint("ep-a"), make_endpoint("ep-b")};
        model.wires = {make_wire("w1", "ep-a", "ep-b")};
        const auto diagram = builder.build(model);
        assert(diagram.wires[0].start_endpoint_id == "ep-a");
        assert(diagram.wires[0].end_endpoint_id == "ep-b");
    }

    // 7. Component terminal: TerminalCandidate resolved to endpoint via
    // EndpointCandidate.component_id (safe, only ever set on Resolved
    // reconstruction - see AP-WIRE-025 design doc).
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        model.endpoint_candidates = {make_endpoint("ep-a", "comp-1")};
        const auto diagram = builder.build(model);
        const auto* c = find_component(diagram.components, "comp-1");
        assert(c->endpoint_ids.size() == 1);
        assert(c->endpoint_ids[0] == "ep-a");
    }

    // 8. Connector: basic passthrough.
    {
        WireModel model;
        ConnectorCandidate connector;
        connector.id = "conn-1";
        connector.component_candidate_id = "comp-1";
        model.connector_candidates = {connector};
        const auto diagram = builder.build(model);
        assert(diagram.connectors.size() == 1);
        assert(diagram.connectors[0].connector_id == "conn-1");
    }

    // 9. Connector terminal: referenced by connector id.
    {
        WireModel model;
        ConnectorCandidate connector;
        connector.id = "conn-1";
        model.connector_candidates = {connector};
        ConnectorTerminal terminal;
        terminal.id = "ct-1";
        terminal.connector_id = "conn-1";
        terminal.endpoint_id = "ep-a";
        model.connector_terminals = {terminal};

        const auto diagram = builder.build(model);
        assert(diagram.connectors[0].connector_terminal_ids.size() == 1);
        assert(diagram.connectors[0].connector_terminal_ids[0] == "ct-1");
    }

    // 10. Wire: identity fields preserved unchanged.
    {
        WireModel model;
        model.wires = {make_wire("w1", "ep-a", "ep-b")};
        model.wires[0].topology_edges = {"e1", "e2"};
        model.wires[0].conductor_segments = {"s1"};
        model.wires[0].heavy_cable = true;
        model.edges = {{"e1", "n1", "n2", "s1"}, {"e2", "n2", "n3", "s1"}};

        const auto diagram = builder.build(model);
        assert(diagram.wires[0].wire_id == "w1");
        assert(diagram.wires[0].topology_edge_ids.size() == 2);
        assert(diagram.wires[0].conductor_segment_ids.size() == 1);
        assert(diagram.wires[0].heavy_cable);
    }

    // 11. Splice: a splice/junction node is never a wire endpoint, and
    // correctly lists the wires that pass through it.
    {
        WireModel model;
        model.nodes = {
            make_node("n-a", TopologyNodeType::ConductorEnd),
            make_node("n-splice", TopologyNodeType::Splice),
            make_node("n-b", TopologyNodeType::ConductorEnd),
            make_node("n-c", TopologyNodeType::ConductorEnd)};
        model.edges = {
            {"e1", "n-a", "n-splice", "s1"},
            {"e2", "n-splice", "n-b", "s1"},
            {"e3", "n-splice", "n-c", "s2"}};
        model.endpoint_candidates = {
            make_endpoint("ep-a"), make_endpoint("ep-b"), make_endpoint("ep-c")};
        model.wires = {
            make_wire("wire-ab", "ep-a", "ep-b"), make_wire("wire-ac", "ep-a", "ep-c")};
        model.wires[0].topology_edges = {"e1", "e2"};
        model.wires[1].topology_edges = {"e1", "e3"};

        const auto diagram = builder.build(model);
        assert(diagram.splices.size() == 1);
        assert(diagram.splices[0].node_id == "n-splice");
        assert(diagram.splices[0].incident_wire_ids.size() == 2);
    }

    // 12. Shared conductor: the same conductor/topology edge legitimately
    // referenced by two distinct wires - no duplication, no corruption.
    {
        WireModel model;
        model.wires = {
            make_wire("wire-ab", "ep-a", "ep-b"), make_wire("wire-ac", "ep-a", "ep-c")};
        model.wires[0].topology_edges = {"e-shared"};
        model.wires[1].topology_edges = {"e-shared"};
        model.wires[0].conductor_segments = {"seg-shared"};
        model.wires[1].conductor_segments = {"seg-shared"};

        const auto diagram = builder.build(model);
        assert(diagram.wires.size() == 2);
        assert(diagram.wires[0].topology_edge_ids[0] == "e-shared");
        assert(diagram.wires[1].topology_edge_ids[0] == "e-shared");
    }

    // 13. Electrical net: wires derived correctly, net/wire remain
    // distinct objects (never collapsed into each other).
    {
        WireModel model;
        model.wires = {make_wire("w1", "ep-a", "ep-b")};
        ElectricalNet net;
        net.id = "net-1";
        net.endpoint_ids = {"ep-a", "ep-b"};
        net.role = DistributionRole::Ground;
        model.electrical_nets = {net};

        const auto diagram = builder.build(model);
        assert(diagram.electrical_nets.size() == 1);
        assert(diagram.electrical_nets[0].wire_ids.size() == 1);
        assert(diagram.electrical_nets[0].wire_ids[0] == "w1");
        assert(diagram.electrical_nets[0].role == DistributionRole::Ground);
        // Net and wire remain separate collections/objects.
        assert(diagram.wires.size() == 1);
    }

    // 14. Wire semantic resolution: referenced by id, not duplicated.
    {
        WireModel model;
        model.wires = {make_wire("w1", "ep-a", "ep-b")};
        WireSemanticResolution resolution;
        resolution.id = "sem-1";
        resolution.wire_id = "w1";
        resolution.wire_color = "Blue";
        resolution.wire_color_status = WireSemanticStatus::Resolved;
        model.wire_semantics = {resolution};

        const auto diagram = builder.build(model);
        assert(diagram.wires[0].wire_semantic_resolution_id == "sem-1");
    }

    // 15. Unresolved identity: no canonicalization present -> Unresolved,
    // never fabricated.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        const auto diagram = builder.build(model);
        const auto* c = find_component(diagram.components, "comp-1");
        assert(c->identity_status == DiagramObjectStatus::Unresolved);
        assert(c->canonical_name.empty());
    }

    // 16. Conflicted identity: preserved, never silently resolved.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        ComponentIdentityCanonicalization canonicalization;
        canonicalization.id = "canon-1";
        canonicalization.component_id = "comp-1";
        canonicalization.status = ComponentIdentityCanonicalizationStatus::Conflicted;
        model.component_identity_canonicalizations = {canonicalization};

        const auto diagram = builder.build(model);
        const auto* c = find_component(diagram.components, "comp-1");
        assert(c->identity_status == DiagramObjectStatus::Conflicted);
        assert(c->canonical_name.empty());
    }

    // 17. Unresolved terminal: an endpoint with no component association
    // remains unrepresented in any component's endpoint_ids - never
    // guessed.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        model.endpoint_candidates = {make_endpoint("ep-a")}; // no component_id
        const auto diagram = builder.build(model);
        const auto* c = find_component(diagram.components, "comp-1");
        assert(c->endpoint_ids.empty());
    }

    // 18. Conflicted terminal (the AP-WIRE-024 known-conflict shape):
    // an endpoint whose reconstruction was Conflicted has component_id
    // cleared upstream (per AP-WIRE-019); the diagram must not invent an
    // association to compensate.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1"), make_component("comp-2")};
        // Conflicted reconstruction clears EndpointCandidate.component_id
        // to empty upstream - the diagram builder never sees a component
        // association for this endpoint at all.
        model.endpoint_candidates = {make_endpoint("ep-conflicted")};
        const auto diagram = builder.build(model);
        for (const auto& c : diagram.components) {
            assert(std::find(
                c.endpoint_ids.begin(), c.endpoint_ids.end(), "ep-conflicted") ==
                c.endpoint_ids.end());
        }
    }

    // 19. Unknown label: a TextRegion with no recognition evidence is
    // still represented, not discarded.
    {
        WireModel model;
        TextRegion region;
        region.id = "region-1";
        model.text_regions = {region};
        const auto diagram = builder.build(model);
        assert(diagram.labels.size() == 1);
        assert(diagram.labels[0].text_region_id == "region-1");
        assert(diagram.labels[0].raw_text.empty());
        assert(diagram.labels[0].status == DiagramObjectStatus::Unresolved);
    }

    // 20. Provenance/evidence: identity_evidence_ids traces back to the
    // originating ComponentIdentityEvidence/Resolution, not just the
    // final canonicalization.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        ComponentIdentityResolution resolution;
        resolution.id = "resolution-1";
        resolution.component_id = "comp-1";
        resolution.evidence_ids = {"evidence-a", "evidence-b"};
        model.component_identity_resolutions = {resolution};
        ComponentIdentityCanonicalization canonicalization;
        canonicalization.id = "canon-1";
        canonicalization.component_id = "comp-1";
        canonicalization.source_resolution_id = "resolution-1";
        canonicalization.status = ComponentIdentityCanonicalizationStatus::Resolved;
        canonicalization.canonical_name = "X";
        model.component_identity_canonicalizations = {canonicalization};

        const auto diagram = builder.build(model);
        const auto* c = find_component(diagram.components, "comp-1");
        assert(std::find(
            c->identity_evidence_ids.begin(), c->identity_evidence_ids.end(),
            "evidence-a") != c->identity_evidence_ids.end());
        assert(std::find(
            c->identity_evidence_ids.begin(), c->identity_evidence_ids.end(),
            "evidence-b") != c->identity_evidence_ids.end());
    }

    // 21. Invalid relationship: a dangling reference is detected and
    // reported, not silently dropped.
    {
        WireModel model;
        model.wires = {make_wire("w1", "ep-missing-a", "ep-missing-b")};
        const auto diagram = builder.build(model);
        assert(diagram.validation.invalid_references >= 2);
        assert(has_issue(diagram.validation, "WIRE-START-ENDPOINT-MISSING"));
        assert(has_issue(diagram.validation, "WIRE-END-ENDPOINT-MISSING"));
    }

    // 22. Duplicate relationship: two TerminalCandidates claiming the
    // same endpoint/component pair are flagged.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        model.endpoint_candidates = {make_endpoint("ep-a")};
        TerminalCandidate t1;
        t1.id = "t-1";
        t1.endpoint_id = "ep-a";
        t1.component_candidate_id = "comp-1";
        TerminalCandidate t2;
        t2.id = "t-2";
        t2.endpoint_id = "ep-a";
        t2.component_candidate_id = "comp-1";
        model.terminal_candidates = {t1, t2};

        const auto diagram = builder.build(model);
        assert(diagram.validation.duplicate_relationships >= 1);
        assert(has_issue(diagram.validation, "DUPLICATE-TERMINAL-COMPONENT-PAIR"));
    }

    // 23. Deterministic serialization: two builds of the same model
    // produce identical ordering and content.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-z"), make_component("comp-a")};
        model.wires = {make_wire("w-z", "ep-a", "ep-b"), make_wire("w-a", "ep-c", "ep-d")};

        const auto d1 = builder.build(model);
        const auto d2 = builder.build(model);

        assert(d1.components.size() == 2);
        assert(d1.components[0].component_id == "comp-a"); // sorted
        assert(d1.components[1].component_id == "comp-z");
        assert(d1.wires[0].wire_id == "w-a");
        assert(d1.wires[1].wire_id == "w-z");

        assert(d1.components.size() == d2.components.size());
        for (std::size_t i = 0; i < d1.components.size(); ++i) {
            assert(d1.components[i].component_id == d2.components[i].component_id);
        }
        for (std::size_t i = 0; i < d1.wires.size(); ++i) {
            assert(d1.wires[i].wire_id == d2.wires[i].wire_id);
        }
    }

    // 24. Known AP-WIRE-024 conflict preservation: simulates the actual
    // documented shape (endpoint whose component association is
    // Conflicted - component_id empty, no fabricated single winner).
    {
        WireModel model;
        model.component_candidates = {make_component("comp-real")};
        EndpointCandidate conflicted = make_endpoint("ep-conflicted");
        conflicted.component_id = ""; // AP-WIRE-019 clears this on Conflicted
        model.endpoint_candidates = {conflicted};
        Wire wire = make_wire("wire-430f59213afaef3a", "ep-other", "ep-conflicted");
        model.wires = {wire};
        model.endpoint_candidates.push_back(make_endpoint("ep-other", "comp-real"));

        const auto diagram = builder.build(model);
        const auto* c = find_component(diagram.components, "comp-real");
        assert(std::find(
            c->endpoint_ids.begin(), c->endpoint_ids.end(), "ep-conflicted") ==
            c->endpoint_ids.end());
        assert(std::find(
            c->endpoint_ids.begin(), c->endpoint_ids.end(), "ep-other") !=
            c->endpoint_ids.end());
    }

    // 25. Full assembly smoke test: a small but complete multi-object
    // model builds without error and every cross-reference resolves.
    {
        WireModel model;
        model.source_id = "fixture";
        model.image_width = 100;
        model.image_height = 100;
        model.component_candidates = {make_component("comp-1"), make_component("comp-2")};
        model.endpoint_candidates = {
            make_endpoint("ep-a", "comp-1"), make_endpoint("ep-b", "comp-2")};
        model.nodes = {
            make_node("n-a", TopologyNodeType::ConductorEnd),
            make_node("n-b", TopologyNodeType::ConductorEnd)};
        model.edges = {{"e1", "n-a", "n-b", "s1"}};
        model.conductor_segments = {{"s1"}};
        model.wires = {make_wire("w1", "ep-a", "ep-b")};
        model.wires[0].topology_edges = {"e1"};
        model.wires[0].conductor_segments = {"s1"};
        WireSemanticResolution resolution;
        resolution.id = "sem-1";
        resolution.wire_id = "w1";
        model.wire_semantics = {resolution};
        ElectricalNet net;
        net.id = "net-1";
        net.endpoint_ids = {"ep-a", "ep-b"};
        model.electrical_nets = {net};

        const auto diagram = builder.build(model);
        assert(diagram.components.size() == 2);
        assert(diagram.wires.size() == 1);
        assert(diagram.electrical_nets.size() == 1);
        assert(diagram.validation.invalid_references == 0);
        assert(diagram.validation.duplicate_relationships == 0);
    }

    return 0;
}

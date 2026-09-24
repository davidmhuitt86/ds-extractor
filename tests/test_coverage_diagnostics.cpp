#include "eke_dx_wire/core/coverage_diagnostics.hpp"

#include <cassert>
#include <string>

using namespace eke::dx::wire;

namespace {

ConductorSegment make_segment(const std::string& id) {
    ConductorSegment segment;
    segment.id = id;
    return segment;
}

TopologyNode make_node(const std::string& id, TopologyNodeType type) {
    TopologyNode node;
    node.id = id;
    node.type = type;
    return node;
}

TopologyEdge make_edge(
    const std::string& id,
    const std::string& from,
    const std::string& to,
    const std::string& segment) {
    TopologyEdge edge;
    edge.id = id;
    edge.from_node = from;
    edge.to_node = to;
    edge.conductor_segment = segment;
    return edge;
}

EndpointCandidate make_endpoint(const std::string& id, const std::string& node_id) {
    EndpointCandidate endpoint;
    endpoint.id = id;
    endpoint.node_id = node_id;
    endpoint.kind = EndpointKind::GeometricConductorEnd;
    return endpoint;
}

Wire make_wire(
    const std::string& id,
    const std::string& start,
    const std::string& end,
    std::vector<std::string> edges,
    std::vector<std::string> segments) {
    Wire wire;
    wire.id = id;
    wire.start_endpoint = start;
    wire.end_endpoint = end;
    wire.topology_edges = std::move(edges);
    wire.conductor_segments = std::move(segments);
    return wire;
}

const CoverageDiagnosticRecord* find_finding(
    const CoverageReport& report, const std::string& code, const std::string& object_id) {
    for (const auto& finding : report.findings) {
        if (finding.code == code && finding.object_id == object_id) {
            return &finding;
        }
    }
    return nullptr;
}

} // namespace

int main() {
    // classify_conductor_ownership: unit-level classification boundaries.
    {
        assert(classify_conductor_ownership(0, 0) == ConductorOwnershipClass::Unreferenced);
        assert(classify_conductor_ownership(3, 0) == ConductorOwnershipClass::TopologyOnly);
        assert(classify_conductor_ownership(0, 1) == ConductorOwnershipClass::WireOnly);
        assert(classify_conductor_ownership(1, 1) == ConductorOwnershipClass::Normal);
        assert(classify_conductor_ownership(1, 2) == ConductorOwnershipClass::Shared);
    }

    // classify_endpoint_wire_coverage boundaries.
    {
        assert(classify_endpoint_wire_coverage(0) == EndpointWireCoverageClass::ZeroWireEndpoint);
        assert(classify_endpoint_wire_coverage(1) == EndpointWireCoverageClass::SingleWireEndpoint);
        assert(classify_endpoint_wire_coverage(2) == EndpointWireCoverageClass::MultipleWireEndpoint);
    }

    // Empty model: every summary is zero, no findings.
    {
        WireModel model;
        const auto report = build_coverage_report(model);
        assert(report.conductors.total == 0);
        assert(report.endpoints.total == 0);
        assert(report.wires.total == 0);
        assert(report.topology_edges.total == 0);
        assert(report.topology_nodes.total == 0);
        assert(report.components.total == 0);
        assert(report.connectors.total == 0);
        assert(report.electrical_nets.total == 0);
        assert(report.findings.empty());
    }

    // Legitimate branching: a shared conductor segment traversed by two
    // distinct endpoint-to-endpoint wires must be classified SHARED with
    // Notice severity, never treated as an error merely because of dual
    // ownership. This is the AP-WIRE-022A example from section 3/5.1.
    {
        WireModel model;
        model.conductor_segments = {make_segment("seg-shared")};
        model.nodes = {
            make_node("n-a", TopologyNodeType::ConductorEnd),
            make_node("n-b", TopologyNodeType::ConductorEnd),
            make_node("n-c", TopologyNodeType::ConductorEnd),
        };
        model.edges = {make_edge("e1", "n-a", "n-b", "seg-shared")};
        model.endpoint_candidates = {
            make_endpoint("ep-a", "n-a"),
            make_endpoint("ep-b", "n-b"),
            make_endpoint("ep-c", "n-c"),
        };
        model.wires = {
            make_wire("wire-ab", "ep-a", "ep-b", {"e1"}, {"seg-shared"}),
            make_wire("wire-ac", "ep-a", "ep-c", {"e1"}, {"seg-shared"}),
        };

        const auto report = build_coverage_report(model);
        assert(report.conductors.shared == 1);
        assert(report.conductors.unreferenced == 0);
        assert(report.conductors.wire_only == 0);

        const auto* shared_finding = find_finding(report, "CONDUCTOR-SHARED", "seg-shared");
        assert(shared_finding != nullptr);
        assert(shared_finding->severity == CoverageSeverity::Notice);
        assert(shared_finding->related_object_ids.size() == 2);

        // ep-a is the terminus of both wires: legitimate multi-ownership.
        assert(report.endpoints.multiple_wire == 1);
        const auto* multi_endpoint = find_finding(report, "ENDPOINT-MULTIPLE-WIRE", "ep-a");
        assert(multi_endpoint != nullptr);
        assert(multi_endpoint->severity == CoverageSeverity::Notice);
        assert(report.endpoints.single_wire == 2); // ep-b, ep-c
    }

    // Actual invalid ownership: a conductor segment with no topology and no
    // wire reference must be UNREFERENCED with Warning severity.
    {
        WireModel model;
        model.conductor_segments = {make_segment("seg-orphan")};
        const auto report = build_coverage_report(model);
        assert(report.conductors.unreferenced == 1);
        const auto* finding = find_finding(report, "CONDUCTOR-UNREFERENCED", "seg-orphan");
        assert(finding != nullptr);
        assert(finding->severity == CoverageSeverity::Warning);
    }

    // A wire referencing a conductor segment that topology never claims is
    // WIRE-ONLY and must be flagged, since real geometry ownership requires
    // a topology edge to exist.
    {
        WireModel model;
        model.conductor_segments = {make_segment("seg-wire-only")};
        model.endpoint_candidates = {
            make_endpoint("ep-x", "n-x"), make_endpoint("ep-y", "n-y")};
        model.wires = {
            make_wire("wire-xy", "ep-x", "ep-y", {}, {"seg-wire-only"})};

        const auto report = build_coverage_report(model);
        assert(report.conductors.wire_only == 1);
        const auto* finding = find_finding(report, "CONDUCTOR-WIRE-ONLY", "seg-wire-only");
        assert(finding != nullptr);
        assert(finding->severity == CoverageSeverity::Warning);
    }

    // DiagramFurniture without terminal evidence must NOT be treated as an
    // electrical-component failure (section 5.6 / 14 requirement).
    {
        WireModel model;
        ComponentCandidate furniture;
        furniture.id = "comp-furniture";
        furniture.kind = ComponentCandidateKind::DiagramFurniture;
        ComponentCandidate real_component;
        real_component.id = "comp-real";
        real_component.kind = ComponentCandidateKind::CircularSymbol;
        model.component_candidates = {furniture, real_component};

        const auto report = build_coverage_report(model);
        assert(report.components.diagram_furniture == 1);
        assert(report.components.furniture_without_terminal_evidence == 1);
        assert(report.components.real_candidates == 1);
        assert(report.components.real_without_terminal_evidence == 1);

        // Furniture must produce no COMPONENT-NO-TERMINAL-EVIDENCE finding.
        assert(find_finding(report, "COMPONENT-NO-TERMINAL-EVIDENCE", "comp-furniture") == nullptr);
        const auto* real_finding =
            find_finding(report, "COMPONENT-NO-TERMINAL-EVIDENCE", "comp-real");
        assert(real_finding != nullptr);
        assert(real_finding->severity == CoverageSeverity::Warning);
    }

    // A component with terminal evidence is not flagged.
    {
        WireModel model;
        ComponentCandidate component;
        component.id = "comp-with-terminal";
        component.kind = ComponentCandidateKind::CircularSymbol;
        model.component_candidates = {component};

        TerminalCandidate terminal;
        terminal.id = "term-1";
        terminal.component_candidate_id = "comp-with-terminal";
        model.terminal_candidates = {terminal};

        const auto report = build_coverage_report(model);
        assert(report.components.real_with_terminal_evidence == 1);
        assert(report.components.real_without_terminal_evidence == 0);
        assert(find_finding(report, "COMPONENT-NO-TERMINAL-EVIDENCE", "comp-with-terminal") == nullptr);
    }

    // Connector coverage: furniture-derived connectors are flagged
    // distinctly from genuinely unresolved connectors.
    {
        WireModel model;
        ComponentCandidate furniture;
        furniture.id = "comp-furniture";
        furniture.kind = ComponentCandidateKind::DiagramFurniture;
        model.component_candidates = {furniture};

        ConnectorCandidate connector_from_furniture;
        connector_from_furniture.id = "conn-furniture";
        connector_from_furniture.component_candidate_id = "comp-furniture";

        ConnectorCandidate connector_unresolved;
        connector_unresolved.id = "conn-unresolved";
        connector_unresolved.component_candidate_id = "comp-missing";

        model.connector_candidates = {connector_from_furniture, connector_unresolved};

        const auto report = build_coverage_report(model);
        assert(report.connectors.furniture_derived == 1);
        assert(report.connectors.unresolved == 1);
        assert(report.connectors.genuine_looking == 0);
        assert(find_finding(report, "CONNECTOR-FURNITURE-DERIVED", "conn-furniture") != nullptr);
        assert(find_finding(report, "CONNECTOR-UNRESOLVED", "conn-unresolved") != nullptr);
    }

    // Electrical net coverage: an endpoint owned by two nets is a Warning
    // (deterministic exclusivity is expected); an endpoint owned by zero
    // nets is a Notice (expected during incomplete net resolution).
    {
        WireModel model;
        model.endpoint_candidates = {
            make_endpoint("ep-multi", "n-1"),
            make_endpoint("ep-none", "n-2"),
        };
        ElectricalNet net_a;
        net_a.id = "net-a";
        net_a.endpoint_ids = {"ep-multi"};
        ElectricalNet net_b;
        net_b.id = "net-b";
        net_b.endpoint_ids = {"ep-multi"};
        model.electrical_nets = {net_a, net_b};

        const auto report = build_coverage_report(model);
        assert(report.electrical_nets.endpoints_in_multiple_nets == 1);
        assert(report.electrical_nets.endpoints_not_in_any_net == 1);

        const auto* multi = find_finding(report, "NET-ENDPOINT-MULTI-OWNED", "ep-multi");
        assert(multi != nullptr);
        assert(multi->severity == CoverageSeverity::Warning);
        assert(multi->related_object_ids.size() == 2);

        const auto* unowned = find_finding(report, "NET-ENDPOINT-UNOWNED", "ep-none");
        assert(unowned != nullptr);
        assert(unowned->severity == CoverageSeverity::Notice);
    }

    // Deterministic ordering: findings must be sorted by
    // (category, code, object_id) regardless of model insertion order.
    {
        WireModel model;
        model.conductor_segments = {
            make_segment("seg-z"), make_segment("seg-a"), make_segment("seg-m")};

        const auto report = build_coverage_report(model);
        assert(report.findings.size() == 3);
        assert(report.findings[0].object_id == "seg-a");
        assert(report.findings[1].object_id == "seg-m");
        assert(report.findings[2].object_id == "seg-z");

        // Re-running against the same model must reproduce the identical
        // ordering (no reliance on hash/pointer/timestamp iteration order).
        const auto report2 = build_coverage_report(model);
        assert(report.findings.size() == report2.findings.size());
        for (std::size_t i = 0; i < report.findings.size(); ++i) {
            assert(report.findings[i].code == report2.findings[i].code);
            assert(report.findings[i].object_id == report2.findings[i].object_id);
        }
    }

    // Topology node coverage: zero-degree node and low-degree splice.
    {
        WireModel model;
        model.nodes = {
            make_node("n-isolated", TopologyNodeType::Continuation),
            make_node("n-splice", TopologyNodeType::Splice),
        };
        // n-splice has exactly one incident edge -> low-degree splice.
        model.edges = {make_edge("e1", "n-splice", "n-splice2", "")};
        model.nodes.push_back(make_node("n-splice2", TopologyNodeType::ConductorEnd));

        const auto report = build_coverage_report(model);
        assert(report.topology_nodes.zero_degree == 1);
        assert(find_finding(report, "TOPOLOGY-NODE-ZERO-DEGREE", "n-isolated") != nullptr);
        assert(report.topology_nodes.low_degree_splice == 1);
        assert(find_finding(report, "TOPOLOGY-NODE-LOW-DEGREE-SPLICE", "n-splice") != nullptr);
        // n-splice2 is a conductor end with no endpoint candidate.
        assert(report.topology_nodes.conductor_end_without_endpoint == 1);
    }

    // Wire integrity coverage cross-references WireModelValidator's issues.
    {
        WireModel model;
        model.endpoint_candidates = {
            make_endpoint("ep-a", "n-a"), make_endpoint("ep-b", "n-b")};
        model.wires = {make_wire("wire-ok", "ep-a", "ep-b", {}, {})};
        WireValidationIssue error_issue;
        error_issue.severity = WireValidationSeverity::Error;
        error_issue.code = "WIRE-NO-TOPOLOGY";
        error_issue.object_id = "wire-ok";
        model.wire_validation.issues = {error_issue};

        const auto report = build_coverage_report(model);
        assert(report.wires.total == 1);
        assert(report.wires.invalid == 1);
        assert(report.wires.valid == 0);
        assert(find_finding(report, "WIRE-INTEGRITY-FAILED", "wire-ok") != nullptr);
    }

    return 0;
}

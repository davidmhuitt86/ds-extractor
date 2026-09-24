#include "eke_dx_wire/topology/physical_wire_identity_reconstructor.hpp"

#include <algorithm>
#include <cassert>
#include <string>

using namespace eke::dx::wire;

namespace {

TopologyNode make_node(
    const std::string& id, TopologyNodeType type, double x = 0.0, double y = 0.0) {
    TopologyNode n;
    n.id = id;
    n.type = type;
    n.position = Point2D{x, y};
    return n;
}

TopologyEdge make_edge(
    const std::string& id, const std::string& from, const std::string& to,
    const std::string& segment) {
    TopologyEdge e;
    e.id = id;
    e.from_node = from;
    e.to_node = to;
    e.conductor_segment = segment;
    return e;
}

ConductorSegment make_segment(const std::string& id) {
    ConductorSegment s;
    s.id = id;
    return s;
}

EndpointCandidate make_endpoint(const std::string& id, const std::string& node_id) {
    EndpointCandidate e;
    e.id = id;
    e.node_id = node_id;
    e.kind = EndpointKind::ComponentTerminal;
    return e;
}

ConductorBoundaryResolution make_resolved_boundary(const std::string& endpoint_id) {
    ConductorBoundaryResolution r;
    r.id = "cbr-" + endpoint_id;
    r.endpoint_id = endpoint_id;
    r.boundary_kind = EndpointKind::ComponentTerminal;
    r.boundary_status = ConductorBoundaryStatus::Resolved;
    return r;
}

const Wire* find_wire_between(
    const std::vector<Wire>& wires, const std::string& a, const std::string& b) {
    for (const auto& w : wires) {
        if ((w.start_endpoint == a && w.end_endpoint == b) ||
            (w.start_endpoint == b && w.end_endpoint == a)) {
            return &w;
        }
    }
    return nullptr;
}

bool any_wire_touches(const std::vector<Wire>& wires, const std::string& id) {
    return std::any_of(wires.begin(), wires.end(), [&](const Wire& w) {
        return w.start_endpoint == id || w.end_endpoint == id;
    });
}

} // namespace

int main() {
    PhysicalWireIdentityReconstructor reconstructor;

    // 1. Simple two-terminal wire -> one Wire, Resolved.
    {
        std::vector<TopologyNode> nodes = {
            make_node("n-a", TopologyNodeType::ConductorEnd),
            make_node("n-b", TopologyNodeType::ConductorEnd)};
        std::vector<TopologyEdge> edges = {make_edge("e1", "n-a", "n-b", "s1")};
        std::vector<ConductorSegment> segments = {make_segment("s1")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-a", "n-a"), make_endpoint("ep-b", "n-b")};
        std::vector<ConductorBoundaryResolution> boundaries = {
            make_resolved_boundary("ep-a"), make_resolved_boundary("ep-b")};

        const auto artifacts = reconstructor.reconstruct(
            nodes, edges, endpoints, segments, boundaries, "src", 0);
        assert(artifacts.wires.size() == 1);
        assert(artifacts.wires[0].identity_status == WireIdentityStatus::Resolved);
        assert(!artifacts.wires[0].identity_evidence_ids.empty());
    }

    // 2. Two-terminal wire passing through Continuation nodes -> one
    // Wire, Resolved.
    {
        std::vector<TopologyNode> nodes = {
            make_node("n-a", TopologyNodeType::ConductorEnd),
            make_node("n-mid1", TopologyNodeType::Continuation),
            make_node("n-mid2", TopologyNodeType::Continuation),
            make_node("n-b", TopologyNodeType::ConductorEnd)};
        std::vector<TopologyEdge> edges = {
            make_edge("e1", "n-a", "n-mid1", "s1"),
            make_edge("e2", "n-mid1", "n-mid2", "s2"),
            make_edge("e3", "n-mid2", "n-b", "s3")};
        std::vector<ConductorSegment> segments = {
            make_segment("s1"), make_segment("s2"), make_segment("s3")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-a", "n-a"), make_endpoint("ep-b", "n-b")};
        std::vector<ConductorBoundaryResolution> boundaries = {
            make_resolved_boundary("ep-a"), make_resolved_boundary("ep-b")};

        const auto artifacts = reconstructor.reconstruct(
            nodes, edges, endpoints, segments, boundaries, "src", 0);
        assert(artifacts.wires.size() == 1);
        assert(artifacts.wires[0].topology_edges.size() == 3);
        assert(artifacts.wires[0].identity_status == WireIdentityStatus::Resolved);
    }

    // 3. Two-terminal wire containing a Splice node but with established
    // physical continuity (shared conductor segment across the splice) ->
    // one endpoint-to-endpoint Wire; the splice is never an endpoint.
    // Also exercises scenario 5 (three-way splice where segment continuity
    // provides sufficient evidence for a specific physical path): the
    // third branch (ep-c) has its own, different segment and correctly
    // gets no wire.
    {
        std::vector<TopologyNode> nodes = {
            make_node("n-a", TopologyNodeType::ConductorEnd),
            make_node("n-splice", TopologyNodeType::Splice),
            make_node("n-b", TopologyNodeType::ConductorEnd),
            make_node("n-c", TopologyNodeType::ConductorEnd)};
        std::vector<TopologyEdge> edges = {
            make_edge("e1", "n-a", "n-splice", "segA"),
            make_edge("e2", "n-splice", "n-b", "segA"), // same segment as e1
            make_edge("e3", "n-splice", "n-c", "segC")}; // distinct branch
        std::vector<ConductorSegment> segments = {
            make_segment("segA"), make_segment("segC")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-a", "n-a"), make_endpoint("ep-b", "n-b"),
            make_endpoint("ep-c", "n-c")};
        std::vector<ConductorBoundaryResolution> boundaries = {
            make_resolved_boundary("ep-a"), make_resolved_boundary("ep-b"),
            make_resolved_boundary("ep-c")};

        const auto artifacts = reconstructor.reconstruct(
            nodes, edges, endpoints, segments, boundaries, "src", 0);
        assert(artifacts.wires.size() == 1);
        const auto* wire = find_wire_between(artifacts.wires, "ep-a", "ep-b");
        assert(wire != nullptr);
        assert(wire->identity_status == WireIdentityStatus::Resolved);
        assert(!any_wire_touches(artifacts.wires, "n-splice"));
        // ep-c: no evidence its branch continues anywhere - no wire.
        assert(!any_wire_touches(artifacts.wires, "ep-c"));
    }

    // 4. Three-way splice with insufficient physical continuity evidence
    // (all three branches have distinct conductor segments) -> no
    // invented branch pairing, physical identity remains unresolved for
    // all three endpoints (no wire created).
    {
        std::vector<TopologyNode> nodes = {
            make_node("n-a", TopologyNodeType::ConductorEnd),
            make_node("n-splice", TopologyNodeType::Splice),
            make_node("n-b", TopologyNodeType::ConductorEnd),
            make_node("n-c", TopologyNodeType::ConductorEnd)};
        std::vector<TopologyEdge> edges = {
            make_edge("e1", "n-a", "n-splice", "segA"),
            make_edge("e2", "n-splice", "n-b", "segB"),
            make_edge("e3", "n-splice", "n-c", "segC")};
        std::vector<ConductorSegment> segments = {
            make_segment("segA"), make_segment("segB"), make_segment("segC")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-a", "n-a"), make_endpoint("ep-b", "n-b"),
            make_endpoint("ep-c", "n-c")};
        std::vector<ConductorBoundaryResolution> boundaries = {
            make_resolved_boundary("ep-a"), make_resolved_boundary("ep-b"),
            make_resolved_boundary("ep-c")};

        const auto artifacts = reconstructor.reconstruct(
            nodes, edges, endpoints, segments, boundaries, "src", 0);
        assert(artifacts.wires.empty());
    }

    // 6. Crossing: two independent conductors crossing with no shared
    // segment create no connection and no wire spans between them; the
    // crossing node is never a wire endpoint. A second case shows a
    // single conductor legitimately drawn straight through a crossing
    // (shared segment on both sides) still forms one wire without ever
    // connecting to the other, unrelated line.
    {
        std::vector<TopologyNode> nodes = {
            make_node("n-a", TopologyNodeType::ConductorEnd),
            make_node("n-cross", TopologyNodeType::Crossing),
            make_node("n-b", TopologyNodeType::ConductorEnd),
            make_node("n-c", TopologyNodeType::ConductorEnd),
            make_node("n-d", TopologyNodeType::ConductorEnd)};
        std::vector<TopologyEdge> edges = {
            make_edge("e1", "n-a", "n-cross", "segAB"),
            make_edge("e2", "n-cross", "n-b", "segAB"), // same line as e1
            make_edge("e3", "n-cross", "n-c", "segC"),  // unrelated line
            make_edge("e4", "n-cross", "n-d", "segD")}; // unrelated line, different segment than e3
        std::vector<ConductorSegment> segments = {
            make_segment("segAB"), make_segment("segC"), make_segment("segD")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-a", "n-a"), make_endpoint("ep-b", "n-b"),
            make_endpoint("ep-c", "n-c"), make_endpoint("ep-d", "n-d")};
        std::vector<ConductorBoundaryResolution> boundaries = {
            make_resolved_boundary("ep-a"), make_resolved_boundary("ep-b"),
            make_resolved_boundary("ep-c"), make_resolved_boundary("ep-d")};

        const auto artifacts = reconstructor.reconstruct(
            nodes, edges, endpoints, segments, boundaries, "src", 0);
        // Only a-b forms a wire (they share segAB, i.e. one continuous
        // conductor drawn through the crossing point).
        assert(artifacts.wires.size() == 1);
        const auto* wire = find_wire_between(artifacts.wires, "ep-a", "ep-b");
        assert(wire != nullptr);
        assert(wire->identity_status == WireIdentityStatus::Resolved);
        // The crossing establishes no connectivity and is never an
        // endpoint; c and d (different segments, no sharing) get no wire
        // at all - never wired to a, b, or each other.
        assert(!any_wire_touches(artifacts.wires, "n-cross"));
        assert(!any_wire_touches(artifacts.wires, "ep-c"));
        assert(!any_wire_touches(artifacts.wires, "ep-d"));
    }

    // 7. Shared conductor section: the same ConductorSegment id
    // legitimately referenced by two structurally independent wires (the
    // existing CONDUCTOR-SHARED representation case) does not confuse
    // reconstruction - both wires are still produced correctly.
    {
        std::vector<TopologyNode> nodes = {
            make_node("n-a", TopologyNodeType::ConductorEnd),
            make_node("n-b", TopologyNodeType::ConductorEnd),
            make_node("n-c", TopologyNodeType::ConductorEnd),
            make_node("n-d", TopologyNodeType::ConductorEnd)};
        std::vector<TopologyEdge> edges = {
            make_edge("e1", "n-a", "n-b", "shared-seg"),
            make_edge("e2", "n-c", "n-d", "shared-seg")};
        std::vector<ConductorSegment> segments = {make_segment("shared-seg")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-a", "n-a"), make_endpoint("ep-b", "n-b"),
            make_endpoint("ep-c", "n-c"), make_endpoint("ep-d", "n-d")};
        std::vector<ConductorBoundaryResolution> boundaries = {
            make_resolved_boundary("ep-a"), make_resolved_boundary("ep-b"),
            make_resolved_boundary("ep-c"), make_resolved_boundary("ep-d")};

        const auto artifacts = reconstructor.reconstruct(
            nodes, edges, endpoints, segments, boundaries, "src", 0);
        assert(artifacts.wires.size() == 2);
        assert(find_wire_between(artifacts.wires, "ep-a", "ep-b") != nullptr);
        assert(find_wire_between(artifacts.wires, "ep-c", "ep-d") != nullptr);
    }

    // 8. Conflicting evidence: at a splice, the incoming conductor's
    // segment matches TWO other incident edges simultaneously (segX
    // shared by e1, e2, AND e3) - a genuine evidence contradiction, since
    // one physical conductor cannot continue in two directions at once.
    // Both candidate continuations are produced, but marked Conflicted;
    // neither is silently preferred.
    {
        std::vector<TopologyNode> nodes = {
            make_node("n-a", TopologyNodeType::ConductorEnd),
            make_node("n-splice", TopologyNodeType::Splice),
            make_node("n-b", TopologyNodeType::ConductorEnd),
            make_node("n-c", TopologyNodeType::ConductorEnd)};
        std::vector<TopologyEdge> edges = {
            make_edge("e1", "n-a", "n-splice", "segX"),
            make_edge("e2", "n-splice", "n-b", "segX"),
            make_edge("e3", "n-splice", "n-c", "segX")};
        std::vector<ConductorSegment> segments = {make_segment("segX")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-a", "n-a"), make_endpoint("ep-b", "n-b"),
            make_endpoint("ep-c", "n-c")};
        std::vector<ConductorBoundaryResolution> boundaries = {
            make_resolved_boundary("ep-a"), make_resolved_boundary("ep-b"),
            make_resolved_boundary("ep-c")};

        const auto artifacts = reconstructor.reconstruct(
            nodes, edges, endpoints, segments, boundaries, "src", 0);
        assert(artifacts.wires.size() == 2);
        for (const auto& wire : artifacts.wires) {
            assert(wire.identity_status == WireIdentityStatus::Conflicted);
        }
        assert(find_wire_between(artifacts.wires, "ep-a", "ep-b") != nullptr);
        assert(find_wire_between(artifacts.wires, "ep-a", "ep-c") != nullptr);
    }

    // 9. Ground-anchored distribution: physical Wire reconstruction takes
    // no ElectricalNet parameter at all (see the header) and is fully
    // exercised here using only Ground-kind endpoints, without any
    // ElectricalNet object existing anywhere in this test - confirming
    // Wire identity reconstruction is structurally independent of
    // electrical-net role resolution.
    {
        std::vector<TopologyNode> nodes = {
            make_node("n-a", TopologyNodeType::ConductorEnd),
            make_node("n-gnd", TopologyNodeType::ConductorEnd)};
        std::vector<TopologyEdge> edges = {make_edge("e1", "n-a", "n-gnd", "s1")};
        std::vector<ConductorSegment> segments = {make_segment("s1")};
        EndpointCandidate ground_endpoint = make_endpoint("ep-gnd", "n-gnd");
        ground_endpoint.kind = EndpointKind::Ground;
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-a", "n-a"), ground_endpoint};
        std::vector<ConductorBoundaryResolution> boundaries = {
            make_resolved_boundary("ep-a")};
        ConductorBoundaryResolution ground_boundary =
            make_resolved_boundary("ep-gnd");
        ground_boundary.boundary_kind = EndpointKind::Ground;
        ground_boundary.ground_status = ConductorBoundaryStatus::Resolved;
        boundaries.push_back(ground_boundary);

        const auto artifacts = reconstructor.reconstruct(
            nodes, edges, endpoints, segments, boundaries, "src", 0);
        assert(artifacts.wires.size() == 1);
        assert(artifacts.wires[0].identity_status == WireIdentityStatus::Resolved);
    }

    // A bare geometric conductor end (no AP-WIRE-030 Resolved boundary)
    // is never treated as a new Wire boundary by the extended pass: with
    // an unresolved boundary on one side, no wire crosses the splice even
    // though the segment-sharing evidence itself would otherwise justify
    // it.
    {
        std::vector<TopologyNode> nodes = {
            make_node("n-a", TopologyNodeType::ConductorEnd),
            make_node("n-splice", TopologyNodeType::Splice),
            make_node("n-b", TopologyNodeType::ConductorEnd),
            make_node("n-c", TopologyNodeType::ConductorEnd)};
        std::vector<TopologyEdge> edges = {
            make_edge("e1", "n-a", "n-splice", "segA"),
            make_edge("e2", "n-splice", "n-b", "segA"),
            make_edge("e3", "n-splice", "n-c", "segC")};
        std::vector<ConductorSegment> segments = {
            make_segment("segA"), make_segment("segC")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-a", "n-a"), make_endpoint("ep-b", "n-b")};
        std::vector<ConductorBoundaryResolution> boundaries = {
            make_resolved_boundary("ep-a")};
        // ep-b has no ConductorBoundaryResolution at all - i.e. never
        // reached AP-WIRE-030's Resolved state.

        const auto artifacts = reconstructor.reconstruct(
            nodes, edges, endpoints, segments, boundaries, "src", 0);
        assert(artifacts.wires.empty());
    }

    // Deterministic output across repeated calls on identical input.
    {
        std::vector<TopologyNode> nodes = {
            make_node("n-a", TopologyNodeType::ConductorEnd),
            make_node("n-splice", TopologyNodeType::Splice),
            make_node("n-b", TopologyNodeType::ConductorEnd),
            make_node("n-c", TopologyNodeType::ConductorEnd)};
        std::vector<TopologyEdge> edges = {
            make_edge("e1", "n-a", "n-splice", "segA"),
            make_edge("e2", "n-splice", "n-b", "segA"),
            make_edge("e3", "n-splice", "n-c", "segC")};
        std::vector<ConductorSegment> segments = {
            make_segment("segA"), make_segment("segC")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("ep-a", "n-a"), make_endpoint("ep-b", "n-b"),
            make_endpoint("ep-c", "n-c")};
        std::vector<ConductorBoundaryResolution> boundaries = {
            make_resolved_boundary("ep-a"), make_resolved_boundary("ep-b"),
            make_resolved_boundary("ep-c")};

        const auto first = reconstructor.reconstruct(
            nodes, edges, endpoints, segments, boundaries, "src", 0);
        const auto second = reconstructor.reconstruct(
            nodes, edges, endpoints, segments, boundaries, "src", 0);
        assert(first.wires.size() == second.wires.size());
        for (std::size_t i = 0; i < first.wires.size(); ++i) {
            assert(first.wires[i].id == second.wires[i].id);
            assert(first.wires[i].identity_status == second.wires[i].identity_status);
        }
    }

    return 0;
}

        assert(find_wire_between(artifacts.wires, "ep-a", "ep-c") != nullptr);
        assert(find_wire_between(artifacts.wires, "ep-a", "ep-d") != nullptr);
        assert(find_wire_between(artifacts.wires, "ep-b", "ep-c") == nullptr);
        assert(find_wire_between(artifacts.wires, "ep-b", "ep-d") == nullptr);
        assert(find_wire_between(artifacts.wires, "ep-c", "ep-d") == nullptr);
    }

    // AP-WIRE-FIX-003 regression: the same three-way ambiguous fork as
    // test 8, but only two of the three tied edges have an eligible
    // (Resolved-boundary) endpoint on their far side (ep-a's own boundary
    // is unresolved, so ep-a never seeds a walk). The single legitimate
    // identity the remaining evidence supports (ep-b to ep-c) must still
    // be discovered - the ambiguity dedup must not depend on which tied
    // edge happens to be globally "first" and accidentally suppress the
    // only reachable pairing.
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
        EndpointCandidate unresolved_a = make_endpoint("ep-a", "n-a");
        unresolved_a.kind = EndpointKind::Unresolved;
        std::vector<EndpointCandidate> endpoints = {
            unresolved_a, make_endpoint("ep-b", "n-b"),
            make_endpoint("ep-c", "n-c")};
        std::vector<ConductorBoundaryResolution> boundaries = {
            make_resolved_boundary("ep-b"), make_resolved_boundary("ep-c")};
        // ep-a is explicitly an unresolved endpoint and therefore remains
        // ineligible to seed a physical-identity walk.

        const auto artifacts = reconstructor.reconstruct(
            nodes, edges, endpoints, segments, boundaries, "src", 0);
        assert(artifacts.wires.size() == 1);
        const auto* wire = find_wire_between(artifacts.wires, "ep-b", "ep-c");
        assert(wire != nullptr);
        assert(wire->identity_status == WireIdentityStatus::Conflicted);
        assert(!any_wire_touches(artifacts.wires, "ep-a"));
    }

    // 9. Ground-anchored distribution: physical Wire reconstruction takes
    // no ElectricalNet parameter at all (see the header) and is fully
    // exercised here using only Ground-kind endpoints, without any
#include "eke_dx_wire/topology/wire_semantic_resolver.hpp"

#include <cassert>
#include <string>

using namespace eke::dx::wire;

namespace {

EndpointCandidate make_endpoint(
    const std::string& id,
    const std::string& wire_color = {},
    const std::string& function_label = {}) {
    EndpointCandidate e;
    e.id = id;
    e.kind = EndpointKind::GeometricConductorEnd;
    e.wire_color = wire_color;
    e.function_label = function_label;
    return e;
}

Wire make_wire(const std::string& id, const std::string& start, const std::string& end) {
    Wire w;
    w.id = id;
    w.start_endpoint = start;
    w.end_endpoint = end;
    return w;
}

EndpointSemanticReconstruction make_reconstruction(
    const std::string& endpoint_id,
    EndpointSemanticReconstructionStatus status,
    const std::string& component_id = {}) {
    EndpointSemanticReconstruction r;
    r.endpoint_id = endpoint_id;
    r.status = status;
    r.component_id = component_id;
    return r;
}

const WireSemanticResolution& find_resolution(
    const std::vector<WireSemanticResolution>& resolutions, const std::string& wire_id) {
    for (const auto& r : resolutions) {
        if (r.wire_id == wire_id) return r;
    }
    static WireSemanticResolution empty;
    assert(false && "resolution not found");
    return empty;
}

} // namespace

int main() {
    WireSemanticResolver resolver;

    // 1. Explicit wire-color evidence at both ends, agreeing -> Resolved,
    // High confidence, reinforced.
    {
        std::vector<Wire> wires = {make_wire("w1", "a", "b")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("a", "Blue"), make_endpoint("b", "Blue")};
        const auto result = resolver.resolve(wires, endpoints, {}, {}, {});
        const auto& r = find_resolution(result.resolutions, "w1");
        assert(r.wire_color_status == WireSemanticStatus::Resolved);
        assert(r.wire_color == "Blue");
        assert(r.wire_color_confidence == ConfidenceClass::High);
    }

    // 2. Compatible evidence from a single source (only one end has
    // evidence) -> Resolved but weaker (Medium) confidence.
    {
        std::vector<Wire> wires = {make_wire("w1", "a", "b")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("a", "Blue"), make_endpoint("b")};
        const auto result = resolver.resolve(wires, endpoints, {}, {}, {});
        const auto& r = find_resolution(result.resolutions, "w1");
        assert(r.wire_color_status == WireSemanticStatus::Resolved);
        assert(r.wire_color == "Blue");
        assert(r.wire_color_confidence == ConfidenceClass::Medium);
    }

    // 3. Conflicting wire-color evidence -> Conflicted, never averaged or
    // arbitrarily picked.
    {
        std::vector<Wire> wires = {make_wire("w1", "a", "b")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("a", "Blue"), make_endpoint("b", "Red")};
        const auto result = resolver.resolve(wires, endpoints, {}, {}, {});
        const auto& r = find_resolution(result.resolutions, "w1");
        assert(r.wire_color_status == WireSemanticStatus::Conflicted);
        assert(r.wire_color.empty());
        assert(r.wire_color_confidence == ConfidenceClass::Unresolved);
    }

    // 4. Explicit function evidence -> Resolved (same mechanism as color).
    {
        std::vector<Wire> wires = {make_wire("w1", "a", "b")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("a", "", "horn"), make_endpoint("b", "", "horn")};
        const auto result = resolver.resolve(wires, endpoints, {}, {}, {});
        const auto& r = find_resolution(result.resolutions, "w1");
        assert(r.function_status == WireSemanticStatus::Resolved);
        assert(r.function_label == "horn");
        assert(r.function_confidence == ConfidenceClass::High);
    }

    // 5. Conflicting function evidence -> Conflicted.
    {
        std::vector<Wire> wires = {make_wire("w1", "a", "b")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("a", "", "horn"), make_endpoint("b", "", "brake")};
        const auto result = resolver.resolve(wires, endpoints, {}, {}, {});
        const auto& r = find_resolution(result.resolutions, "w1");
        assert(r.function_status == WireSemanticStatus::Conflicted);
        assert(r.function_label.empty());
    }

    // 6. No evidence at all -> Unresolved (never guessed).
    {
        std::vector<Wire> wires = {make_wire("w1", "a", "b")};
        std::vector<EndpointCandidate> endpoints = {make_endpoint("a"), make_endpoint("b")};
        const auto result = resolver.resolve(wires, endpoints, {}, {}, {});
        const auto& r = find_resolution(result.resolutions, "w1");
        assert(r.wire_color_status == WireSemanticStatus::Unresolved);
        assert(r.function_status == WireSemanticStatus::Unresolved);
        assert(r.start_component_status == WireSemanticStatus::Unresolved);
        assert(r.electrical_net_status == WireSemanticStatus::Unresolved);
    }

    // 7. AP-WIRE-024 conflict constraint: a Conflicted
    // EndpointSemanticReconstruction must never be treated as authoritative
    // wire semantic evidence for component association.
    {
        std::vector<Wire> wires = {make_wire("w1", "a", "b")};
        std::vector<EndpointCandidate> endpoints = {make_endpoint("a"), make_endpoint("b")};
        std::vector<EndpointSemanticReconstruction> reconstructions = {
            make_reconstruction("a", EndpointSemanticReconstructionStatus::Conflicted),
            make_reconstruction("b", EndpointSemanticReconstructionStatus::Unresolved)};
        const auto result = resolver.resolve(wires, endpoints, reconstructions, {}, {});
        const auto& r = find_resolution(result.resolutions, "w1");
        assert(r.start_component_status == WireSemanticStatus::Conflicted);
        assert(r.start_component_id.empty());
        assert(r.end_component_status == WireSemanticStatus::Unresolved);
    }

    // 8. Component association is not fabricated: a Resolved
    // reconstruction with an empty component_id must not be reported
    // Resolved (there is nothing to report).
    {
        std::vector<Wire> wires = {make_wire("w1", "a", "b")};
        std::vector<EndpointCandidate> endpoints = {make_endpoint("a"), make_endpoint("b")};
        std::vector<EndpointSemanticReconstruction> reconstructions = {
            make_reconstruction("a", EndpointSemanticReconstructionStatus::Resolved, ""),
        };
        const auto result = resolver.resolve(wires, endpoints, reconstructions, {}, {});
        const auto& r = find_resolution(result.resolutions, "w1");
        assert(r.start_component_status == WireSemanticStatus::Unresolved);
        assert(r.start_component_id.empty());
    }

    // A genuinely Resolved reconstruction with a real component id IS used.
    {
        std::vector<Wire> wires = {make_wire("w1", "a", "b")};
        std::vector<EndpointCandidate> endpoints = {make_endpoint("a"), make_endpoint("b")};
        std::vector<EndpointSemanticReconstruction> reconstructions = {
            make_reconstruction("a", EndpointSemanticReconstructionStatus::Resolved, "comp-1"),
        };
        const auto result = resolver.resolve(wires, endpoints, reconstructions, {}, {});
        const auto& r = find_resolution(result.resolutions, "w1");
        assert(r.start_component_status == WireSemanticStatus::Resolved);
        assert(r.start_component_id == "comp-1");
    }

    // 9. Electrical-net membership is not modified: the resolver only
    // reads ElectricalNet.endpoint_ids, never writes back into it.
    {
        std::vector<Wire> wires = {make_wire("w1", "a", "b")};
        std::vector<EndpointCandidate> endpoints = {make_endpoint("a"), make_endpoint("b")};
        ElectricalNet net;
        net.id = "net-1";
        net.endpoint_ids = {"a", "b"};
        const std::vector<ElectricalNet> nets_before = {net};
        std::vector<ElectricalNet> nets = nets_before;
        const auto result = resolver.resolve(wires, endpoints, {}, {}, nets);
        assert(nets.size() == nets_before.size());
        assert(nets[0].endpoint_ids == nets_before[0].endpoint_ids);
        const auto& r = find_resolution(result.resolutions, "w1");
        assert(r.electrical_net_status == WireSemanticStatus::Resolved);
        assert(r.electrical_net_id == "net-1");
        assert(r.electrical_net_confidence == ConfidenceClass::High);
    }

    // Electrical net: both endpoints resolve to different nets ->
    // Conflicted (a genuine cross-stage inconsistency, never hidden).
    {
        std::vector<Wire> wires = {make_wire("w1", "a", "b")};
        std::vector<EndpointCandidate> endpoints = {make_endpoint("a"), make_endpoint("b")};
        ElectricalNet net_a; net_a.id = "net-a"; net_a.endpoint_ids = {"a"};
        ElectricalNet net_b; net_b.id = "net-b"; net_b.endpoint_ids = {"b"};
        const auto result = resolver.resolve(wires, endpoints, {}, {}, {net_a, net_b});
        const auto& r = find_resolution(result.resolutions, "w1");
        assert(r.electrical_net_status == WireSemanticStatus::Conflicted);
        assert(r.electrical_net_id.empty());
    }

    // Electrical net: only one endpoint resolved -> Resolved but Medium
    // confidence (single-sided).
    {
        std::vector<Wire> wires = {make_wire("w1", "a", "b")};
        std::vector<EndpointCandidate> endpoints = {make_endpoint("a"), make_endpoint("b")};
        ElectricalNet net; net.id = "net-1"; net.endpoint_ids = {"a"};
        const auto result = resolver.resolve(wires, endpoints, {}, {}, {net});
        const auto& r = find_resolution(result.resolutions, "w1");
        assert(r.electrical_net_status == WireSemanticStatus::Resolved);
        assert(r.electrical_net_confidence == ConfidenceClass::Medium);
    }

    // 10/11/12. Wire/endpoint identity and topology are unchanged: the
    // resolver's inputs (wires, endpoints) are passed by const reference
    // and never mutated; verify byte-for-byte equality after the call.
    {
        std::vector<Wire> wires = {make_wire("w1", "a", "b")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("a", "Blue"), make_endpoint("b", "Blue")};
        const std::vector<Wire> wires_before = wires;
        const std::vector<EndpointCandidate> endpoints_before = endpoints;

        resolver.resolve(wires, endpoints, {}, {}, {});

        assert(wires.size() == wires_before.size());
        assert(wires[0].id == wires_before[0].id);
        assert(wires[0].start_endpoint == wires_before[0].start_endpoint);
        assert(wires[0].end_endpoint == wires_before[0].end_endpoint);
        assert(endpoints[0].id == endpoints_before[0].id);
        assert(endpoints[0].wire_color == endpoints_before[0].wire_color);
    }

    // 13. Deterministic output: identical input produces identical output,
    // including ordering, across repeated calls.
    {
        std::vector<Wire> wires = {
            make_wire("w-z", "a", "b"), make_wire("w-a", "c", "d")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("a", "Blue"), make_endpoint("b", "Blue"),
            make_endpoint("c"), make_endpoint("d")};

        const auto result_1 = resolver.resolve(wires, endpoints, {}, {}, {});
        const auto result_2 = resolver.resolve(wires, endpoints, {}, {}, {});

        assert(result_1.resolutions.size() == 2);
        // Sorted by wire_id: "w-a" before "w-z".
        assert(result_1.resolutions[0].wire_id == "w-a");
        assert(result_1.resolutions[1].wire_id == "w-z");

        assert(result_1.resolutions.size() == result_2.resolutions.size());
        for (std::size_t i = 0; i < result_1.resolutions.size(); ++i) {
            assert(result_1.resolutions[i].id == result_2.resolutions[i].id);
            assert(result_1.resolutions[i].wire_color == result_2.resolutions[i].wire_color);
            assert(result_1.resolutions[i].wire_color_status == result_2.resolutions[i].wire_color_status);
        }
    }

    // 14. Empty semantic evidence: an empty model produces an empty,
    // valid result with zeroed coverage - not a crash, not fabricated
    // resolutions.
    {
        const auto result = resolver.resolve({}, {}, {}, {}, {});
        assert(result.resolutions.empty());
        assert(result.coverage.total == 0);
        assert(result.coverage.wire_color_resolved == 0);
        assert(result.coverage.fully_unresolved == 0);
    }

    // 15. Shared conductor/wire cases remain valid: two distinct wires
    // sharing a splice-adjacent endpoint each get their own independent
    // resolution; one wire's color evidence must not leak into another's.
    {
        std::vector<Wire> wires = {
            make_wire("wire-ab", "a", "b"), make_wire("wire-ac", "a", "c")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("a", "Blue"), make_endpoint("b", "Blue"),
            make_endpoint("c", "Red")};
        const auto result = resolver.resolve(wires, endpoints, {}, {}, {});
        const auto& ab = find_resolution(result.resolutions, "wire-ab");
        const auto& ac = find_resolution(result.resolutions, "wire-ac");
        assert(ab.wire_color_status == WireSemanticStatus::Resolved);
        assert(ab.wire_color == "Blue");
        assert(ac.wire_color_status == WireSemanticStatus::Conflicted);
    }

    // 16. Coverage diagnostics distinguish Resolved/Unresolved/Conflicted
    // and correctly count fully_unresolved wires.
    {
        std::vector<Wire> wires = {
            make_wire("w1", "a", "b"), make_wire("w2", "c", "d")};
        std::vector<EndpointCandidate> endpoints = {
            make_endpoint("a", "Blue"), make_endpoint("b", "Blue"),
            make_endpoint("c"), make_endpoint("d")};
        const auto result = resolver.resolve(wires, endpoints, {}, {}, {});
        assert(result.coverage.total == 2);
        assert(result.coverage.wire_color_resolved == 1);
        assert(result.coverage.wire_color_unresolved == 1);
        assert(result.coverage.fully_unresolved == 1);
    }

    // Connector association: only a Resolved ConnectorTerminal is
    // authoritative; Unresolved/Conflicted ones are ignored.
    {
        std::vector<Wire> wires = {make_wire("w1", "a", "b")};
        std::vector<EndpointCandidate> endpoints = {make_endpoint("a"), make_endpoint("b")};
        ConnectorTerminal resolved_terminal;
        resolved_terminal.id = "ct-1";
        resolved_terminal.endpoint_id = "a";
        resolved_terminal.connector_id = "conn-1";
        resolved_terminal.status = ConnectorTerminalStatus::Resolved;
        ConnectorTerminal unresolved_terminal;
        unresolved_terminal.id = "ct-2";
        unresolved_terminal.endpoint_id = "b";
        unresolved_terminal.connector_id = "conn-2";
        unresolved_terminal.status = ConnectorTerminalStatus::Unresolved;

        const auto result = resolver.resolve(
            wires, endpoints, {}, {resolved_terminal, unresolved_terminal}, {});
        const auto& r = find_resolution(result.resolutions, "w1");
        assert(r.start_connector_status == WireSemanticStatus::Resolved);
        assert(r.start_connector_id == "conn-1");
        assert(r.end_connector_status == WireSemanticStatus::Unresolved);
        assert(r.end_connector_id.empty());
    }

    return 0;
}

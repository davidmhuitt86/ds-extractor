#include "eke_dx_wire/topology/wire_identity_key.hpp"

#include <cassert>

using namespace eke::dx::wire;

namespace {

Wire make_wire(
    const std::string& start,
    const std::string& end,
    std::vector<std::string> topology_edges,
    std::vector<std::string> conductor_segments) {
    Wire wire;
    wire.start_endpoint = start;
    wire.end_endpoint = end;
    wire.topology_edges = std::move(topology_edges);
    wire.conductor_segments = std::move(conductor_segments);
    return wire;
}

} // namespace

int main() {
    // AP-DIAG-AUDIT-003's exact reproduced pair 1
    // (wire-9da1927f1f6dbf98 / wire-fd53d84a92e53bb4): identical
    // topology_edges/conductor_segments, endpoints reversed. Must
    // canonicalize to the same key.
    {
        const Wire forward = make_wire(
            "endpoint-candidate-9d7e7957c56e3777",
            "endpoint-candidate-87250d4701edd231",
            {"topology-edge-7b4bc0d5b22a8513"},
            {"normalized-conductor-segment-0787ea8a78f08cba"});
        const Wire reversed = make_wire(
            "endpoint-candidate-87250d4701edd231",
            "endpoint-candidate-9d7e7957c56e3777",
            {"topology-edge-7b4bc0d5b22a8513"},
            {"normalized-conductor-segment-0787ea8a78f08cba"});

        assert(canonical_wire_identity_key(forward) ==
               canonical_wire_identity_key(reversed));
    }

    // AP-DIAG-AUDIT-003's exact reproduced pair 2
    // (wire-49477c4a4828ab6e / wire-7fa7c11e9eed226a): same shape.
    {
        const Wire forward = make_wire(
            "endpoint-candidate-6193f59e31ae96ec",
            "endpoint-candidate-bfae64deea64562f",
            {"topology-edge-7dccad922e6f2958"},
            {"normalized-conductor-segment-6ec8c451d0ef2961"});
        const Wire reversed = make_wire(
            "endpoint-candidate-bfae64deea64562f",
            "endpoint-candidate-6193f59e31ae96ec",
            {"topology-edge-7dccad922e6f2958"},
            {"normalized-conductor-segment-6ec8c451d0ef2961"});

        assert(canonical_wire_identity_key(forward) ==
               canonical_wire_identity_key(reversed));
    }

    // A multi-edge path reversed end-to-end (traversal direction reverses
    // path order too) must still canonicalize identically - the key
    // sorts topology_edges/conductor_segments rather than trusting
    // sequence order.
    {
        const Wire forward = make_wire(
            "endpoint-A", "endpoint-B",
            {"edge-1", "edge-2", "edge-3"},
            {"segment-1", "segment-2"});
        const Wire reversed = make_wire(
            "endpoint-B", "endpoint-A",
            {"edge-3", "edge-2", "edge-1"},
            {"segment-2", "segment-1"});

        assert(canonical_wire_identity_key(forward) ==
               canonical_wire_identity_key(reversed));
    }

    // Legitimate shared-conductor Wires (AP-WIRE-029: one net != one
    // wire, one wire != one net) - two DISTINCT endpoint pairs that
    // happen to reference the same conductor segment must NOT collapse
    // to the same key.
    {
        const Wire wire_a = make_wire(
            "endpoint-1", "endpoint-2",
            {"edge-shared"}, {"segment-shared"});
        const Wire wire_b = make_wire(
            "endpoint-3", "endpoint-4",
            {"edge-shared"}, {"segment-shared"});

        assert(canonical_wire_identity_key(wire_a) !=
               canonical_wire_identity_key(wire_b));
    }

    // Different endpoint pair entirely (no shared geometry at all) must
    // not collapse.
    {
        const Wire wire_a = make_wire(
            "endpoint-1", "endpoint-2", {"edge-a"}, {"segment-a"});
        const Wire wire_b = make_wire(
            "endpoint-5", "endpoint-6", {"edge-b"}, {"segment-b"});

        assert(canonical_wire_identity_key(wire_a) !=
               canonical_wire_identity_key(wire_b));
    }

    // Same endpoint pair, but a genuinely DIFFERENT conductor path
    // (distinct topology_edges/conductor_segments) - e.g. two
    // independently-discovered candidate routes between the same two
    // endpoints - must remain distinct; only the demonstrated exact
    // duplicate (same path, reversed order) collapses.
    {
        const Wire wire_a = make_wire(
            "endpoint-1", "endpoint-2",
            {"edge-route-1"}, {"segment-route-1"});
        const Wire wire_b = make_wire(
            "endpoint-1", "endpoint-2",
            {"edge-route-2"}, {"segment-route-2"});

        assert(canonical_wire_identity_key(wire_a) !=
               canonical_wire_identity_key(wire_b));
    }

    // Same endpoint pair and same topology_edges, but different
    // conductor_segments (e.g. a heavy_cable segment vs. its normalized
    // counterpart referencing a different segment id) must also remain
    // distinct - the key must not ignore conductor-segment identity.
    {
        const Wire wire_a = make_wire(
            "endpoint-1", "endpoint-2",
            {"edge-a"}, {"segment-a"});
        const Wire wire_b = make_wire(
            "endpoint-1", "endpoint-2",
            {"edge-a"}, {"segment-different"});

        assert(canonical_wire_identity_key(wire_a) !=
               canonical_wire_identity_key(wire_b));
    }

    return 0;
}

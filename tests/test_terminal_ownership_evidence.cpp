// AP-DIAG-FIX-005: test-first reproduction of the AP-DIAG-AUDIT-004
// terminal-attribution defect, and the positive/negative controls proving
// the fix distinguishes real ownership evidence from proximity-only
// attribution.
//
// The geometry below is not invented - it reproduces the exact real TRX300
// coordinates AP-DIAG-AUDIT-004 traced: a 7x7px component
// (component-candidate-shape-region-c2335cc575d107b1 in the real
// extraction) that owns zero SymbolPrimitives, confirmed by direct visual
// inspection of samples/trx300ODG.png to be the pointer/dot glyph of the
// "SUB FUSE 15A" annotation leader line - not an engineering symbol.
//
//   - TEST-001 reproduces wire-ded6cca62fbe3cd9: BOTH its endpoints
//     (endpoint-candidate-7b1232ec85583772 at (880.5,416), attributed via
//     TerminalRecognizer's boundary-alignment fallback, and
//     endpoint-candidate-8b9c74742885946f at (880.5,442), attributed via
//     TerminalLocationDetector's bounding-box proximity) must no longer
//     resolve to a ComponentTerminal candidate for this component.
//   - TEST-002 reproduces wire-e49784ea9fbccace's single false endpoint
//     (endpoint-candidate-e9392250710fc19a at (885.5,435), also via
//     TerminalLocationDetector) - same spurious component, a third,
//     independent topology node.
//   - Positive controls confirm a component that legitimately owns a
//     SymbolPrimitive (of any kind, not only TerminalLead) still resolves
//     through both producers exactly as before.

#include "eke_dx_wire/topology/terminal_location_detector.hpp"
#include "eke_dx_wire/topology/terminal_recognizer.hpp"

#include <cassert>
#include <iostream>

using namespace eke::dx::wire;

namespace {

ComponentCandidate make_component(
    const std::string& id,
    ComponentCandidateKind kind,
    BoundingBox bounds) {
    ComponentCandidate value;
    value.id = id;
    value.kind = kind;
    value.bounds = bounds;
    return value;
}

EndpointCandidate make_endpoint(
    const std::string& id,
    const std::string& node_id,
    Point2D position,
    std::vector<std::string> incident_edges = {}) {
    EndpointCandidate value;
    value.id = id;
    value.node_id = node_id;
    value.position = position;
    value.incident_edges = std::move(incident_edges);
    return value;
}

} // namespace

int main() {
    // The real spurious component: 7x7px, circular_symbol, zero owned
    // SymbolPrimitives, zero associated RejectedGeometryEvidence - exactly
    // component-candidate-shape-region-c2335cc575d107b1.
    const ComponentCandidate spurious = make_component(
        "component-candidate-shape-region-c2335cc575d107b1",
        ComponentCandidateKind::CircularSymbol,
        BoundingBox{874, 430, 7, 7});

    // --- TEST-001: wire-ded6cca62fbe3cd9 - endpoint A (TerminalRecognizer
    // boundary-alignment fallback path) -----------------------------------
    {
        auto endpoint_a = make_endpoint(
            "endpoint-candidate-7b1232ec85583772",
            "topology-node-c19bc053fae73262",
            Point2D{880.5, 416},
            {"topology-edge-9760e3ea7c75d9c9"});
        auto endpoint_b_node = make_endpoint(
            "endpoint-candidate-8b9c74742885946f",
            "topology-node-c1acd653faf5caf7",
            Point2D{880.5, 442});

        TopologyNode node_a{
            "topology-node-c19bc053fae73262", {880.5, 416},
            TopologyNodeType::ConductorEnd, true};
        TopologyNode node_b{
            "topology-node-c1acd653faf5caf7", {880.5, 442},
            TopologyNodeType::ConductorEnd, true};
        TopologyEdge edge{
            "topology-edge-9760e3ea7c75d9c9",
            "topology-node-c19bc053fae73262",
            "topology-node-c1acd653faf5caf7",
            "normalized-conductor-segment-01eda2b703cc25ab"};

        TerminalRecognizer recognizer;
        const auto result = recognizer.recognize(
            {spurious}, {}, /*primitives=*/{}, {endpoint_a},
            {node_a, node_b}, {edge}, /*existing_candidates=*/{});

        // Before AP-DIAG-FIX-005: the conductor from endpoint A points
        // directly at the component's boundary (cosine 1.0, well past
        // minimum_alignment_cosine), and the component's zero owned
        // SymbolPrimitives were never checked - a false ComponentTerminal
        // candidate was produced. After the fix: zero ownership evidence
        // means the alignment fallback must not fire at all.
        assert(result.candidates.empty());
    }

    // --- TEST-001: wire-ded6cca62fbe3cd9 - endpoint B (TerminalLocationDetector
    // bounding-box proximity path) -----------------------------------------
    {
        auto endpoint_b = make_endpoint(
            "endpoint-candidate-8b9c74742885946f",
            "topology-node-c1acd653faf5caf7",
            Point2D{880.5, 442});

        TerminalLocationDetector detector;
        const auto result = detector.detect(
            {spurious}, {endpoint_b}, /*rejected_geometry=*/{},
            /*symbol_primitives=*/{});

        // Before the fix: distance 5px is within boundary_tolerance (8px),
        // producing a medium-confidence ComponentTerminal candidate with no
        // ownership check at all. After the fix: the component owns no
        // SymbolPrimitive and has no associated RejectedGeometryEvidence,
        // so it must be excluded from consideration entirely.
        assert(result.candidates.empty());
    }

    // --- TEST-002: wire-e49784ea9fbccace - the third, independent
    // topology node attributing to the same spurious component -----------
    {
        auto endpoint_third = make_endpoint(
            "endpoint-candidate-e9392250710fc19a",
            "topology-node-cf10dd596d2bfc16",
            Point2D{885.5, 435});

        TerminalLocationDetector detector;
        const auto result = detector.detect(
            {spurious}, {endpoint_third}, {}, {});

        // Before the fix: distance 4.5px, well within boundary_tolerance,
        // produced a medium-confidence false ComponentTerminal candidate -
        // proving the defect is not isolated to wire-ded6cca62fbe3cd9.
        // After the fix: same ownership gate, same result - no candidate.
        assert(result.candidates.empty());
    }

    // --- Positive control: TerminalLocationDetector still attributes a
    // component that legitimately owns a SymbolPrimitive -------------------
    {
        const ComponentCandidate legitimate = make_component(
            "component-legitimate", ComponentCandidateKind::CircularSymbol,
            BoundingBox{100, 100, 20, 20});
        SymbolPrimitive owned_primitive;
        owned_primitive.id = "primitive-legitimate";
        owned_primitive.component_id = "component-legitimate";
        // Deliberately NOT TerminalLead - AP-DIAG-FIX-005 requires only
        // that the component owns *some* real symbol geometry, mirroring
        // the real TRX300 case (component-candidate-shape-region-
        // bfae05a427189376) where 6 legitimate ComponentTerminal endpoints
        // are backed by Unknown-kind primitives, never TerminalLead.
        owned_primitive.kind = SymbolPrimitiveKind::Unknown;
        owned_primitive.bounds = BoundingBox{100, 100, 20, 20};

        auto ep = make_endpoint("endpoint-legit", "node-legit", {100, 115});

        TerminalLocationDetector detector;
        const auto result = detector.detect(
            {legitimate}, {ep}, {}, {owned_primitive});

        assert(result.candidates.size() == 1);
        assert(result.candidates.front().endpoint_id == "endpoint-legit");
        assert(result.candidates.front().component_candidate_id ==
               "component-legitimate");
        assert(result.candidates.front().kind ==
               TerminalCandidateKind::ComponentBoundary);
    }

    // --- Positive control: TerminalRecognizer's boundary-alignment
    // fallback still attributes a component that owns a non-TerminalLead
    // SymbolPrimitive, exactly like the real bfae05a427189376 case --------
    {
        const ComponentCandidate legitimate = make_component(
            "component-legitimate-2", ComponentCandidateKind::CircularSymbol,
            BoundingBox{100, 100, 20, 20});
        SymbolPrimitive owned_primitive;
        owned_primitive.id = "primitive-legitimate-2";
        owned_primitive.component_id = "component-legitimate-2";
        owned_primitive.kind = SymbolPrimitiveKind::Unknown;
        owned_primitive.bounds = BoundingBox{100, 100, 20, 20};

        auto ep = make_endpoint("ep2", "n1", {90, 110}, {"e1"});
        TopologyNode n1{"n1", {90, 110}, TopologyNodeType::ConductorEnd, true};
        TopologyNode n2{"n2", {95, 110}, TopologyNodeType::Continuation, true};
        TopologyEdge e{"e1", "n1", "n2", "seg"};

        TerminalRecognizer recognizer;
        const auto result = recognizer.recognize(
            {legitimate}, {}, {owned_primitive}, {ep}, {n1, n2}, {e}, {});

        assert(result.candidates.size() == 1);
        assert(result.candidates[0].endpoint_id == "ep2");
        assert(result.candidates[0].component_candidate_id ==
               "component-legitimate-2");
    }

    // --- Positive control: a genuine ChassisGround (which, per the real
    // model, always owns at least one owned SymbolPrimitive) is unaffected.
    {
        const ComponentCandidate ground = make_component(
            "ground-legit", ComponentCandidateKind::ChassisGround,
            BoundingBox{200, 100, 20, 20});
        SymbolPrimitive lead;
        lead.id = "lead-legit";
        lead.component_id = "ground-legit";
        lead.kind = SymbolPrimitiveKind::TerminalLead;
        lead.bounds = BoundingBox{205, 90, 8, 12};

        auto ep = make_endpoint("ground-endpoint", "n-ground", {210, 100});

        TerminalLocationDetector detector;
        const auto result = detector.detect(
            {ground}, {ep}, {}, {lead});

        assert(result.candidates.size() == 1);
        assert(result.candidates.front().kind ==
               TerminalCandidateKind::GroundConnection);
    }

    // --- The ambiguous case: endpoint-candidate-87250d4701edd231 /
    // wire-fd53d84a92e53bb4 - a zero-primitive component near a wire that
    // otherwise participates in a resolved net. Ownership evidence is
    // insufficient, so no ComponentTerminal is manufactured - the endpoint
    // must fall back to unresolved/geometric rather than being guessed
    // either way. -----------------------------------------------------
    {
        const ComponentCandidate ambiguous = make_component(
            "component-candidate-shape-region-d1aefcaca5503ad9",
            ComponentCandidateKind::CircularSymbol,
            BoundingBox{629, 543, 8, 10});
        auto ep = make_endpoint(
            "endpoint-candidate-87250d4701edd231", "node-ambiguous",
            Point2D{629.5, 556});

        TerminalLocationDetector detector;
        const auto result = detector.detect(
            {ambiguous}, {ep}, {}, {});

        assert(result.candidates.empty());
    }

    std::cout << "terminal ownership evidence tests passed\n";
    return 0;
}

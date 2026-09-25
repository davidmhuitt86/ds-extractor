#include "eke_dx_wire/topology/terminal_recognizer.hpp"

#include <cassert>
#include <iostream>
#include <utility>

using namespace eke::dx::wire;

namespace {

ComponentCandidate component(
    const std::string& id,
    ComponentCandidateKind kind,
    BoundingBox bounds) {
    ComponentCandidate value;
    value.id = id;
    value.kind = kind;
    value.bounds = bounds;
    return value;
}

EndpointCandidate endpoint(
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
    TerminalRecognizer recognizer;

    // AP-WIRE-023 TerminalLead is evidence for an existing endpoint only.
    {
        std::vector<ComponentCandidate> components = {
            component("ground-1", ComponentCandidateKind::ChassisGround,
                      {100, 100, 20, 20})};

        std::vector<ComponentSymbolGeometry> geometries = {
            {"geometry-ground-1", "ground-1", {"lead-1"}, ConfidenceClass::Low}};

        std::vector<SymbolPrimitive> primitives = {
            {"lead-1", "ground-1", SymbolPrimitiveKind::TerminalLead,
             {100, 108, 8, 2}, 16.0, ConfidenceClass::Medium,
             {"fixture", 0, {100, 108, 8, 2}, "test"}}};

        auto ep = endpoint(
            "endpoint-1", "node-1", {108, 109}, {"edge-1"});

        const auto result = recognizer.recognize(
            components, geometries, primitives, {ep}, {}, {}, {});

        assert(result.candidates.size() == 1);
        assert(result.candidates[0].endpoint_id == "endpoint-1");
        assert(result.candidates[0].component_candidate_id == "ground-1");
        assert(result.candidates[0].kind == TerminalCandidateKind::GroundConnection);
        assert(result.candidates[0].confidence == ConfidenceClass::Medium ||
               result.candidates[0].confidence == ConfidenceClass::Low);
    }

    // Two equidistant endpoints are ambiguous; no winner is invented.
    {
        std::vector<ComponentCandidate> components = {
            component("component-1", ComponentCandidateKind::CircularSymbol,
                      {100, 100, 20, 20})};
        std::vector<SymbolPrimitive> primitives = {
            {"lead-1", "component-1", SymbolPrimitiveKind::TerminalLead,
             {100, 108, 8, 2}, 16.0, ConfidenceClass::High,
             {"fixture", 0, {100, 108, 8, 2}, "test"}}};
        // Primitive bounds are (100,108,8,2), i.e. x in [100,108]. Placing
        // one endpoint 2px outside each side gives two endpoints at the
        // exact same distance (2px) from the primitive box - a genuine
        // tie, not merely two different distances.
        auto a = endpoint("a", "node-a", {98, 109});
        auto b = endpoint("b", "node-b", {110, 109});
        const auto result = recognizer.recognize(
            components, {}, primitives, {a, b}, {}, {}, {});
        assert(result.candidates.empty());
    }

    // Boundary-alignment fallback recognizes an existing endpoint only when
    // its conductor points toward the component AND the component owns at
    // least one SymbolPrimitive (AP-DIAG-FIX-005: boundary distance plus
    // conductor alignment alone is not sufficient ownership evidence - a
    // component with zero owned primitives at all must never reach this
    // fallback, however well its geometry happens to align).
    {
        std::vector<ComponentCandidate> components = {
            component("component-2", ComponentCandidateKind::CircularSymbol,
                      {100, 100, 20, 20})};
        std::vector<SymbolPrimitive> primitives = {
            {"primitive-2", "component-2", SymbolPrimitiveKind::Unknown,
             {100, 100, 20, 20}, 400.0, ConfidenceClass::Medium,
             {"fixture", 0, {100, 100, 20, 20}, "test"}}};
        auto ep = endpoint("ep", "n1", {90, 110}, {"e1"});
        TopologyNode n1{"n1", {90, 110}, TopologyNodeType::ConductorEnd, true};
        TopologyNode n2{"n2", {95, 110}, TopologyNodeType::Continuation, true};
        TopologyEdge e{"e1", "n1", "n2", "seg"};
        const auto result = recognizer.recognize(
            components, {}, primitives, {ep}, {n1, n2}, {e}, {});
        assert(result.candidates.size() == 1);
        assert(result.candidates[0].endpoint_id == "ep");
        assert(result.candidates[0].component_candidate_id == "component-2");
        assert(result.candidates[0].confidence == ConfidenceClass::Low);
    }

    // A conductor pointing away from the component is not terminal evidence
    // - this remains true even for a component that does own a primitive,
    // isolating the alignment check from the ownership check.
    {
        auto ep = endpoint("ep", "n1", {90, 110}, {"e1"});
        TopologyNode n1{"n1", {90, 110}, TopologyNodeType::ConductorEnd, true};
        TopologyNode n2{"n2", {85, 110}, TopologyNodeType::Continuation, true};
        TopologyEdge e{"e1", "n1", "n2", "seg"};
        std::vector<SymbolPrimitive> primitives = {
            {"primitive-3", "component-3", SymbolPrimitiveKind::Unknown,
             {100, 100, 20, 20}, 400.0, ConfidenceClass::Medium,
             {"fixture", 0, {100, 100, 20, 20}, "test"}}};
        const auto result = recognizer.recognize(
            {component("component-3", ComponentCandidateKind::CircularSymbol,
                       {100, 100, 20, 20})},
            {}, primitives, {ep}, {n1, n2}, {e}, {});
        assert(result.candidates.empty());
    }

    // AP-DIAG-FIX-005: the exact defect AP-DIAG-AUDIT-004 found - a
    // component with ZERO owned SymbolPrimitives must never reach the
    // boundary-alignment fallback, even when its conductor alignment is
    // perfect. Same geometry as the first boundary-alignment case above,
    // with the owned primitive removed.
    {
        std::vector<ComponentCandidate> components = {
            component("component-zero-evidence",
                      ComponentCandidateKind::CircularSymbol,
                      {100, 100, 20, 20})};
        auto ep = endpoint("ep", "n1", {90, 110}, {"e1"});
        TopologyNode n1{"n1", {90, 110}, TopologyNodeType::ConductorEnd, true};
        TopologyNode n2{"n2", {95, 110}, TopologyNodeType::Continuation, true};
        TopologyEdge e{"e1", "n1", "n2", "seg"};
        const auto result = recognizer.recognize(
            components, {}, /*primitives=*/{}, {ep}, {n1, n2}, {e}, {});
        assert(result.candidates.empty());
    }

    // Existing association wins; the recognizer must not duplicate it.
    {
        auto ep = endpoint("ep", "n1", {100, 110});
        TerminalCandidate existing;
        existing.id = "terminal-candidate-existing";
        existing.endpoint_id = "ep";
        existing.component_candidate_id = "component-4";

        const auto result = recognizer.recognize(
            {component("component-4", ComponentCandidateKind::Enclosure,
                       {100, 100, 20, 20})},
            {}, {}, {ep}, {}, {}, {existing});
        assert(result.candidates.empty());
    }

    // Diagram furniture is never a terminal-recognition source.
    {
        const auto result = recognizer.recognize(
            {component("furniture", ComponentCandidateKind::DiagramFurniture,
                       {100, 100, 20, 20})},
            {}, {}, {endpoint("ep", "n1", {105, 105})}, {}, {}, {});
        assert(result.candidates.empty());
    }

    // AP-DIAG-FIX-002: the component-kind -> TerminalCandidateKind mapping
    // this stage relies on to eventually distinguish a Connector terminal
    // from a generic component terminal or a ground terminal. Each of these
    // reuses the boundary-alignment fallback path exercised above, varying
    // only the owning component's kind.
    const auto boundary_alignment_kind =
        [&](ComponentCandidateKind component_kind) {
            auto ep = endpoint("ep", "n1", {90, 110}, {"e1"});
            TopologyNode n1{"n1", {90, 110}, TopologyNodeType::ConductorEnd, true};
            TopologyNode n2{"n2", {95, 110}, TopologyNodeType::Continuation, true};
            TopologyEdge e{"e1", "n1", "n2", "seg"};
            // AP-DIAG-FIX-005: the boundary-alignment fallback now requires
            // ownership evidence, so this shared probe must supply a
            // (non-TerminalLead) owned primitive to keep testing what it
            // was designed to test - the kind mapping/alignment math -
            // rather than incidentally re-testing the ownership gate.
            std::vector<SymbolPrimitive> primitives = {
                {"primitive-kind-probe", "component-kind-probe",
                 SymbolPrimitiveKind::Unknown, {100, 100, 20, 20}, 400.0,
                 ConfidenceClass::Medium,
                 {"fixture", 0, {100, 100, 20, 20}, "test"}}};
            return recognizer.recognize(
                {component("component-kind-probe", component_kind,
                           {100, 100, 20, 20})},
                {}, primitives, {ep}, {n1, n2}, {e}, {});
        };

    // TEST 5: a generic Enclosure/CircularSymbol component's terminal
    // evidence remains a ComponentBoundary/ComponentTerminal - it is never
    // reclassified as a connector merely because it is a plausible,
    // recognized terminal.
    {
        const auto result = boundary_alignment_kind(
            ComponentCandidateKind::Enclosure);
        assert(result.candidates.size() == 1);
        assert(result.candidates[0].kind ==
               TerminalCandidateKind::ComponentBoundary);
    }

    // A PrimitiveSymbol-kind component is the only existing, evidence-based
    // route to connector terminal evidence in this pipeline (AP-WIRE-024's
    // own terminal_kind() mapping) - confirmed here directly rather than
    // assumed, since no existing test exercised this specific mapping.
    {
        const auto result = boundary_alignment_kind(
            ComponentCandidateKind::PrimitiveSymbol);
        assert(result.candidates.size() == 1);
        assert(result.candidates[0].kind ==
               TerminalCandidateKind::ConnectorBoundary);
    }

    // TEST 6: ChassisGround remains distinct from a connector - a ground
    // symbol's terminal evidence is GroundConnection, never
    // ConnectorBoundary, however similar its geometry might otherwise be.
    {
        const auto result = boundary_alignment_kind(
            ComponentCandidateKind::ChassisGround);
        assert(result.candidates.size() == 1);
        assert(result.candidates[0].kind ==
               TerminalCandidateKind::GroundConnection);
    }

    // TEST 7: insufficient/absent component evidence (Unknown) never
    // produces a guessed Connector (or any other) classification - the
    // candidate is dropped entirely rather than invented.
    {
        const auto result = boundary_alignment_kind(
            ComponentCandidateKind::Unknown);
        assert(result.candidates.empty());
    }

    std::cout << "terminal recognizer tests passed\n";
    return 0;
}

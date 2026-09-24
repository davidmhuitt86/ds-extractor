#include "eke_dx_wire/topology/terminal_recognizer.hpp"

#include <cassert>
#include <iostream>

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
        auto a = endpoint("a", "node-a", {108, 109});
        auto b = endpoint("b", "node-b", {108, 107});
        const auto result = recognizer.recognize(
            components, {}, primitives, {a, b}, {}, {}, {});
        assert(result.candidates.empty());
    }

    // Boundary-alignment fallback recognizes an existing endpoint only when
    // its conductor points toward the component.
    {
        std::vector<ComponentCandidate> components = {
            component("component-2", ComponentCandidateKind::CircularSymbol,
                      {100, 100, 20, 20})};
        auto ep = endpoint("ep", "n1", {90, 110}, {"e1"});
        TopologyNode n1{"n1", {90, 110}, TopologyNodeType::ConductorEnd, true};
        TopologyNode n2{"n2", {95, 110}, TopologyNodeType::Continuation, true};
        TopologyEdge e{"e1", "n1", "n2", "seg"};
        const auto result = recognizer.recognize(
            components, {}, {}, {ep}, {n1, n2}, {e}, {});
        assert(result.candidates.size() == 1);
        assert(result.candidates[0].endpoint_id == "ep");
        assert(result.candidates[0].component_candidate_id == "component-2");
        assert(result.candidates[0].confidence == ConfidenceClass::Low);
    }

    // A conductor pointing away from the component is not terminal evidence.
    {
        auto ep = endpoint("ep", "n1", {90, 110}, {"e1"});
        TopologyNode n1{"n1", {90, 110}, TopologyNodeType::ConductorEnd, true};
        TopologyNode n2{"n2", {85, 110}, TopologyNodeType::Continuation, true};
        TopologyEdge e{"e1", "n1", "n2", "seg"};
        const auto result = recognizer.recognize(
            {component("component-3", ComponentCandidateKind::CircularSymbol,
                       {100, 100, 20, 20})},
            {}, {}, {ep}, {n1, n2}, {e}, {});
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

    std::cout << "terminal recognizer tests passed\n";
    return 0;
}

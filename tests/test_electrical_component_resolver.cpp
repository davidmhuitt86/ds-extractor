// AP-DIAG-FIX-008: focused unit coverage for the ComponentCandidate ->
// ElectricalComponent semantic boundary. See
// docs/AP-DIAG-FIX-008_Electrical_Component_Semantic_Boundary.md for the
// governing rule set this exercises.

#include "eke_dx_wire/topology/electrical_component_resolver.hpp"

#include <cassert>
#include <iostream>

using namespace eke::dx::wire;

namespace {

ComponentCandidate make_component(
    const std::string& id, ComponentCandidateKind kind) {
    ComponentCandidate c;
    c.id = id;
    c.kind = kind;
    c.confidence = ConfidenceClass::High;
    return c;
}

SymbolFamilyResolution make_family_resolution(
    const std::string& component_id,
    SymbolFamily family,
    SymbolFamilyResolutionStatus status,
    ConfidenceClass confidence = ConfidenceClass::High) {
    SymbolFamilyResolution r;
    r.id = "symbol-family-resolution-" + component_id;
    r.component_id = component_id;
    r.family = family;
    r.status = status;
    r.confidence = confidence;
    return r;
}

EndpointCandidate make_component_terminal(
    const std::string& id, const std::string& component_id) {
    EndpointCandidate e;
    e.id = id;
    e.kind = EndpointKind::ComponentTerminal;
    e.component_id = component_id;
    e.confidence = ConfidenceClass::High;
    return e;
}

const ElectricalComponent& find(
    const std::vector<ElectricalComponent>& components,
    const std::string& component_candidate_id) {
    for (const auto& c : components) {
        if (c.component_candidate_id == component_candidate_id) return c;
    }
    static ElectricalComponent empty;
    return empty;
}

} // namespace

int main() {
    ElectricalComponentResolver resolver;

    // TEST 1: a geometric candidate does not automatically become an
    // ElectricalComponent. A bare CircularSymbol with no family evidence
    // and no terminal must remain Unresolved, never Resolved.
    {
        const auto components = std::vector<ComponentCandidate>{
            make_component("comp-1", ComponentCandidateKind::CircularSymbol)};
        const auto result = resolver.resolve(components, {}, {}, {});
        const auto& electrical = find(result.electrical_components, "comp-1");
        assert(electrical.status == ElectricalComponentResolutionStatus::Unresolved);
        assert(electrical.family == SymbolFamily::Unknown);
    }

    // TEST 2: a ComponentCandidate can remain unresolved even with a
    // terminal, if no electrical-function evidence exists - terminal
    // presence alone must never promote it.
    {
        const auto components = std::vector<ComponentCandidate>{
            make_component("comp-2", ComponentCandidateKind::CircularSymbol)};
        const auto endpoints = std::vector<EndpointCandidate>{
            make_component_terminal("ep-2", "comp-2")};
        const auto result = resolver.resolve(components, {}, {}, endpoints);
        const auto& electrical = find(result.electrical_components, "comp-2");
        assert(electrical.status == ElectricalComponentResolutionStatus::Unresolved);
        assert(!electrical.terminal_endpoint_ids.empty());
    }

    // TEST 3: an ElectricalComponent requires sufficient electrical
    // identity evidence - a Resolved, non-Ground, non-Unknown
    // SymbolFamilyResolution plus at least one terminal. With only the
    // family evidence and no terminal, it must stay Unresolved.
    {
        const auto components = std::vector<ComponentCandidate>{
            make_component("comp-3", ComponentCandidateKind::Enclosure)};
        const auto families = std::vector<SymbolFamilyResolution>{
            make_family_resolution(
                "comp-3", SymbolFamily::Relay, SymbolFamilyResolutionStatus::Resolved)};
        const auto result = resolver.resolve(components, families, {}, {});
        const auto& electrical = find(result.electrical_components, "comp-3");
        assert(electrical.status == ElectricalComponentResolutionStatus::Unresolved);
        // The family evidence is preserved for explainability even though
        // it did not, by itself, promote the candidate.
        assert(electrical.family == SymbolFamily::Relay);
        assert(!electrical.evidence_ids.empty());
    }

    // With both family evidence AND a terminal, comp-3 becomes Resolved -
    // establishes the positive case TEST 3's negative case is contrasted
    // against.
    {
        const auto components = std::vector<ComponentCandidate>{
            make_component("comp-3b", ComponentCandidateKind::Enclosure)};
        const auto families = std::vector<SymbolFamilyResolution>{
            make_family_resolution(
                "comp-3b", SymbolFamily::Relay, SymbolFamilyResolutionStatus::Resolved)};
        const auto endpoints = std::vector<EndpointCandidate>{
            make_component_terminal("ep-3b", "comp-3b")};
        const auto result = resolver.resolve(components, families, {}, endpoints);
        const auto& electrical = find(result.electrical_components, "comp-3b");
        assert(electrical.status == ElectricalComponentResolutionStatus::Resolved);
        assert(electrical.family == SymbolFamily::Relay);
    }

    // TEST 4: a connector is not automatically promoted to
    // ElectricalComponent solely because it has terminals - even with
    // qualifying family evidence and a terminal, ConnectorCandidate
    // ownership rejects it outright (Definition E).
    {
        const auto components = std::vector<ComponentCandidate>{
            make_component("comp-4", ComponentCandidateKind::Enclosure)};
        const auto families = std::vector<SymbolFamilyResolution>{
            make_family_resolution(
                "comp-4", SymbolFamily::Relay, SymbolFamilyResolutionStatus::Resolved)};
        const auto endpoints = std::vector<EndpointCandidate>{
            make_component_terminal("ep-4", "comp-4")};
        ConnectorCandidate connector;
        connector.id = "connector-4";
        connector.component_candidate_id = "comp-4";
        const auto result = resolver.resolve(
            components, families, {connector}, endpoints);
        const auto& electrical = find(result.electrical_components, "comp-4");
        assert(electrical.status == ElectricalComponentResolutionStatus::Rejected);
        assert(electrical.rejection_reason ==
               ElectricalComponentRejectionReason::ConnectorInterface);
    }

    // TEST 5: a chassis-ground reference is not automatically classified
    // as an ordinary component, even when it has a terminal and a
    // Resolved Ground family evidence entry (exactly the real-world shape
    // of every genuine ChassisGround component in this pipeline).
    {
        const auto components = std::vector<ComponentCandidate>{
            make_component("comp-5", ComponentCandidateKind::ChassisGround)};
        const auto families = std::vector<SymbolFamilyResolution>{
            make_family_resolution(
                "comp-5", SymbolFamily::Ground, SymbolFamilyResolutionStatus::Resolved)};
        const auto endpoints = std::vector<EndpointCandidate>{
            make_component_terminal("ep-5", "comp-5")};
        const auto result = resolver.resolve(components, families, {}, endpoints);
        const auto& electrical = find(result.electrical_components, "comp-5");
        assert(electrical.status == ElectricalComponentResolutionStatus::Rejected);
        assert(electrical.rejection_reason ==
               ElectricalComponentRejectionReason::ChassisGroundReference);
    }

    // Ground family evidence on a NON-ChassisGround-shaped candidate must
    // also never promote it (Ground is categorically excluded from
    // ElectricalComponent identity, not merely gated by shape kind).
    {
        const auto components = std::vector<ComponentCandidate>{
            make_component("comp-5b", ComponentCandidateKind::CircularSymbol)};
        const auto families = std::vector<SymbolFamilyResolution>{
            make_family_resolution(
                "comp-5b", SymbolFamily::Ground, SymbolFamilyResolutionStatus::Resolved)};
        const auto endpoints = std::vector<EndpointCandidate>{
            make_component_terminal("ep-5b", "comp-5b")};
        const auto result = resolver.resolve(components, families, {}, endpoints);
        const auto& electrical = find(result.electrical_components, "comp-5b");
        assert(electrical.status != ElectricalComponentResolutionStatus::Resolved);
    }

    // TEST 6: a fuse is valid as an ElectricalComponent because it has
    // electrical terminals and an electrical protection function
    // (SymbolFamily::Fuse, added by this AP specifically because the
    // TRX300 ground-truth inventory includes fuses).
    {
        const auto components = std::vector<ComponentCandidate>{
            make_component("comp-6", ComponentCandidateKind::PrimitiveSymbol)};
        const auto families = std::vector<SymbolFamilyResolution>{
            make_family_resolution(
                "comp-6", SymbolFamily::Fuse, SymbolFamilyResolutionStatus::Resolved)};
        const auto endpoints = std::vector<EndpointCandidate>{
            make_component_terminal("ep-6a", "comp-6"),
            make_component_terminal("ep-6b", "comp-6")};
        const auto result = resolver.resolve(components, families, {}, endpoints);
        const auto& electrical = find(result.electrical_components, "comp-6");
        assert(electrical.status == ElectricalComponentResolutionStatus::Resolved);
        assert(electrical.family == SymbolFamily::Fuse);
        assert(electrical.terminal_endpoint_ids.size() == 2);
    }

    // TEST 7: the number of ComponentCandidates may exceed the number of
    // resolved ElectricalComponents - a mixed batch where only some
    // candidates carry sufficient evidence.
    {
        const auto components = std::vector<ComponentCandidate>{
            make_component("comp-7a", ComponentCandidateKind::Enclosure),
            make_component("comp-7b", ComponentCandidateKind::CircularSymbol),
            make_component("comp-7c", ComponentCandidateKind::DiagramFurniture),
            make_component("comp-7d", ComponentCandidateKind::ChassisGround)};
        const auto families = std::vector<SymbolFamilyResolution>{
            make_family_resolution(
                "comp-7a", SymbolFamily::Battery, SymbolFamilyResolutionStatus::Resolved),
            make_family_resolution(
                "comp-7d", SymbolFamily::Ground, SymbolFamilyResolutionStatus::Resolved)};
        const auto endpoints = std::vector<EndpointCandidate>{
            make_component_terminal("ep-7a", "comp-7a"),
            make_component_terminal("ep-7d", "comp-7d")};
        const auto result = resolver.resolve(components, families, {}, endpoints);
        assert(result.electrical_components.size() == 4);
        std::size_t resolved_count = 0;
        for (const auto& c : result.electrical_components) {
            if (c.status == ElectricalComponentResolutionStatus::Resolved) {
                ++resolved_count;
            }
        }
        assert(resolved_count == 1); // only comp-7a
        assert(resolved_count < result.electrical_components.size());
        assert(find(result.electrical_components, "comp-7c").rejection_reason ==
               ElectricalComponentRejectionReason::DiagramFurniture);
    }

    // TEST 8: production output remains deterministic - identical input
    // produces identical ids, ordering, and field values across repeated
    // calls.
    {
        const auto components = std::vector<ComponentCandidate>{
            make_component("comp-8a", ComponentCandidateKind::Enclosure),
            make_component("comp-8b", ComponentCandidateKind::CircularSymbol)};
        const auto families = std::vector<SymbolFamilyResolution>{
            make_family_resolution(
                "comp-8a", SymbolFamily::Switch, SymbolFamilyResolutionStatus::Resolved)};
        const auto endpoints = std::vector<EndpointCandidate>{
            make_component_terminal("ep-8a", "comp-8a")};

        const auto result_a = resolver.resolve(components, families, {}, endpoints);
        const auto result_b = resolver.resolve(components, families, {}, endpoints);

        assert(result_a.electrical_components.size() ==
               result_b.electrical_components.size());
        for (std::size_t i = 0; i < result_a.electrical_components.size(); ++i) {
            assert(result_a.electrical_components[i].id ==
                   result_b.electrical_components[i].id);
            assert(result_a.electrical_components[i].component_candidate_id ==
                   result_b.electrical_components[i].component_candidate_id);
            assert(result_a.electrical_components[i].status ==
                   result_b.electrical_components[i].status);
        }
    }

    std::cout << "electrical component resolver unit tests passed\n";
    return 0;
}

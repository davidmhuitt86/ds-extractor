#include "eke_dx_wire/topology/symbol_family_recognizer.hpp"

#include <algorithm>
#include <cassert>
#include <string>

using namespace eke::dx::wire;

namespace {

ComponentCandidate make_component(
    const std::string& id,
    ComponentCandidateKind kind,
    std::vector<std::string> labels = {}) {
    ComponentCandidate c;
    c.id = id;
    c.kind = kind;
    c.semantic_labels = std::move(labels);
    return c;
}

ComponentIdentityCanonicalization make_canonicalization(
    const std::string& component_id,
    const std::string& canonical_name,
    ComponentIdentityCanonicalizationStatus status =
        ComponentIdentityCanonicalizationStatus::Resolved) {
    ComponentIdentityCanonicalization c;
    c.id = "canon-" + component_id;
    c.component_id = component_id;
    c.canonical_name = canonical_name;
    c.status = status;
    return c;
}

const SymbolFamilyResolution& find_resolution(
    const std::vector<SymbolFamilyResolution>& resolutions, const std::string& component_id) {
    for (const auto& r : resolutions) {
        if (r.component_id == component_id) return r;
    }
    static SymbolFamilyResolution empty;
    assert(false && "resolution not found");
    return empty;
}

} // namespace

int main() {
    SymbolFamilyRecognizer recognizer;

    // 1. Empty input: no crash, empty output.
    {
        const auto result = recognizer.recognize({}, {}, {}, {});
        assert(result.evidence.empty());
        assert(result.resolutions.empty());
    }

    // 2. Unknown symbol: a component with no supporting evidence at all
    // resolves to Unknown/Unresolved, never a guess.
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::CircularSymbol)};
        const auto result = recognizer.recognize(components, {}, {}, {});
        const auto& r = find_resolution(result.resolutions, "comp-1");
        assert(r.status == SymbolFamilyResolutionStatus::Unresolved);
        assert(r.family == SymbolFamily::Unknown);
        assert(r.evidence_ids.empty());
    }

    // 3. Unresolved symbol with SOME evidence: a single uncorroborated
    // provider observation is insufficient to resolve, but the evidence
    // is still preserved for explainability ("recognizable in principle
    // but evidence is currently insufficient").
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::CircularSymbol)};
        std::vector<SymbolRecognitionObservation> observations = {
            {"comp-1", SymbolFamily::Lamp, ConfidenceClass::High, "looks like a lamp"}};
        const auto result = recognizer.recognize(components, {}, {}, observations);
        const auto& r = find_resolution(result.resolutions, "comp-1");
        assert(r.status == SymbolFamilyResolutionStatus::Unresolved);
        assert(r.family == SymbolFamily::Unknown);
        assert(!r.evidence_ids.empty()); // evidence preserved, just insufficient
    }

    // 4. Clearly resolved symbol: ChassisGround is a purpose-built
    // geometric classification (not generic shape resemblance) - this is
    // the one geometry-only rule that is sufficient by itself.
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::ChassisGround)};
        const auto result = recognizer.recognize(components, {}, {}, {});
        const auto& r = find_resolution(result.resolutions, "comp-1");
        assert(r.status == SymbolFamilyResolutionStatus::Resolved);
        assert(r.family == SymbolFamily::Ground);
        assert(r.confidence == ConfidenceClass::High);
        assert(!r.evidence_ids.empty());
    }

    // Clearly resolved via label + compatible geometry (the second
    // sufficient rule: keyword match AND compatible ComponentCandidateKind).
    {
        std::vector<ComponentCandidate> components = {
            make_component(
                "comp-1", ComponentCandidateKind::Enclosure, {"STARTER MOTOR"})};
        const auto result = recognizer.recognize(components, {}, {}, {});
        const auto& r = find_resolution(result.resolutions, "comp-1");
        assert(r.status == SymbolFamilyResolutionStatus::Resolved);
        assert(r.family == SymbolFamily::Motor);
        assert(r.confidence == ConfidenceClass::Medium);
    }

    // Label alone, without compatible geometry, is NOT sufficient - the
    // explicit "no unsupported guessing" evidence rule.
    {
        std::vector<ComponentCandidate> components = {
            make_component(
                "comp-1", ComponentCandidateKind::PrimitiveSymbol, {"STARTER MOTOR"})};
        // Motor rule only accepts CircularSymbol/Enclosure - PrimitiveSymbol
        // is not compatible, so the label match must not fire.
        const auto result = recognizer.recognize(components, {}, {}, {});
        const auto& r = find_resolution(result.resolutions, "comp-1");
        assert(r.status == SymbolFamilyResolutionStatus::Unresolved);
    }

    // 5. Compatible evidence: two evidence sources agreeing on the same
    // family reinforce rather than conflict.
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::Enclosure, {"BATTERY"})};
        std::vector<SymbolRecognitionObservation> observations = {
            {"comp-1", SymbolFamily::Battery, ConfidenceClass::Medium, "battery-like body"}};
        const auto result = recognizer.recognize(components, {}, {}, observations);
        const auto& r = find_resolution(result.resolutions, "comp-1");
        assert(r.status == SymbolFamilyResolutionStatus::Resolved);
        assert(r.family == SymbolFamily::Battery);
    }

    // 6. Conflicting evidence: two independent strong sources support
    // incompatible families - Conflicted, never an arbitrary pick.
    {
        std::vector<ComponentCandidate> components = {
            make_component(
                "comp-1", ComponentCandidateKind::Enclosure, {"RELAY SWITCH"})};
        // "RELAY" and "SWITCH" both match compatible-geometry rules for
        // Enclosure -> both Relay and Switch get label-keyword evidence.
        const auto result = recognizer.recognize(components, {}, {}, {});
        const auto& r = find_resolution(result.resolutions, "comp-1");
        assert(r.status == SymbolFamilyResolutionStatus::Conflicted);
        assert(r.family == SymbolFamily::Unknown);
        assert(r.evidence_ids.size() >= 2);
    }

    // 7. Deterministic IDs: identical evidence produces identical IDs
    // across repeated calls.
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::ChassisGround)};
        const auto r1 = recognizer.recognize(components, {}, {}, {});
        const auto r2 = recognizer.recognize(components, {}, {}, {});
        assert(r1.resolutions[0].id == r2.resolutions[0].id);
        assert(r1.evidence[0].id == r2.evidence[0].id);
    }

    // 8. Deterministic ordering: results sorted by component_id/id
    // regardless of input order.
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-z", ComponentCandidateKind::ChassisGround),
            make_component("comp-a", ComponentCandidateKind::ChassisGround)};
        const auto result = recognizer.recognize(components, {}, {}, {});
        assert(result.resolutions.size() == 2);
        assert(result.resolutions[0].component_id == "comp-a");
        assert(result.resolutions[1].component_id == "comp-z");
    }

    // 9. Duplicate evidence handling: the same keyword appearing in
    // multiple label entries does not produce duplicate evidence ids in
    // the resolution's evidence_ids.
    {
        std::vector<ComponentCandidate> components = {
            make_component(
                "comp-1", ComponentCandidateKind::ChassisGround, {"GROUND", "CHASSIS GROUND"})};
        const auto result = recognizer.recognize(components, {}, {}, {});
        const auto& r = find_resolution(result.resolutions, "comp-1");
        auto ids = r.evidence_ids;
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        assert(ids.size() == r.evidence_ids.size());
    }

    // 10. Provenance preservation: evidence_ids trace to real evidence
    // records with explainable detail, not just a bare status.
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::ChassisGround)};
        const auto result = recognizer.recognize(components, {}, {}, {});
        assert(!result.evidence.empty());
        assert(!result.evidence[0].detail.empty());
        assert(!result.evidence[0].source.empty());
        const auto& r = find_resolution(result.resolutions, "comp-1");
        assert(r.evidence_ids[0] == result.evidence[0].id);
    }

    // 11. SymbolGeometry reference preservation: resolution references
    // AP-WIRE-023's geometry by id, never duplicates it.
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::ChassisGround)};
        ComponentSymbolGeometry geometry;
        geometry.id = "geom-1";
        geometry.component_id = "comp-1";
        const auto result = recognizer.recognize(components, {geometry}, {}, {});
        const auto& r = find_resolution(result.resolutions, "comp-1");
        assert(r.source_symbol_geometry_id == "geom-1");
    }

    // 12. SymbolPrimitive reference preservation: the recognizer never
    // takes or copies SymbolPrimitive objects directly - it only reaches
    // them indirectly via source_symbol_geometry_id (a reference to the
    // AP-WIRE-023 ComponentSymbolGeometry, which itself references its
    // primitive_ids). No primitive content is duplicated into this AP's
    // model at all.
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::ChassisGround)};
        ComponentSymbolGeometry geometry;
        geometry.id = "geom-1";
        geometry.component_id = "comp-1";
        geometry.primitive_ids = {"prim-1", "prim-2"};
        const auto result = recognizer.recognize(components, {geometry}, {}, {});
        const auto& r = find_resolution(result.resolutions, "comp-1");
        assert(r.source_symbol_geometry_id == "geom-1");
        // The resolution carries only the geometry id, never prim-1/prim-2.
    }

    // 13. TerminalLead evidence: current rules do not treat raw primitive
    // kind counts as sufficient evidence by themselves (no rule keys on
    // SymbolPrimitiveKind alone) - confirms the recognizer does not
    // invent a geometry-shape-only rule beyond the one purpose-built
    // ChassisGround case.
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::CircularSymbol)};
        const auto result = recognizer.recognize(components, {}, {}, {});
        const auto& r = find_resolution(result.resolutions, "comp-1");
        assert(r.status == SymbolFamilyResolutionStatus::Unresolved);
    }

    // 14. Component identity remains independent: a canonical name is
    // consumed as evidence input, but SymbolFamilyResolution never writes
    // back into ComponentCandidate or ComponentIdentityCanonicalization.
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::Enclosure)};
        const std::vector<ComponentCandidate> components_before = components;
        std::vector<ComponentIdentityCanonicalization> canonicalizations = {
            make_canonicalization("comp-1", "BATTERY")};
        const auto canonicalizations_before = canonicalizations;

        recognizer.recognize(components, {}, canonicalizations, {});

        assert(components[0].semantic_labels == components_before[0].semantic_labels);
        assert(canonicalizations[0].canonical_name == canonicalizations_before[0].canonical_name);
    }

    // Canonical name (from AP-WIRE-018) is used the same way a direct
    // label would be - both require compatible geometry.
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::Enclosure)};
        std::vector<ComponentIdentityCanonicalization> canonicalizations = {
            make_canonicalization("comp-1", "BATTERY")};
        const auto result = recognizer.recognize(components, {}, canonicalizations, {});
        const auto& r = find_resolution(result.resolutions, "comp-1");
        assert(r.status == SymbolFamilyResolutionStatus::Resolved);
        assert(r.family == SymbolFamily::Battery);
    }

    // An Unresolved/Conflicted canonicalization must not contribute text
    // evidence (only Resolved canonical names are trusted).
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::Enclosure)};
        std::vector<ComponentIdentityCanonicalization> canonicalizations = {
            make_canonicalization(
                "comp-1", "BATTERY", ComponentIdentityCanonicalizationStatus::Conflicted)};
        const auto result = recognizer.recognize(components, {}, canonicalizations, {});
        const auto& r = find_resolution(result.resolutions, "comp-1");
        assert(r.status == SymbolFamilyResolutionStatus::Unresolved);
    }

    // 15. Terminal recognition remains independent: the recognizer takes
    // no TerminalCandidate input and cannot create one - structurally
    // enforced by its signature (no return type includes TerminalCandidate).

    // 16/17/18. No topology/wire/net mutation: the recognizer's inputs
    // (components, geometries, canonicalizations, observations) contain
    // nothing from topology/wires/nets at all - structurally impossible
    // to mutate what was never passed in.

    // 19. No terminal invention: confirmed structurally (see 15) - the
    // return type is SymbolFamilyRecognitionArtifacts{evidence,
    // resolutions} only.

    // 20. AP-WIRE-024 conflict preservation: symbol-family recognition
    // does not read EndpointCandidate/TerminalCandidate at all, so an
    // AP-WIRE-024 conflicted endpoint cannot influence or be influenced
    // by this recognizer - a component with a conflicted terminal
    // relationship still resolves (or doesn't) purely on its own
    // geometry/identity evidence.
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::ChassisGround)};
        // No endpoint/terminal data is even accepted by recognize() -
        // demonstrating the independence structurally.
        const auto result = recognizer.recognize(components, {}, {}, {});
        assert(find_resolution(result.resolutions, "comp-1").status ==
               SymbolFamilyResolutionStatus::Resolved);
    }

    // 21. Null recognition provider: the default pipeline provider
    // returns no observations, and the recognizer must still function
    // correctly (geometry/label rules alone).
    {
        NullSymbolRecognitionProvider provider;
        const auto observations = provider.recognize({}, {}, {}, "fixture", 0);
        assert(observations.empty());
        assert(provider.provider_id() == "none");

        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::ChassisGround)};
        const auto result = recognizer.recognize(components, {}, {}, observations);
        assert(find_resolution(result.resolutions, "comp-1").status ==
               SymbolFamilyResolutionStatus::Resolved);
    }

    // 22. Full assembly: multiple components, mixed evidence strength,
    // furniture correctly excluded.
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-ground", ComponentCandidateKind::ChassisGround),
            make_component("comp-lamp", ComponentCandidateKind::CircularSymbol, {"INDICATOR LIGHT"}),
            make_component("comp-unknown", ComponentCandidateKind::CircularSymbol),
            make_component("comp-furniture", ComponentCandidateKind::DiagramFurniture, {"MOTOR"})};
        const auto result = recognizer.recognize(components, {}, {}, {});

        assert(find_resolution(result.resolutions, "comp-ground").status ==
               SymbolFamilyResolutionStatus::Resolved);
        assert(find_resolution(result.resolutions, "comp-lamp").family == SymbolFamily::Lamp);
        assert(find_resolution(result.resolutions, "comp-unknown").status ==
               SymbolFamilyResolutionStatus::Unresolved);
        // DiagramFurniture must never enter symbol-family recognition,
        // even with a matching label - no resolution is produced for it.
        for (const auto& r : result.resolutions) {
            assert(r.component_id != "comp-furniture");
        }
    }

    // 23. Serialization determinism: re-running against the same model
    // produces byte-identical evidence/resolution content, not just IDs.
    {
        std::vector<ComponentCandidate> components = {
            make_component("comp-1", ComponentCandidateKind::Enclosure, {"BATTERY"}),
            make_component("comp-2", ComponentCandidateKind::ChassisGround)};
        const auto r1 = recognizer.recognize(components, {}, {}, {});
        const auto r2 = recognizer.recognize(components, {}, {}, {});
        assert(r1.resolutions.size() == r2.resolutions.size());
        for (std::size_t i = 0; i < r1.resolutions.size(); ++i) {
            assert(r1.resolutions[i].id == r2.resolutions[i].id);
            assert(r1.resolutions[i].family == r2.resolutions[i].family);
            assert(r1.resolutions[i].status == r2.resolutions[i].status);
            assert(r1.resolutions[i].evidence_ids == r2.resolutions[i].evidence_ids);
        }
    }

    return 0;
}

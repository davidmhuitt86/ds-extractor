#include "eke_dx_wire/topology/symbol_family_recognizer.hpp"
#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <unordered_map>

namespace eke::dx::wire {
namespace {

std::string to_upper(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

const char* family_name(SymbolFamily family) {
    switch (family) {
    case SymbolFamily::Ground: return "ground";
    case SymbolFamily::Lamp: return "lamp";
    case SymbolFamily::Switch: return "switch";
    case SymbolFamily::Relay: return "relay";
    case SymbolFamily::Motor: return "motor";
    case SymbolFamily::Diode: return "diode";
    case SymbolFamily::Alternator: return "alternator";
    case SymbolFamily::Battery: return "battery";
    case SymbolFamily::Solenoid: return "solenoid";
    case SymbolFamily::Coil: return "coil";
    case SymbolFamily::Unknown: return "unknown";
    }
    return "unknown";
}

struct LabelRule {
    SymbolFamily family;
    std::vector<std::string> keywords;
    std::set<ComponentCandidateKind> compatible_kinds;
};

// Every rule requires BOTH a keyword match against already-resolved
// component identity text AND a geometrically compatible
// ComponentCandidateKind. A label match alone (e.g. against a
// DiagramFurniture-adjacent mislabel) or geometry alone (e.g. any
// CircularSymbol) is never sufficient on its own - see the AP's
// explicit "no unsupported symbol-family guessing" requirement.
const std::vector<LabelRule>& label_rules() {
    static const std::vector<LabelRule> rules = {
        {SymbolFamily::Ground, {"GROUND", "GND"},
         {ComponentCandidateKind::ChassisGround, ComponentCandidateKind::Enclosure,
          ComponentCandidateKind::CircularSymbol}},
        {SymbolFamily::Lamp, {"LAMP", "LIGHT", "INDICATOR", "HEADLIGHT", "TAILLIGHT", "BULB"},
         {ComponentCandidateKind::CircularSymbol, ComponentCandidateKind::Enclosure}},
        {SymbolFamily::Switch, {"SWITCH"},
         {ComponentCandidateKind::CircularSymbol, ComponentCandidateKind::Enclosure,
          ComponentCandidateKind::PrimitiveSymbol}},
        {SymbolFamily::Relay, {"RELAY"},
         {ComponentCandidateKind::Enclosure, ComponentCandidateKind::CircularSymbol}},
        {SymbolFamily::Motor, {"MOTOR"},
         {ComponentCandidateKind::CircularSymbol, ComponentCandidateKind::Enclosure}},
        {SymbolFamily::Diode, {"DIODE", "RECTIFIER"},
         {ComponentCandidateKind::CircularSymbol, ComponentCandidateKind::Enclosure,
          ComponentCandidateKind::PrimitiveSymbol}},
        {SymbolFamily::Alternator, {"ALTERNATOR", "GENERATOR"},
         {ComponentCandidateKind::CircularSymbol, ComponentCandidateKind::Enclosure}},
        {SymbolFamily::Battery, {"BATTERY"}, {ComponentCandidateKind::Enclosure}},
        {SymbolFamily::Solenoid, {"SOLENOID"},
         {ComponentCandidateKind::Enclosure, ComponentCandidateKind::CircularSymbol}},
        {SymbolFamily::Coil, {"COIL", "PULSE GENERATOR"},
         {ComponentCandidateKind::Enclosure, ComponentCandidateKind::CircularSymbol}},
    };
    return rules;
}

} // namespace

SymbolFamilyRecognitionArtifacts SymbolFamilyRecognizer::recognize(
    const std::vector<ComponentCandidate>& components,
    const std::vector<ComponentSymbolGeometry>& geometries,
    const std::vector<ComponentIdentityCanonicalization>& canonicalizations,
    const std::vector<SymbolRecognitionObservation>& provider_observations) const {

    SymbolFamilyRecognitionArtifacts result;

    std::map<std::string, std::string> symbol_geometry_by_component;
    for (const auto& geometry : geometries) {
        symbol_geometry_by_component[geometry.component_id] = geometry.id;
    }

    std::unordered_map<std::string, std::string> canonical_name_by_component;
    for (const auto& canonicalization : canonicalizations) {
        if (canonicalization.status == ComponentIdentityCanonicalizationStatus::Resolved) {
            canonical_name_by_component[canonicalization.component_id] =
                canonicalization.canonical_name;
        }
    }

    std::multimap<std::string, SymbolRecognitionObservation> observations_by_component;
    for (const auto& observation : provider_observations) {
        observations_by_component.emplace(observation.component_id, observation);
    }

    for (const auto& component : components) {
        if (component.kind == ComponentCandidateKind::DiagramFurniture ||
            component.id.empty()) {
            continue;
        }

        std::vector<std::string> label_texts;
        for (const auto& label : component.semantic_labels) {
            label_texts.push_back(to_upper(label));
        }
        if (const auto it = canonical_name_by_component.find(component.id);
            it != canonical_name_by_component.end()) {
            label_texts.push_back(to_upper(it->second));
        }

        std::vector<SymbolFamilyEvidence> component_evidence;

        // Rule 1: purpose-built geometric classification. ChassisGround is
        // produced by ShapeDetector's dedicated ground-bar-pattern
        // detector (parallel bars of decreasing width), not a generic
        // circle/rectangle bucket - this is categorically different from
        // "this shape resembles a ground symbol."
        if (component.kind == ComponentCandidateKind::ChassisGround) {
            SymbolFamilyEvidence evidence;
            evidence.component_id = component.id;
            evidence.family = SymbolFamily::Ground;
            evidence.kind = SymbolFamilyEvidenceKind::PurposeBuiltGeometricClassification;
            evidence.confidence = ConfidenceClass::High;
            evidence.source = "shape-detector-chassis-ground";
            evidence.detail =
                "ComponentCandidateKind::ChassisGround comes from a purpose-built "
                "ground-symbol detector, not a generic shape bucket";
            evidence.id = stable_id(
                "symbol-family-evidence",
                component.id + ":ground:" + evidence.source);
            component_evidence.push_back(std::move(evidence));
        }

        // Rule 2: label keyword + compatible geometry.
        for (const auto& rule : label_rules()) {
            if (!rule.compatible_kinds.count(component.kind)) continue;

            for (const auto& keyword : rule.keywords) {
                const bool matched = std::any_of(
                    label_texts.begin(), label_texts.end(),
                    [&](const std::string& text) { return text.find(keyword) != std::string::npos; });
                if (!matched) continue;

                SymbolFamilyEvidence evidence;
                evidence.component_id = component.id;
                evidence.family = rule.family;
                evidence.kind = SymbolFamilyEvidenceKind::LabelKeywordWithCompatibleGeometry;
                evidence.confidence = ConfidenceClass::Medium;
                evidence.source = "label-keyword:" + keyword;
                evidence.detail =
                    "resolved component label/identity text contains \"" + keyword +
                    "\" and component geometry is compatible with " + family_name(rule.family);
                evidence.id = stable_id(
                    "symbol-family-evidence",
                    component.id + ":" + family_name(rule.family) + ":" + evidence.source);
                component_evidence.push_back(std::move(evidence));
                break; // one evidence entry per rule/family is enough
            }
        }

        // Rule 3: provider observations. Evidence only - see the
        // strength/corroboration logic below for why a single
        // uncorroborated provider observation cannot resolve a family by
        // itself.
        const auto range = observations_by_component.equal_range(component.id);
        for (auto it = range.first; it != range.second; ++it) {
            const auto& observation = it->second;
            if (observation.family == SymbolFamily::Unknown) continue;

            SymbolFamilyEvidence evidence;
            evidence.component_id = component.id;
            evidence.family = observation.family;
            evidence.kind = SymbolFamilyEvidenceKind::ProviderObservation;
            evidence.confidence = observation.confidence;
            evidence.source = "provider-observation";
            evidence.detail = observation.detail;
            evidence.id = stable_id(
                "symbol-family-evidence",
                component.id + ":" + family_name(observation.family) + ":provider:" +
                    observation.detail);
            component_evidence.push_back(std::move(evidence));
        }

        // ---- resolution ------------------------------------------------
        std::map<SymbolFamily, std::vector<const SymbolFamilyEvidence*>> by_family;
        for (const auto& evidence : component_evidence) {
            by_family[evidence.family].push_back(&evidence);
        }

        std::vector<SymbolFamily> strong_families;
        for (const auto& [family, items] : by_family) {
            const bool has_strong_kind = std::any_of(
                items.begin(), items.end(),
                [](const SymbolFamilyEvidence* e) {
                    return e->kind != SymbolFamilyEvidenceKind::ProviderObservation;
                });
            const bool provider_corroborated = items.size() >= 2 && !has_strong_kind;
            if (has_strong_kind || provider_corroborated) {
                strong_families.push_back(family);
            }
        }
        std::sort(strong_families.begin(), strong_families.end());

        SymbolFamilyResolution resolution;
        resolution.component_id = component.id;
        resolution.id = stable_id("symbol-family-resolution", component.id);
        if (const auto it = symbol_geometry_by_component.find(component.id);
            it != symbol_geometry_by_component.end()) {
            resolution.source_symbol_geometry_id = it->second;
        }

        if (strong_families.size() == 1) {
            resolution.status = SymbolFamilyResolutionStatus::Resolved;
            resolution.family = strong_families.front();
            const auto& winning_evidence = by_family[resolution.family];
            const bool has_purpose_built = std::any_of(
                winning_evidence.begin(), winning_evidence.end(),
                [](const SymbolFamilyEvidence* e) {
                    return e->kind == SymbolFamilyEvidenceKind::PurposeBuiltGeometricClassification;
                });
            resolution.confidence =
                has_purpose_built ? ConfidenceClass::High : ConfidenceClass::Medium;
            for (const auto* evidence : winning_evidence) {
                resolution.evidence_ids.push_back(evidence->id);
            }
        } else if (strong_families.size() >= 2) {
            resolution.status = SymbolFamilyResolutionStatus::Conflicted;
            resolution.family = SymbolFamily::Unknown;
            resolution.confidence = ConfidenceClass::Unresolved;
            for (const auto family : strong_families) {
                for (const auto* evidence : by_family[family]) {
                    resolution.evidence_ids.push_back(evidence->id);
                }
            }
        } else {
            resolution.status = SymbolFamilyResolutionStatus::Unresolved;
            resolution.family = SymbolFamily::Unknown;
            resolution.confidence = ConfidenceClass::Unresolved;
            // Preserve any weak/uncorroborated evidence for explainability
            // even though it was insufficient to resolve - this is the
            // "recognizable in principle but evidence is currently
            // insufficient" state, distinct from "no evidence at all."
            for (const auto& evidence : component_evidence) {
                resolution.evidence_ids.push_back(evidence.id);
            }
        }

        std::sort(resolution.evidence_ids.begin(), resolution.evidence_ids.end());
        resolution.evidence_ids.erase(
            std::unique(resolution.evidence_ids.begin(), resolution.evidence_ids.end()),
            resolution.evidence_ids.end());

        for (auto& evidence : component_evidence) {
            result.evidence.push_back(std::move(evidence));
        }
        result.resolutions.push_back(std::move(resolution));
    }

    std::sort(
        result.evidence.begin(), result.evidence.end(),
        [](const SymbolFamilyEvidence& a, const SymbolFamilyEvidence& b) { return a.id < b.id; });
    std::sort(
        result.resolutions.begin(), result.resolutions.end(),
        [](const SymbolFamilyResolution& a, const SymbolFamilyResolution& b) {
            return a.component_id < b.component_id;
        });

    return result;
}

} // namespace eke::dx::wire

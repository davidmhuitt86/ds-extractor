#include "eke_dx_wire/topology/component_identity_registry.hpp"

#include <vector>

using namespace eke::dx::wire;

int main() {
    StaticComponentIdentityRegistry registry({
        {
            "component.honda.ignition-switch",
            "Ignition Switch",
            {"IGNITION SWITCH", "IGN-SWITCH"}
        },
        {
            "component.honda.starter-relay",
            "Starter Relay",
            {"STARTER RELAY"}
        },
        {
            "component.honda.ambiguous-a",
            "Ambiguous A",
            {"SHARED LABEL"}
        },
        {
            "component.honda.ambiguous-b",
            "Ambiguous B",
            {"SHARED LABEL"}
        }
    });

    const auto ignition = registry.lookup(" ignition_switch ");
    if (!ignition.has_value() || ignition->canonical_id != "component.honda.ignition-switch") return 1;

    const auto missing = registry.lookup("HEADLIGHT");
    if (missing.has_value()) return 1;

    const auto ambiguous = registry.lookup("SHARED LABEL");
    if (ambiguous.has_value()) return 1;

    ComponentIdentityCanonicalizer canonicalizer;

    const std::vector<ComponentIdentityResolution> resolutions = {
        {
            "resolution-1",
            "component-1",
            "IGNITION SWITCH",
            ConfidenceClass::High,
            ComponentIdentityResolutionStatus::Resolved,
            {}
        },
        {
            "resolution-2",
            "component-2",
            "HEADLIGHT",
            ConfidenceClass::Medium,
            ComponentIdentityResolutionStatus::Resolved,
            {}
        },
        {
            "resolution-3",
            "component-3",
            "",
            ConfidenceClass::Unresolved,
            ComponentIdentityResolutionStatus::Conflicted,
            {}
        }
    };

    const auto canonicalized =
        canonicalizer.canonicalize(resolutions, registry);

    if (canonicalized.size() != 3) return 1;

    const auto find_by_source = [&](const std::string& id)
        -> const ComponentIdentityCanonicalization* {
        for (const auto& item : canonicalized) {
            if (item.source_resolution_id == id) {
                return &item;
            }
        }
        return nullptr;
    };

    const auto* resolved = find_by_source("resolution-1");
    if (resolved == nullptr ||
        resolved->status != ComponentIdentityCanonicalizationStatus::Resolved ||
        resolved->canonical_id != "component.honda.ignition-switch" ||
        resolved->canonical_name != "Ignition Switch" ||
        resolved->confidence != ConfidenceClass::High) return 1;

    const auto* not_found = find_by_source("resolution-2");
    if (not_found == nullptr ||
        not_found->status != ComponentIdentityCanonicalizationStatus::NotFound ||
        !not_found->canonical_id.empty() ||
        not_found->confidence != ConfidenceClass::Unresolved) return 1;

    const auto* conflicted = find_by_source("resolution-3");
    if (conflicted == nullptr ||
        conflicted->status != ComponentIdentityCanonicalizationStatus::Conflicted ||
        !conflicted->canonical_id.empty()) return 1;

    NullComponentIdentityRegistry null_registry;
    const auto null_result = canonicalizer.canonicalize(
        resolutions,
        null_registry);
    for (const auto& item : null_result) {
        if (item.status == ComponentIdentityCanonicalizationStatus::Resolved) {
            return 1;
        }
    }

    return 0;
}

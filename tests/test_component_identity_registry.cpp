#include "eke_dx_wire/topology/component_identity_registry.hpp"

#include <cassert>
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
    assert(ignition.has_value());
    assert(ignition->canonical_id == "component.honda.ignition-switch");

    const auto missing = registry.lookup("HEADLIGHT");
    assert(!missing.has_value());

    const auto ambiguous = registry.lookup("SHARED LABEL");
    assert(!ambiguous.has_value());

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

    assert(canonicalized.size() == 3);

    const auto find_by_source = [&](const std::string& id) -> const ComponentIdentityCanonicalization& {
        for (const auto& item : canonicalized) {
            if (item.source_resolution_id == id) {
                return item;
            }
        }
        assert(false);
        return canonicalized.front();
    };

    const auto& resolved = find_by_source("resolution-1");
    assert(resolved.status == ComponentIdentityCanonicalizationStatus::Resolved);
    assert(resolved.canonical_id == "component.honda.ignition-switch");
    assert(resolved.canonical_name == "Ignition Switch");
    assert(resolved.confidence == ConfidenceClass::High);

    const auto& not_found = find_by_source("resolution-2");
    assert(not_found.status == ComponentIdentityCanonicalizationStatus::NotFound);
    assert(not_found.canonical_id.empty());
    assert(not_found.confidence == ConfidenceClass::Unresolved);

    const auto& conflicted = find_by_source("resolution-3");
    assert(conflicted.status == ComponentIdentityCanonicalizationStatus::Conflicted);
    assert(conflicted.canonical_id.empty());

    NullComponentIdentityRegistry null_registry;
    const auto null_result = canonicalizer.canonicalize(
        resolutions,
        null_registry);
    for (const auto& item : null_result) {
        if (item.status == ComponentIdentityCanonicalizationStatus::Resolved) {
            assert(false);
        }
    }

    return 0;
}

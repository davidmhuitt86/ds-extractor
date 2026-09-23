#include "eke_dx_wire/topology/component_identity_resolver.hpp"

#include <cassert>
#include <string>
#include <vector>

using namespace eke::dx::wire;

int main() {
    ComponentIdentityResolver resolver;

    const std::vector<ComponentIdentityEvidence> evidence {
        {"e1", "component-a", ComponentIdentityEvidenceKind::ComponentLabel,
         "Ignition Switch", "IGNITION SWITCH", ConfidenceClass::High, 5.0, "test"},
        {"e2", "component-a", ComponentIdentityEvidenceKind::ComponentLabel,
         "IGNITION SWITCH", "IGNITION SWITCH", ConfidenceClass::Medium, 8.0, "test"},
        {"e3", "component-b", ComponentIdentityEvidenceKind::ComponentLabel,
         "Starter Relay", "STARTER RELAY", ConfidenceClass::High, 4.0, "test"},
        {"e4", "component-b", ComponentIdentityEvidenceKind::ComponentLabel,
         "Solenoid", "SOLENOID", ConfidenceClass::High, 4.0, "test"},
        {"e5", "component-c", ComponentIdentityEvidenceKind::ComponentLabel,
         "Unknown", "", ConfidenceClass::High, 2.0, "test"}
    };

    const auto resolved = resolver.resolve(evidence);

    assert(resolved.size() == 2);
    assert(resolved[0].component_id == "component-a");
    assert(resolved[0].status == ComponentIdentityResolutionStatus::Resolved);
    assert(resolved[0].identity == "IGNITION SWITCH");
    assert(resolved[0].confidence == ConfidenceClass::High);
    assert(resolved[0].evidence_ids.size() == 2);

    assert(resolved[1].component_id == "component-b");
    assert(resolved[1].status == ComponentIdentityResolutionStatus::Conflicted);
    assert(resolved[1].identity.empty());
    assert(resolved[1].confidence == ConfidenceClass::Unresolved);
    assert(resolved[1].evidence_ids.size() == 2);

    return 0;
}

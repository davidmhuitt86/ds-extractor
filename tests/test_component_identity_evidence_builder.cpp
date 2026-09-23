#include "eke_dx_wire/topology/component_identity_evidence_builder.hpp"

#include <cassert>
#include <vector>

using namespace eke::dx::wire;

int main() {
    ComponentIdentityEvidenceBuilder builder;

    EngineeringObjectSemanticResolution component{
        "resolution-1", "text-1", "component-1",
        SemanticAssociationTargetKind::Component,
        TextSemanticKind::ComponentLabel,
        "Ignition Coil", "IGNITION COIL",
        ConfidenceClass::High, 4.0, "test"
    };

    EngineeringObjectSemanticResolution duplicate{
        "resolution-2", "text-2", "component-1",
        SemanticAssociationTargetKind::Component,
        TextSemanticKind::ComponentLabel,
        "Ignition Coil", "IGNITION COIL",
        ConfidenceClass::Medium, 2.0, "test"
    };

    EngineeringObjectSemanticResolution connector{
        "resolution-3", "text-3", "component-2",
        SemanticAssociationTargetKind::Component,
        TextSemanticKind::ConnectorLabel,
        "4P CONNECTOR", "4P CONNECTOR",
        ConfidenceClass::High, 3.0, "test"
    };

    EngineeringObjectSemanticResolution endpoint{
        "resolution-4", "text-4", "endpoint-1",
        SemanticAssociationTargetKind::Endpoint,
        TextSemanticKind::ComponentLabel,
        "IGNITION COIL", "IGNITION COIL",
        ConfidenceClass::High, 1.0, "test"
    };

    const auto result = builder.build(
        {component, duplicate, connector, endpoint});

    assert(result.size() == 2);
    assert(result[0].component_id == "component-1");
    assert(result[0].normalized_text == "IGNITION COIL");
    assert(result[0].confidence == ConfidenceClass::High);
    assert(result[1].component_id == "component-2");
    assert(result[1].kind == ComponentIdentityEvidenceKind::ConnectorLabel);

    return 0;
}

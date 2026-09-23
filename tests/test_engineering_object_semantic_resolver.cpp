#include "eke_dx_wire/topology/engineering_object_semantic_resolver.hpp"

#include <cassert>
#include <string>
#include <vector>

using namespace eke::dx::wire;

int main() {
    EngineeringObjectSemanticResolver resolver;

    TextSemanticEvidence label{
        "semantic-1",
        "text-1",
        "IGNITION COIL",
        "IGNITION COIL",
        TextSemanticKind::ComponentLabel,
        ConfidenceClass::High,
        "json-observation"
    };

    SemanticAssociation association{
        "assoc-1",
        "text-1",
        "component-1",
        SemanticAssociationTargetKind::Component,
        SemanticAssociationRelation::LabelToComponent,
        5.0,
        ConfidenceClass::High
    };

    const auto resolved = resolver.resolve({label}, {association});
    assert(resolved.size() == 1);
    assert(resolved[0].target_id == "component-1");
    assert(resolved[0].target_kind == SemanticAssociationTargetKind::Component);
    assert(resolved[0].semantic_kind == TextSemanticKind::ComponentLabel);
    assert(resolved[0].normalized_text == "IGNITION COIL");
    assert(resolved[0].confidence == ConfidenceClass::High);

    TextSemanticEvidence ambiguous{
        "semantic-2",
        "text-2",
        "COIL",
        "COIL",
        TextSemanticKind::ComponentLabel,
        ConfidenceClass::High,
        "json-observation"
    };

    SemanticAssociation a{
        "assoc-2a", "text-2", "component-a",
        SemanticAssociationTargetKind::Component,
        SemanticAssociationRelation::LabelToComponent,
        10.0, ConfidenceClass::High
    };
    SemanticAssociation b{
        "assoc-2b", "text-2", "component-b",
        SemanticAssociationTargetKind::Component,
        SemanticAssociationRelation::LabelToComponent,
        10.0, ConfidenceClass::High
    };

    assert(resolver.resolve({ambiguous}, {a, b}).empty());

    TextSemanticEvidence conflict_a{
        "semantic-3a", "text-3", "IGNITION", "IGNITION",
        TextSemanticKind::FunctionLabel, ConfidenceClass::High,
        "json-observation"
    };
    TextSemanticEvidence conflict_b{
        "semantic-3b", "text-3", "STARTER", "STARTER",
        TextSemanticKind::FunctionLabel, ConfidenceClass::High,
        "json-observation"
    };

    SemanticAssociation ca{
        "assoc-3a", "text-3", "endpoint-1",
        SemanticAssociationTargetKind::Endpoint,
        SemanticAssociationRelation::LabelToEndpoint,
        4.0, ConfidenceClass::High
    };
    SemanticAssociation cb{
        "assoc-3b", "text-3", "endpoint-1",
        SemanticAssociationTargetKind::Endpoint,
        SemanticAssociationRelation::LabelToEndpoint,
        4.0, ConfidenceClass::High
    };

    // One text region cannot produce two semantic observations in the same
    // category with different values without becoming ambiguous.
    const auto conflict = resolver.resolve(
        {conflict_a, conflict_b}, {ca, cb});
    assert(conflict.empty());

    return 0;
}

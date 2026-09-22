#include "eke_dx_wire/topology/semantic_evidence_associator.hpp"

#include <cassert>
#include <cmath>

using namespace eke::dx::wire;

int main() {
    TextRegion text;
    text.id = "text-region-1";
    text.kind = TextRegionKind::Label;
    text.bounds = BoundingBox{100, 100, 20, 8};
    text.confidence = 0.85;

    ComponentCandidate component;
    component.id = "component-candidate-1";
    component.kind = ComponentCandidateKind::CircularSymbol;
    component.bounds = BoundingBox{125, 100, 20, 20};
    component.confidence = ConfidenceClass::High;

    EndpointCandidate endpoint;
    endpoint.id = "endpoint-1";
    endpoint.position = Point2D{120.0, 104.0};
    endpoint.kind = EndpointKind::ComponentTerminal;
    endpoint.terminal_role = TerminalRole::ComponentTerminal;
    endpoint.confidence = ConfidenceClass::Medium;

    EndpointCandidate far_endpoint;
    far_endpoint.id = "endpoint-far";
    far_endpoint.position = Point2D{300.0, 300.0};
    far_endpoint.kind = EndpointKind::GeometricConductorEnd;
    far_endpoint.confidence = ConfidenceClass::Low;

    const auto result =
        SemanticEvidenceAssociator().associate(
            {text},
            {component},
            {endpoint, far_endpoint});

    assert(result.size() == 2);

    const auto* component_association = &result[0];
    const auto* endpoint_association = &result[1];
    if (result[0].target_kind != SemanticAssociationTargetKind::Component) {
        component_association = &result[1];
        endpoint_association = &result[0];
    }

    assert(component_association->text_region_id == "text-region-1");
    assert(component_association->target_kind ==
        SemanticAssociationTargetKind::Component);
    assert(component_association->relation ==
        SemanticAssociationRelation::LabelToComponent);
    assert(component_association->target_id == "component-candidate-1");
    assert(component_association->confidence == ConfidenceClass::High);

    assert(endpoint_association->text_region_id == "text-region-1");
    assert(endpoint_association->target_kind ==
        SemanticAssociationTargetKind::Endpoint);
    assert(endpoint_association->relation ==
        SemanticAssociationRelation::LabelToEndpoint);
    assert(endpoint_association->target_id == "endpoint-1");
    assert(endpoint_association->confidence == ConfidenceClass::High);
    assert(std::abs(endpoint_association->distance - 0.0) < 1e-9);

    return 0;
}

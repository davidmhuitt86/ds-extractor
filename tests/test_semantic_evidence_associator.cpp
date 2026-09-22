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

    assert(result[0].text_region_id == "text-region-1");
    assert(result[0].target_kind ==
        SemanticAssociationTargetKind::Component);
    assert(result[0].relation ==
        SemanticAssociationRelation::LabelToComponent);
    assert(result[0].target_id == "component-candidate-1");
    assert(result[0].confidence == ConfidenceClass::High);

    assert(result[1].text_region_id == "text-region-1");
    assert(result[1].target_kind ==
        SemanticAssociationTargetKind::Endpoint);
    assert(result[1].relation ==
        SemanticAssociationRelation::LabelToEndpoint);
    assert(result[1].target_id == "endpoint-1");
    assert(result[1].confidence == ConfidenceClass::High);
    assert(std::abs(result[1].distance - 0.0) < 1e-9);

    return 0;
}

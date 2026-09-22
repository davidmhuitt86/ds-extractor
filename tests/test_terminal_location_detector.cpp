#include "eke_dx_wire/topology/terminal_location_detector.hpp"

#include <cassert>
#include <iostream>

using namespace eke::dx::wire;

int main() {
    ComponentCandidate component;
    component.id = "component-1";
    component.kind = ComponentCandidateKind::Enclosure;
    component.bounds = BoundingBox{100, 100, 40, 30};

    EndpointCandidate near;
    near.id = "endpoint-near";
    near.position = Point2D{100.0, 115.0};

    EndpointCandidate far;
    far.id = "endpoint-far";
    far.position = Point2D{200.0, 200.0};

    TerminalLocationDetector detector;
    const auto result = detector.detect(
        {component},
        {near, far});

    assert(result.candidates.size() == 1);
    assert(result.candidates.front().endpoint_id == "endpoint-near");
    assert(result.candidates.front().component_candidate_id == "component-1");
    assert(result.candidates.front().kind ==
        TerminalCandidateKind::ComponentBoundary);
    assert(result.candidates.front().confidence ==
        ConfidenceClass::High);

    ComponentCandidate ground;
    ground.id = "ground-1";
    ground.kind = ComponentCandidateKind::ChassisGround;
    ground.bounds = BoundingBox{200, 100, 20, 20};

    EndpointCandidate ground_endpoint;
    ground_endpoint.id = "endpoint-ground";
    ground_endpoint.position = Point2D{210.0, 100.0};

    const auto ground_result = detector.detect(
        {ground},
        {ground_endpoint});

    assert(ground_result.candidates.size() == 1);
    assert(ground_result.candidates.front().kind ==
        TerminalCandidateKind::GroundConnection);

    // AP-SEMANTIC-006 regression: a conductor endpoint represented inside
    // a detected component symbol is still a terminal attachment. The old
    // boundary-distance calculation rejected this when the endpoint was
    // more than 8 px from the outer rectangle.
    EndpointCandidate interior;
    interior.id = "endpoint-interior";
    interior.position = Point2D{120.0, 115.0};

    const auto interior_result = detector.detect(
        {component},
        {interior});

    assert(interior_result.candidates.size() == 1);
    assert(interior_result.candidates.front().endpoint_id ==
        "endpoint-interior");
    assert(interior_result.candidates.front().distance_to_component == 0.0);
    assert(interior_result.candidates.front().confidence ==
        ConfidenceClass::High);

    std::cout << "terminal location detector tests passed\n";
    return 0;
}

#include "eke_dx_wire/image/component_candidate_classifier.hpp"

#include <cassert>
#include <iostream>

using namespace eke::dx::wire;

int main() {
    ShapeDetectionArtifacts shapes;

    ShapeRegion enclosure;
    enclosure.id = "shape-1";
    enclosure.kind = ShapeKind::Rectangle;
    enclosure.role = ShapeRole::Enclosure;
    enclosure.bounds = BoundingBox{10, 20, 100, 50};
    enclosure.confidence = 0.95;
    shapes.regions.push_back(enclosure);

    ShapeRegion circle;
    circle.id = "shape-2";
    circle.kind = ShapeKind::Circle;
    circle.role = ShapeRole::Primitive;
    circle.bounds = BoundingBox{120, 20, 20, 20};
    circle.confidence = 0.80;
    shapes.regions.push_back(circle);

    ShapeRegion ground;
    ground.id = "shape-3";
    ground.kind = ShapeKind::ChassisGround;
    ground.role = ShapeRole::Exclusion;
    ground.bounds = BoundingBox{150, 20, 20, 25};
    ground.confidence = 0.70;
    shapes.regions.push_back(ground);

    ShapeRegion primitive;
    primitive.id = "shape-4";
    primitive.kind = ShapeKind::Rectangle;
    primitive.role = ShapeRole::Primitive;
    primitive.bounds = BoundingBox{180, 20, 20, 20};
    primitive.confidence = 0.40;
    shapes.regions.push_back(primitive);

    ComponentCandidateClassifier classifier;
    const auto candidates = classifier.classify(shapes);

    assert(candidates.size() == 4);
    assert(candidates[0].id == "component-candidate-shape-1");
    assert(candidates[0].kind == ComponentCandidateKind::Enclosure);
    assert(candidates[0].confidence == ConfidenceClass::High);

    assert(candidates[1].kind == ComponentCandidateKind::CircularSymbol);
    assert(candidates[1].confidence == ConfidenceClass::Medium);

    assert(candidates[2].kind == ComponentCandidateKind::ChassisGround);
    assert(candidates[2].confidence == ConfidenceClass::Medium);

    assert(candidates[3].kind == ComponentCandidateKind::PrimitiveSymbol);
    assert(candidates[3].confidence == ConfidenceClass::Low);

    std::cout << "component candidate classifier tests passed\n";
    return 0;
}

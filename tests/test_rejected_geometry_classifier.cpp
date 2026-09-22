#include "eke_dx_wire/image/rejected_geometry_classifier.hpp"

#include <cassert>
#include <vector>

using namespace eke::dx::wire;

int main() {
    RejectedGeometryEvidence component_line;
    component_line.id = "rejected-component";
    component_line.geometry = {{90, 110}, {99, 110}};

    RejectedGeometryEvidence text_line;
    text_line.id = "rejected-text";
    text_line.geometry = {{10, 10}, {18, 10}};

    RejectedGeometryEvidence unresolved;
    unresolved.id = "rejected-unresolved";
    unresolved.geometry = {{180, 180}, {190, 180}};

    ComponentCandidate component;
    component.id = "component-1";
    component.bounds = {100, 100, 30, 25};

    TextRegion text;
    text.id = "text-1";
    text.bounds = {10, 5, 40, 10};

    std::vector<RejectedGeometryEvidence> rejected{
        component_line, text_line, unresolved};

    RejectedGeometryClassifier classifier;
    classifier.classify(
        rejected,
        {component},
        {text});

    assert(
        rejected[0].classification ==
        RejectedGeometryClass::ComponentAssociated);
    assert(rejected[0].associated_object_id == "component-1");

    assert(
        rejected[1].classification ==
        RejectedGeometryClass::TextAssociated);
    assert(rejected[1].associated_object_id == "text-1");

    assert(
        rejected[2].classification ==
        RejectedGeometryClass::Unresolved);
    assert(rejected[2].associated_object_id.empty());

    return 0;
}

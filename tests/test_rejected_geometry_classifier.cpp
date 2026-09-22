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
    component.kind = ComponentCandidateKind::Enclosure;
    component.bounds = {100, 100, 30, 25};

    ComponentCandidate connector;
    connector.id = "connector-1";
    connector.kind = ComponentCandidateKind::PrimitiveSymbol;
    connector.bounds = {50, 100, 30, 25};

    RejectedGeometryEvidence connector_line;
    connector_line.id = "rejected-connector";
    connector_line.geometry = {{40, 112}, {90, 112}};

    TextRegion text;
    text.id = "text-1";
    text.bounds = {10, 5, 40, 10};

    std::vector<RejectedGeometryEvidence> rejected{
        component_line, text_line, unresolved};

    RejectedGeometryClassifier classifier;
    classifier.classify(
        rejected,
        {component, connector},
        {text});

    assert(
        rejected[0].classification ==
        RejectedGeometryClass::ComponentAssociated);
    assert(rejected[0].associated_object_id == "component-1");

    rejected.push_back(connector_line);
    classifier.classify(
        rejected,
        {component, connector},
        {text});
    assert(
        rejected.back().classification ==
        RejectedGeometryClass::ConnectorAssociated);
    assert(rejected.back().associated_object_id == "connector-1");

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

#include "eke_dx_wire/image/geometry_ownership_classifier.hpp"

#include <cassert>

using namespace eke::dx::wire;

static ConductorSegment segment(
    const char* id, Point2D a, Point2D b) {
    ConductorSegment result;
    result.id = id;
    result.geometry = {a, b};
    result.provenance.source_id = "fixture";
    return result;
}

int main() {
    ComponentCandidate component;
    component.id = "component-1";
    component.kind = ComponentCandidateKind::Enclosure;
    component.bounds = {40, 40, 20, 20};

    ComponentCandidate connector;
    connector.id = "connector-1";
    connector.kind = ComponentCandidateKind::PrimitiveSymbol;
    connector.bounds = {80, 40, 20, 20};

    TextRegion text;
    text.id = "text-1";
    text.bounds = {120, 40, 20, 10};

    const std::vector<ConductorSegment> candidates{
        // This line enters a component but only terminates at its boundary.
        // It must remain a conductor: endpoint proximity is not ownership.
        segment("terminal-wire", {20, 50}, {40, 50}),

        // Both ends are strictly inside the component: graphical ownership.
        segment("component-owned", {42, 50}, {58, 50}),

        // The line crosses the component completely. Bounding-box overlap
        // must not turn a crossing conductor into component-owned geometry.
        segment("component-crossing", {20, 55}, {70, 55}),

        // The line enters the component and ends internally. It is retained
        // as conductor evidence for downstream terminal interpretation.
        segment("component-entering", {20, 45}, {45, 45}),

        // Both ends are strictly inside the connector primitive.
        segment("connector-owned", {82, 50}, {98, 50}),

        // Crossing a connector is not graphical ownership.
        segment("connector-crossing", {70, 55}, {110, 55}),

        // Text-associated line-like geometry.
        segment("text-owned", {122, 45}, {138, 45}),

        // Ordinary conductor far from graphical objects.
        segment("wire", {10, 80}, {30, 80})
    };

    GeometryOwnershipConfig config;
    config.component_overlap_fraction = 0.50;
    config.text_overlap_fraction = 0.25;

    const auto result =
        GeometryOwnershipClassifier(config).classify(
            candidates,
            {component, connector},
            {text});

    assert(result.conductor_candidates.size() == 5);
    assert(result.rejected.size() == 3);

    assert(result.conductor_candidates[0].id == "terminal-wire");
    assert(result.conductor_candidates[1].id == "component-crossing");
    assert(result.conductor_candidates[2].id == "component-entering");
    assert(result.conductor_candidates[3].id == "connector-crossing");
    assert(result.conductor_candidates[4].id == "wire");

    assert(result.rejected[0].id == "component-owned");
    assert(result.rejected[0].classification ==
           RejectedGeometryClass::ComponentAssociated);
    assert(result.rejected[0].associated_object_id == "component-1");

    assert(result.rejected[1].id == "connector-owned");
    assert(result.rejected[1].classification ==
           RejectedGeometryClass::ConnectorAssociated);
    assert(result.rejected[1].associated_object_id == "connector-1");

    assert(result.rejected[2].id == "text-owned");
    assert(result.rejected[2].classification ==
           RejectedGeometryClass::TextAssociated);
    assert(result.rejected[2].associated_object_id == "text-1");

    return 0;
}

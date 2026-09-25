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

    // AP-DIAG-FIX-001: a component's own boundary/housing outline is drawn
    // ink, but not a conductor - it must be excluded even though it touches
    // neither the "strictly inside" nor the "overlap fraction" tests above
    // (both of which the original CDI Unit/battery/lamp-base defects fell
    // through, since a housing outline runs ALONG the boundary rather than
    // inside it, and the shape detector's own measured bounds commonly sit
    // a pixel or two inside the drawn stroke).
    ComponentCandidate housing;
    housing.id = "housing-1";
    housing.kind = ComponentCandidateKind::Enclosure;
    housing.bounds = {200, 200, 60, 40};

    // TEST A: all four sides of the housing's own outline must be rejected,
    // not reconstructed as conductors/wires.
    {
        const std::vector<ConductorSegment> boundary_candidates{
            segment("housing-top", {200, 200}, {260, 200}),
            segment("housing-right", {260, 200}, {260, 240}),
            segment("housing-bottom", {200, 240}, {260, 240}),
            segment("housing-left", {200, 200}, {200, 240})};

        const auto boundary_result =
            GeometryOwnershipClassifier(config).classify(
                boundary_candidates, {housing}, {});

        assert(boundary_result.conductor_candidates.empty());
        assert(boundary_result.rejected.size() == 4);
        for (const auto& evidence : boundary_result.rejected) {
            assert(evidence.classification ==
                   RejectedGeometryClass::ComponentAssociated);
            assert(evidence.associated_object_id == "housing-1");
            assert(evidence.reason ==
                   "graphical_object_ownership_component_boundary");
        }
    }

    // TEST B / TEST C: a genuine conductor perpendicular to the boundary,
    // entering from outside and terminating at (or passing through) it, is
    // a completely different shape from the boundary line itself (it runs
    // across the boundary's coordinate, not along it) and must remain
    // conductor evidence on both the entering and exiting sides.
    {
        const std::vector<ConductorSegment> lead_candidates{
            // Enters from above, ends exactly at the top edge (230,200).
            segment("lead-entering-top", {230, 170}, {230, 200}),
            // Exits from below, starting exactly at the bottom edge (230,240).
            segment("lead-exiting-bottom", {230, 240}, {230, 270})};

        const auto lead_result =
            GeometryOwnershipClassifier(config).classify(
                lead_candidates, {housing}, {});

        assert(lead_result.conductor_candidates.size() == 2);
        assert(lead_result.rejected.empty());
    }

    // TEST D: two adjacent components, each with their own boundary, plus a
    // real conductor spanning the gap between them. Neither housing's
    // boundary exclusion should affect the other, or the real conductor.
    {
        ComponentCandidate housing_a;
        housing_a.id = "housing-a";
        housing_a.kind = ComponentCandidateKind::Enclosure;
        housing_a.bounds = {300, 200, 30, 30};

        ComponentCandidate housing_b;
        housing_b.id = "housing-b";
        housing_b.kind = ComponentCandidateKind::Enclosure;
        housing_b.bounds = {345, 200, 30, 30};

        const std::vector<ConductorSegment> adjacent_candidates{
            segment("housing-a-right", {330, 200}, {330, 230}),
            segment("housing-b-left", {345, 200}, {345, 230}),
            // A real conductor bridging the gap between the two housings,
            // well clear of either one's top/bottom edges.
            segment("bridging-wire", {330, 215}, {345, 215})};

        const auto adjacent_result =
            GeometryOwnershipClassifier(config).classify(
                adjacent_candidates, {housing_a, housing_b}, {});

        assert(adjacent_result.conductor_candidates.size() == 1);
        assert(adjacent_result.conductor_candidates[0].id == "bridging-wire");
        assert(adjacent_result.rejected.size() == 2);
        for (const auto& evidence : adjacent_result.rejected) {
            assert(evidence.associated_object_id == "housing-a" ||
                   evidence.associated_object_id == "housing-b");
        }
    }

    // TEST E: a real, unrelated wire running close to a component's edge but
    // clearly outside the boundary-tracing pixel tolerance (6px, vs. the
    // 2px tolerance) must remain detectable - proximity alone is never
    // sufficient grounds for exclusion, only near-exact coincidence with the
    // established boundary line is.
    {
        const std::vector<ConductorSegment> nearby_candidates{
            segment("nearby-wire", {266, 195}, {266, 235})};

        const auto nearby_result =
            GeometryOwnershipClassifier(config).classify(
                nearby_candidates, {housing}, {});

        assert(nearby_result.conductor_candidates.size() == 1);
        assert(nearby_result.rejected.empty());
    }

    return 0;
}

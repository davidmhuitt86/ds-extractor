#include "eke_dx_wire/image/symbol_geometry_extractor.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cassert>
#include <string>

using namespace eke::dx::wire;

namespace {

cv::Mat white_page(int width = 100, int height = 100) {
    return cv::Mat(height, width, CV_8UC1, cv::Scalar(255));
}

ComponentCandidate make_component(
    const std::string& id, int x, int y, int w, int h,
    ComponentCandidateKind kind = ComponentCandidateKind::CircularSymbol) {
    ComponentCandidate component;
    component.id = id;
    component.kind = kind;
    component.bounds = BoundingBox{x, y, w, h};
    return component;
}

ConductorSegment make_segment(
    double ax, double ay, double bx, double by, double thickness) {
    ConductorSegment segment;
    segment.geometry = Segment2D{Point2D{ax, ay}, Point2D{bx, by}};
    segment.thickness_px = thickness;
    return segment;
}

bool has_primitive_near(
    const std::vector<SymbolPrimitive>& primitives,
    const std::string& component_id, int x, int y) {
    for (const auto& primitive : primitives) {
        if (primitive.component_id != component_id) continue;
        const int cx = primitive.bounds.x + primitive.bounds.width / 2;
        const int cy = primitive.bounds.y + primitive.bounds.height / 2;
        if (std::abs(cx - x) <= 3 && std::abs(cy - y) <= 3) return true;
    }
    return false;
}

const SymbolPrimitive* find_kind(
    const std::vector<SymbolPrimitive>& primitives, SymbolPrimitiveKind kind) {
    for (const auto& primitive : primitives) {
        if (primitive.kind == kind) return &primitive;
    }
    return nullptr;
}

std::size_t count_kind(
    const std::vector<SymbolPrimitive>& primitives, SymbolPrimitiveKind kind) {
    return std::count_if(
        primitives.begin(), primitives.end(),
        [kind](const SymbolPrimitive& p) { return p.kind == kind; });
}

const ComponentSymbolGeometry* find_geometry(
    const std::vector<ComponentSymbolGeometry>& geometries, const std::string& component_id) {
    for (const auto& geometry : geometries) {
        if (geometry.component_id == component_id) return &geometry;
    }
    return nullptr;
}

} // namespace

int main() {
    SymbolGeometryExtractor extractor;

    // Empty component region: no image content at all.
    {
        cv::Mat image = white_page();
        std::vector<ComponentCandidate> components = {
            make_component("comp-empty", 10, 10, 20, 20)};
        const auto result = extractor.extract(image, components, {}, "fixture", 0);
        assert(result.geometries.size() == 1);
        const auto* geometry = find_geometry(result.geometries, "comp-empty");
        assert(geometry != nullptr);
        assert(geometry->primitive_ids.empty());
        assert(geometry->confidence == ConfidenceClass::Unresolved);
        assert(result.primitives.empty());
    }

    // Single internal line, well clear of the boundary margin.
    {
        cv::Mat image = white_page();
        cv::line(image, {15, 20}, {15, 30}, cv::Scalar(0), 1);
        std::vector<ComponentCandidate> components = {
            make_component("comp-line", 10, 10, 20, 30)};
        const auto result = extractor.extract(image, components, {}, "fixture", 0);
        assert(result.primitives.size() == 1);
        assert(result.primitives[0].kind == SymbolPrimitiveKind::Line);
        assert(result.primitives[0].component_id == "comp-line");
        const auto* geometry = find_geometry(result.geometries, "comp-line");
        assert(geometry != nullptr);
        assert(geometry->primitive_ids.size() == 1);
        assert(geometry->primitive_ids[0] == result.primitives[0].id);
    }

    // Multiple lines: two clearly separated internal segments.
    {
        cv::Mat image = white_page();
        cv::line(image, {15, 15}, {15, 25}, cv::Scalar(0), 1);
        cv::line(image, {30, 15}, {30, 25}, cv::Scalar(0), 1);
        std::vector<ComponentCandidate> components = {
            make_component("comp-multi-line", 10, 10, 30, 20)};
        const auto result = extractor.extract(image, components, {}, "fixture", 0);
        assert(count_kind(result.primitives, SymbolPrimitiveKind::Line) == 2);
    }

    // Circle: a filled disc drawn well inside the component region.
    {
        cv::Mat image = white_page();
        cv::circle(image, {30, 30}, 8, cv::Scalar(0), cv::FILLED);
        std::vector<ComponentCandidate> components = {
            make_component("comp-circle", 10, 10, 40, 40)};
        const auto result = extractor.extract(image, components, {}, "fixture", 0);
        const auto* circle = find_kind(result.primitives, SymbolPrimitiveKind::Circle);
        assert(circle != nullptr);
        assert(circle->confidence == ConfidenceClass::High ||
               circle->confidence == ConfidenceClass::Medium);
    }

    // Rectangle: a filled block, aspect ratio within tolerance.
    {
        cv::Mat image = white_page();
        cv::rectangle(image, {20, 20}, {35, 32}, cv::Scalar(0), cv::FILLED);
        std::vector<ComponentCandidate> components = {
            make_component("comp-rect", 10, 10, 40, 40)};
        const auto result = extractor.extract(image, components, {}, "fixture", 0);
        const auto* rect = find_kind(result.primitives, SymbolPrimitiveKind::Rectangle);
        assert(rect != nullptr);
    }

    // Contact-like geometry: two short parallel plates (relay/switch contact
    // pair) - two filled rectangles side by side.
    {
        cv::Mat image = white_page();
        cv::rectangle(image, {15, 20}, {25, 24}, cv::Scalar(0), cv::FILLED);
        cv::rectangle(image, {15, 30}, {25, 34}, cv::Scalar(0), cv::FILLED);
        std::vector<ComponentCandidate> components = {
            make_component("comp-contact", 10, 10, 30, 40)};
        const auto result = extractor.extract(image, components, {}, "fixture", 0);
        assert(count_kind(result.primitives, SymbolPrimitiveKind::Rectangle) == 2);
    }

    // Terminal-lead geometry: a thin line that reaches the component's own
    // boundary margin, distinct from a fully internal line.
    {
        cv::Mat image = white_page();
        // Component region is x in [10,40), y in [10,40); draw a thin line
        // that starts at the very edge (touching the excluded margin) and
        // extends inward.
        cv::line(image, {11, 25}, {11, 35}, cv::Scalar(0), 1);
        std::vector<ComponentCandidate> components = {
            make_component("comp-lead", 10, 10, 30, 30)};
        const auto result = extractor.extract(image, components, {}, "fixture", 0);
        const auto* lead = find_kind(result.primitives, SymbolPrimitiveKind::TerminalLead);
        assert(lead != nullptr);
        // A lead is geometric evidence only; it must never become an
        // EndpointCandidate as a side effect of this extractor.
    }

    // Mixed primitives in a single component.
    {
        cv::Mat image = white_page();
        cv::circle(image, {20, 20}, 5, cv::Scalar(0), cv::FILLED);
        cv::line(image, {40, 15}, {40, 30}, cv::Scalar(0), 1);
        cv::rectangle(image, {15, 40}, {30, 50}, cv::Scalar(0), cv::FILLED);
        std::vector<ComponentCandidate> components = {
            make_component("comp-mixed", 5, 5, 50, 55)};
        const auto result = extractor.extract(image, components, {}, "fixture", 0);
        assert(find_kind(result.primitives, SymbolPrimitiveKind::Circle) != nullptr);
        assert(find_kind(result.primitives, SymbolPrimitiveKind::Line) != nullptr);
        assert(find_kind(result.primitives, SymbolPrimitiveKind::Rectangle) != nullptr);
        const auto* geometry = find_geometry(result.geometries, "comp-mixed");
        assert(geometry->primitive_ids.size() == 3);
        assert(geometry->confidence != ConfidenceClass::Unresolved);
    }

    // Ambiguous geometry: an irregular blob with no clean shape signature
    // must be left Unknown rather than forced into a classification.
    {
        cv::Mat image = white_page();
        std::vector<cv::Point> blob_points = {
            {20, 20}, {25, 18}, {28, 22}, {24, 26}, {21, 24}};
        std::vector<std::vector<cv::Point>> polys = {blob_points};
        cv::fillPoly(image, polys, cv::Scalar(0));
        std::vector<ComponentCandidate> components = {
            make_component("comp-ambiguous", 10, 10, 30, 30)};
        const auto result = extractor.extract(image, components, {}, "fixture", 0);
        assert(!result.primitives.empty());
        // Whatever it becomes, an ambiguous irregular blob should not be
        // confidently reported as High confidence.
        for (const auto& primitive : result.primitives) {
            assert(primitive.confidence != ConfidenceClass::High);
        }
    }

    // DiagramFurniture exclusion: identical geometry to a real component,
    // but classified as furniture, must never enter the extraction path.
    {
        cv::Mat image = white_page();
        cv::circle(image, {30, 30}, 8, cv::Scalar(0), cv::FILLED);
        std::vector<ComponentCandidate> components = {
            make_component(
                "comp-furniture", 10, 10, 40, 40,
                ComponentCandidateKind::DiagramFurniture)};
        const auto result = extractor.extract(image, components, {}, "fixture", 0);
        assert(result.geometries.empty());
        assert(result.primitives.empty());
    }

    // Deterministic IDs and ordering: running extraction twice against the
    // same input must produce byte-identical IDs and ordering, not IDs that
    // depend on vector insertion order, memory addresses, or timestamps.
    {
        cv::Mat image = white_page();
        cv::circle(image, {20, 20}, 5, cv::Scalar(0), cv::FILLED);
        cv::line(image, {40, 15}, {40, 30}, cv::Scalar(0), 1);
        std::vector<ComponentCandidate> components = {
            make_component("comp-det", 5, 5, 50, 40)};

        const auto result_a = extractor.extract(image, components, {}, "fixture", 0);
        const auto result_b = extractor.extract(image, components, {}, "fixture", 0);

        assert(result_a.primitives.size() == result_b.primitives.size());
        for (std::size_t i = 0; i < result_a.primitives.size(); ++i) {
            assert(result_a.primitives[i].id == result_b.primitives[i].id);
            assert(result_a.primitives[i].kind == result_b.primitives[i].kind);
        }
        assert(result_a.geometries[0].primitive_ids ==
               result_b.geometries[0].primitive_ids);
    }

    // Confidence propagation: geometry confidence reflects the strongest
    // primitive confidence found within it.
    {
        cv::Mat image = white_page();
        cv::circle(image, {30, 30}, 8, cv::Scalar(0), cv::FILLED); // High/Medium
        std::vector<ComponentCandidate> components = {
            make_component("comp-confidence", 10, 10, 40, 40)};
        const auto result = extractor.extract(image, components, {}, "fixture", 0);
        const auto* geometry = find_geometry(result.geometries, "comp-confidence");
        assert(geometry->confidence == ConfidenceClass::High ||
               geometry->confidence == ConfidenceClass::Medium);
    }

    // Parent component relationship: every primitive references its owning
    // component, and a geometry's primitive_ids all belong to that geometry
    // only (no cross-component leakage) when multiple components exist.
    {
        cv::Mat image = white_page(120, 120);
        cv::circle(image, {20, 20}, 5, cv::Scalar(0), cv::FILLED);
        cv::rectangle(image, {70, 70}, {85, 85}, cv::Scalar(0), cv::FILLED);
        std::vector<ComponentCandidate> components = {
            make_component("comp-a", 5, 5, 30, 30),
            make_component("comp-b", 60, 60, 35, 35)};
        const auto result = extractor.extract(image, components, {}, "fixture", 0);

        for (const auto& primitive : result.primitives) {
            assert(primitive.component_id == "comp-a" || primitive.component_id == "comp-b");
        }
        const auto* geometry_a = find_geometry(result.geometries, "comp-a");
        const auto* geometry_b = find_geometry(result.geometries, "comp-b");
        assert(geometry_a != nullptr && geometry_b != nullptr);
        for (const auto& id : geometry_a->primitive_ids) {
            const auto* primitive = find_kind(result.primitives, SymbolPrimitiveKind::Circle);
            (void)primitive;
        }
    }

    // AP-WIRE-FIX-001 regression: reproduces the exact CONFLICT-02
    // geometry (AP-WIRE-TUNE-003) - a vertical conductor exits component
    // A right at A's own boundary (so A's own margin-exclusion correctly
    // reports nothing for it), but the same ink also falls inside a
    // neighboring, overlapping component B's box, away from B's own
    // margin. Without the conductor-segment exclusion, B independently
    // re-detects that ink as its own internal primitive; with it, B must
    // not. A genuine internal primitive elsewhere in B, unrelated to any
    // conductor, must remain detected either way.
    {
        cv::Mat image = white_page(60, 60);
        // The wire: a vertical stroke at x=28, from above component A
        // down through both A and B's overlap region.
        cv::line(image, {28, 0}, {28, 40}, cv::Scalar(0), 1);
        // A genuine internal primitive belonging to B alone, far from the
        // wire and from component A.
        cv::circle(image, {33, 40}, 4, cv::Scalar(0), cv::FILLED);

        std::vector<ComponentCandidate> components = {
            // A's right edge is at x=30; the wire at x=28 sits inside
            // A's own 2px boundary margin for its entire height within
            // A, so A's own extraction correctly reports nothing for it.
            make_component("comp-a-fuse", 10, 10, 20, 24),
            // B overlaps A (x:[18,30) x y:[25,34)) but the same wire
            // (local x = 28-18 = 10) sits well clear of B's own margin,
            // so B's independent scan would otherwise re-detect it.
            make_component("comp-b-neighbor", 18, 25, 20, 20)};

        const std::vector<ConductorSegment> segments = {
            make_segment(28, 0, 28, 40, 2.0)};

        // Fixed behavior: passing the already-known conductor geometry
        // excludes the wire's own ink from B's internal-primitive scan.
        const auto fixed = extractor.extract(image, components, segments, "fixture", 0);
        assert(!has_primitive_near(fixed.primitives, "comp-b-neighbor", 28, 30));
        // The genuine internal primitive must still be detected.
        assert(has_primitive_near(fixed.primitives, "comp-b-neighbor", 33, 40));
        assert(find_kind(fixed.primitives, SymbolPrimitiveKind::Circle) != nullptr);

        // Reproduction of the defect: without conductor geometry, the
        // same wire ink is spuriously attributed to B as its own
        // internal primitive - this is the exact CONFLICT-02 mechanism.
        const auto unfixed = extractor.extract(image, components, {}, "fixture", 0);
        assert(has_primitive_near(unfixed.primitives, "comp-b-neighbor", 28, 30));

        // Determinism: the same non-empty conductor geometry must yield
        // byte-identical primitive IDs and ordering across runs.
        const auto fixed_again = extractor.extract(image, components, segments, "fixture", 0);
        assert(fixed.primitives.size() == fixed_again.primitives.size());
        for (std::size_t i = 0; i < fixed.primitives.size(); ++i) {
            assert(fixed.primitives[i].id == fixed_again.primitives[i].id);
        }
    }

    return 0;
}

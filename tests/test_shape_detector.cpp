#include "eke_dx_wire/image/shape_detector.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cstdint>
#include <cassert>

using namespace eke::dx::wire;

int main() {
    cv::Mat image(220, 320, CV_8UC1, cv::Scalar(255));

    cv::rectangle(image, {20, 20}, {80, 70}, cv::Scalar(0), 2);
    // A bare, empty rectangle is deliberately never classified as a
    // Rectangle candidate (detect_rectangles() requires actual interior
    // content as a discriminator against empty wire loops) - a small
    // mark inside represents a real component's internal content.
    cv::circle(image, {50, 45}, 4, cv::Scalar(0), cv::FILLED);
    cv::circle(image, {130, 50}, 18, cv::Scalar(0), 2);

    // Chassis-ground style: three centered horizontal bars with decreasing
    // widths, plus a short vertical connection above them.
    cv::line(image, {220, 40}, {220, 58}, cv::Scalar(0), 2);
    cv::line(image, {205, 60}, {235, 60}, cv::Scalar(0), 2);
    cv::line(image, {210, 67}, {230, 67}, cv::Scalar(0), 2);
    cv::line(image, {215, 74}, {225, 74}, cv::Scalar(0), 2);

    // AP-DIAG-FIX-003: this location used to hold a deliberate "low
    // quality" 2-bar-only fixture, intended to prove the (then-permitted)
    // degraded-scan fallback still recognized a ground symbol missing its
    // third bar. Forensic evidence (AP-DIAG-AUDIT-002,
    // docs/AP-DIAG-FIX-003_ChassisGround_Evidence_Classification.md) found
    // that exact fallback path was what let 7 of 8 confirmed false
    // positives through in the only real-world diagram exercised to date,
    // while neither of that diagram's two genuine ground symbols ever
    // needed it. ground_min_bars is now 3 - the same two bars are drawn
    // here, unchanged, specifically to prove this fixture is NOT
    // recognized as ChassisGround now, which the shape-region-count
    // assertions below the detect() call verify explicitly.
    cv::line(image, {280, 100}, {280, 114}, cv::Scalar(0), 2);
    cv::line(image, {268, 116}, {292, 116}, cv::Scalar(0), 2);
    cv::line(image, {273, 123}, {287, 123}, cv::Scalar(0), 2);

    const auto result =
        ShapeDetector().detect(image, "fixture", 0);

    assert(!result.exclusion_mask.empty());
    assert(result.exclusion_mask.type() == CV_8UC1);
    assert(result.regions.size() >= 3);

    const auto has_kind = [&](ShapeKind kind) {
        return std::any_of(
            result.regions.begin(), result.regions.end(),
            [&](const ShapeRegion& region) {
                return region.kind == kind;
            });
    };

    assert(has_kind(ShapeKind::Rectangle));
    assert(has_kind(ShapeKind::Circle));
    assert(has_kind(ShapeKind::ChassisGround));

    const auto ground_near = [&](int cx, int cy, int tol) {
        return std::any_of(
            result.regions.begin(), result.regions.end(),
            [&](const ShapeRegion& region) {
                if (region.kind != ShapeKind::ChassisGround) return false;
                const int bx = region.bounds.x + region.bounds.width / 2;
                const int by = region.bounds.y + region.bounds.height / 2;
                return std::abs(bx - cx) <= tol && std::abs(by - cy) <= tol;
            });
    };

    // The real 3-bar symbol at (220, ~60-74) is still recognized...
    assert(ground_near(220, 67, 15));
    // ...but the 2-bar-only fixture at (280, ~116-123) is not
    // (AP-DIAG-FIX-003 - see the comment above where it's drawn).
    assert(!ground_near(280, 119, 15));

    assert(result.exclusion_mask.at<std::uint8_t>(45, 50) == 255);
    // Circle regions are always added with ShapeRole::Primitive (see
    // detect_circles()), and detect()'s own exclusion-mask loop
    // deliberately skips ShapeRole::Primitive regions ("Primitive
    // symbols remain visible in SHAPES but do not erase the conductor
    // field") - a circle is therefore never part of the exclusion mask.
    assert(result.exclusion_mask.at<std::uint8_t>(50, 130) == 0);
    assert(result.exclusion_mask.at<std::uint8_t>(67, 220) == 255);

    const auto circle_near = [](
        const ShapeDetectionArtifacts& artifacts, int cx, int cy, int tol) {
        return std::any_of(
            artifacts.regions.begin(), artifacts.regions.end(),
            [&](const ShapeRegion& region) {
                if (region.kind != ShapeKind::Circle) return false;
                const int bx = region.bounds.x + region.bounds.width / 2;
                const int by = region.bounds.y + region.bounds.height / 2;
                return std::abs(bx - cx) <= tol && std::abs(by - cy) <= tol;
            });
    };

    // AP-WIRE-FIX-002 regression: reproduces the exact CONFLICT-01/03/04
    // geometry (AP-WIRE-TUNE-004) - two parallel bus conductors crossing
    // two parallel drop conductors, each extending well beyond the small
    // enclosed gap in every direction, exactly as on TRX300. Before the
    // fix this empty cell's contour clears every other circularity/
    // aspect/edge-support gate and is reported as a spurious
    // ShapeKind::Circle; the fix must reject it.
    {
        cv::Mat image(200, 200, CV_8UC1, cv::Scalar(255));
        cv::line(image, {50, 0}, {50, 150}, cv::Scalar(0), 2);
        cv::line(image, {70, 0}, {70, 150}, cv::Scalar(0), 2);
        cv::line(image, {0, 60}, {150, 60}, cv::Scalar(0), 2);
        cv::line(image, {0, 75}, {150, 75}, cv::Scalar(0), 2);

        const auto crossing_result =
            ShapeDetector().detect(image, "fixture", 0);

        assert(!circle_near(crossing_result, 60, 67, 10));
    }

    // A second, independent bus crossing (different spacing/position),
    // covering the case of more than one false candidate in one image -
    // TUNE-004 found CONFLICT-01 and CONFLICT-03 shared the exact same
    // crossing, and a fourth, previously unreported one turned up during
    // AP-WIRE-FIX-002's own regression sweep.
    {
        cv::Mat image(220, 220, CV_8UC1, cv::Scalar(255));
        cv::line(image, {140, 10}, {140, 200}, cv::Scalar(0), 2);
        cv::line(image, {155, 10}, {155, 200}, cv::Scalar(0), 2);
        cv::line(image, {100, 90}, {210, 90}, cv::Scalar(0), 2);
        cv::line(image, {100, 102}, {210, 102}, cv::Scalar(0), 2);

        const auto crossing_result =
            ShapeDetector().detect(image, "fixture", 0);

        assert(!circle_near(crossing_result, 147, 96, 10));
    }

    // Legitimate circle with a conductor lead touching its edge (the
    // realistic case: a wire attaching to a terminal, not passing
    // through and out the far side) must still be detected. Only one of
    // its four bounding-box sides shows any line continuation beyond the
    // box - the fix's "all four sides" requirement must not reject it.
    {
        cv::Mat image(200, 160, CV_8UC1, cv::Scalar(255));
        cv::circle(image, {80, 100}, 16, cv::Scalar(0), 2);
        cv::line(image, {0, 100}, {63, 100}, cv::Scalar(0), 2);

        const auto result_with_lead =
            ShapeDetector().detect(image, "fixture", 0);

        assert(circle_near(result_with_lead, 80, 100, 6));
    }

    // Determinism: running the crossing-detection path twice against the
    // same input must produce the same rejection outcome and the same
    // accepted-region count, not something order- or memory-dependent.
    {
        cv::Mat image(200, 200, CV_8UC1, cv::Scalar(255));
        cv::line(image, {50, 0}, {50, 150}, cv::Scalar(0), 2);
        cv::line(image, {70, 0}, {70, 150}, cv::Scalar(0), 2);
        cv::line(image, {0, 60}, {150, 60}, cv::Scalar(0), 2);
        cv::line(image, {0, 75}, {150, 75}, cv::Scalar(0), 2);
        cv::circle(image, {170, 170}, 12, cv::Scalar(0), 2);

        const auto first = ShapeDetector().detect(image, "fixture", 0);
        const auto second = ShapeDetector().detect(image, "fixture", 0);

        assert(first.regions.size() == second.regions.size());
        assert(!circle_near(first, 60, 67, 10));
        assert(!circle_near(second, 60, 67, 10));
        assert(circle_near(first, 170, 170, 6));
        assert(circle_near(second, 170, 170, 6));
    }

    // AP-DIAG-FIX-003 regression: a coincidental 3-bar decreasing-width
    // sequence with IRREGULAR spacing must still be rejected. This
    // reproduces the one confirmed false positive that survived requiring
    // 3 bars alone (AP-DIAG-AUDIT-002's "SWITCH" text-glyph instance,
    // whose accepted match had gaps of 11px then 3px - ratio 3.67 -
    // against both genuine symbols' gap ratios of 1.0 and 1.5).
    {
        cv::Mat image(160, 160, CV_8UC1, cv::Scalar(255));
        cv::line(image, {80, 20}, {80, 38}, cv::Scalar(0), 2);
        cv::line(image, {65, 40}, {95, 40}, cv::Scalar(0), 2);   // width 30
        // Large, irregular gap (11px) before the second bar - not the
        // tight, consistent rhythm a single drawn glyph has.
        cv::line(image, {72, 55}, {88, 55}, cv::Scalar(0), 2);  // width 16
        cv::line(image, {75, 60}, {85, 60}, cv::Scalar(0), 2);  // width 10

        const auto result = ShapeDetector().detect(image, "fixture", 0);

        const auto ground_near = [&](int cx, int cy, int tol) {
            return std::any_of(
                result.regions.begin(), result.regions.end(),
                [&](const ShapeRegion& region) {
                    if (region.kind != ShapeKind::ChassisGround) return false;
                    const int bx = region.bounds.x + region.bounds.width / 2;
                    const int by = region.bounds.y + region.bounds.height / 2;
                    return std::abs(bx - cx) <= tol && std::abs(by - cy) <= tol;
                });
        };

        assert(!ground_near(80, 45, 20));
    }

    // AP-DIAG-FIX-003 regression: a genuine 3-bar ground symbol must still
    // be recognized even when an entirely unrelated bar, from a distant
    // part of the same diagram, happens to share a similar center_x. This
    // reproduces the exact defect found in AP-DIAG-AUDIT-002's follow-up
    // forensic reconstruction: the initial x-tolerance grouping pass has
    // no Y-locality constraint of its own, so a stray bar over 100px away
    // could previously merge into the same candidate group purely via
    // center_x proximity, corrupting the sort-by-Y order and making the
    // real, closely-spaced 3-bar sequence unreachable. Splitting the
    // x-tolerance group into maximal Y-contiguous runs (this fix) keeps
    // the real symbol reachable regardless of what else shares its
    // column.
    {
        cv::Mat image(400, 200, CV_8UC1, cv::Scalar(255));

        // The genuine symbol: stem + 3 bars, decreasing width, tight
        // uniform spacing - same shape as the confirmed TRX300 battery
        // ground symbol's own bars (widths 18, 12, 5; gaps of 2, 2).
        cv::line(image, {100, 260}, {100, 278}, cv::Scalar(0), 2);
        cv::line(image, {91, 280}, {109, 280}, cv::Scalar(0), 3);  // width 18
        cv::line(image, {94, 285}, {106, 285}, cv::Scalar(0), 1);  // width 12
        cv::line(image, {97, 289}, {103, 289}, cv::Scalar(0), 1);  // width 5

        // A completely unrelated bar, over 150px away vertically, whose
        // rounded center_x happens to land within the existing +/-3px
        // x-tolerance used to group candidate bars in the same column.
        // It must not be able to attach itself to the real symbol's group
        // and disrupt it.
        cv::line(image, {97, 100}, {103, 100}, cv::Scalar(0), 2);  // width 5

        const auto result = ShapeDetector().detect(image, "fixture", 0);

        const auto ground_near = [&](int cx, int cy, int tol) {
            return std::any_of(
                result.regions.begin(), result.regions.end(),
                [&](const ShapeRegion& region) {
                    if (region.kind != ShapeKind::ChassisGround) return false;
                    const int bx = region.bounds.x + region.bounds.width / 2;
                    const int by = region.bounds.y + region.bounds.height / 2;
                    return std::abs(bx - cx) <= tol && std::abs(by - cy) <= tol;
                });
        };

        assert(ground_near(100, 284, 15));
    }

    // AP-DIAG-FIX-003 regression: the exclusion mask must cover only the
    // ground symbol's own bars (plus a small anti-aliasing margin), never
    // the wire approaching it from above. A prior version of this fix
    // masked the entire stem-search corridor (up to ground_stem_search_
    // height, 14px) and was found (AP-DIAG-AUDIT-002 follow-up) to erase
    // real approach-wire ink for symbols packed close to other
    // components, destroying otherwise-valid Wire objects. The corridor
    // above the bars must remain eligible for ordinary conductor
    // detection.
    {
        cv::Mat image(160, 160, CV_8UC1, cv::Scalar(255));
        // A long "approach wire" from something well above, terminating
        // at the symbol's bars - standing in for a switch/sensor lead.
        cv::line(image, {80, 20}, {80, 58}, cv::Scalar(0), 2);
        cv::line(image, {65, 60}, {95, 60}, cv::Scalar(0), 2);  // width 30
        cv::line(image, {70, 65}, {90, 65}, cv::Scalar(0), 2);  // width 20
        cv::line(image, {74, 69}, {86, 69}, cv::Scalar(0), 1);  // width 12

        const auto result = ShapeDetector().detect(image, "fixture", 0);
        assert(!result.exclusion_mask.empty());

        // A point well up the approach wire, comfortably inside where the
        // old 14px-tall stem-search corridor would have been masked, must
        // remain eligible (0 = not excluded).
        assert(result.exclusion_mask.at<std::uint8_t>(30, 80) == 0);
        // The bars themselves must still be excluded.
        assert(result.exclusion_mask.at<std::uint8_t>(60, 80) == 255);
    }

    return 0;
}

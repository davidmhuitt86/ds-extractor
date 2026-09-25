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

    // Low-quality variant: one of the three ground bars is missing.
    // The remaining two bars still have aligned centers, decreasing width,
    // and a conductor/stem above the upper bar.
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

    return 0;
}

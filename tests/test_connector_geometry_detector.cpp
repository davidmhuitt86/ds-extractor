#include "eke_dx_wire/image/connector_geometry_detector.hpp"

#include <opencv2/imgproc.hpp>

#include <cassert>
#include <iostream>

using namespace eke::dx::wire;

int main() {
    // Background/ink polarity matches the codebase-wide convention used by
    // ImageNormalizer's real output and every ShapeDetector fixture: a
    // light (255) background with dark (0) ink. ConnectorGeometryDetector's
    // own THRESH_BINARY_INV call assumes exactly this polarity (identical
    // to ShapeDetector's), so a fixture drawn with a black background and
    // white ink is thresholded backwards - the entire background becomes
    // "ink" and the drawn shape itself becomes a gap, corrupting every
    // contour found.
    cv::Mat image(120, 180, CV_8UC1, cv::Scalar(255));

    // Synthetic inline connector body: a concave/notched rectangle with a
    // conductor passing near its footprint. The conductor is drawn as two
    // short stubs just outside the body's own bounding box rather than one
    // continuous line through it: a single line spanning the full width
    // would cross directly over the notch side-walls at x=70 and x=105
    // (both within the notch's y=58-65 band), fusing the conductor's ink
    // with the body's own outline into one connected blob and destroying
    // the body's own closed contour - pass-through continuity evidence
    // does not require literally overlapping the body's own stroke
    // (ConnectorGeometryDetector's pass_through_continuity() already probes
    // a band extending past the bounding box, per its own header comment).
    const std::vector<cv::Point> body{
        {60, 45}, {115, 45}, {115, 58}, {105, 58},
        {105, 65}, {115, 65}, {115, 78}, {60, 78},
        {60, 65}, {70, 65}, {70, 58}, {60, 58}
    };
    cv::polylines(image, body, true, cv::Scalar(0), 2);
    cv::line(image, {35, 61}, {58, 61}, cv::Scalar(0), 2);
    cv::line(image, {117, 61}, {140, 61}, cv::Scalar(0), 2);

    ConnectorGeometryDetectorConfig config;
    config.max_area_ratio = 0.20;
    ConnectorGeometryDetector detector(config);
    const auto first = detector.detect(image, "synthetic", 0);
    const auto second = detector.detect(image, "synthetic", 0);

    assert(!first.regions.empty());
    assert(first.regions.size() == second.regions.size());
    assert(first.regions.front().id == second.regions.front().id);
    assert(first.regions.front().bounds.x >= 50);
    assert(first.regions.front().bounds.x <= 65);
    assert(first.regions.front().kind == ShapeKind::ConnectorBody);
    assert(first.regions.front().confidence >= 0.65);

    // Component-attached connector body: there is deliberately no
    // pass-through conductor. Recognition must still be geometry-driven.
    cv::Mat attached(120, 180, CV_8UC1, cv::Scalar(255));
    const std::vector<cv::Point> attached_body{
        {60, 45}, {115, 45}, {115, 58}, {105, 58},
        {105, 65}, {115, 65}, {115, 78}, {60, 78},
        {60, 65}, {70, 65}, {70, 58}, {60, 58}
    };
    cv::polylines(
        attached, attached_body, true, cv::Scalar(0), 2);

    const auto attached_result =
        detector.detect(attached, "synthetic-attached", 0);
    assert(!attached_result.regions.empty());
    assert(attached_result.regions.front().kind == ShapeKind::ConnectorBody);

    // A generic non-convex body without the connector-family notch must not
    // become a connector merely because it has multiple contour vertices.
    cv::Mat unrelated(120, 180, CV_8UC1, cv::Scalar(255));
    const std::vector<cv::Point> unrelated_body{
        {55, 45}, {115, 45}, {115, 75}, {90, 75},
        {90, 65}, {80, 65}, {80, 75}, {55, 75}
    };
    cv::polylines(
        unrelated, unrelated_body, true, cv::Scalar(0), 2);
    const auto unrelated_result =
        detector.detect(unrelated, "synthetic-unrelated", 0);
    assert(unrelated_result.regions.empty());

    // A single-sided indentation can generate two convexity defects in a
    // rasterized stroke. It must still be rejected because it does not form
    // the opposing-side connector profile.
    cv::Mat one_sided(120, 180, CV_8UC1, cv::Scalar(255));
    const std::vector<cv::Point> one_sided_body{
        {55, 45}, {115, 45}, {115, 58}, {105, 58},
        {105, 65}, {115, 65}, {115, 78}, {55, 78}
    };
    cv::polylines(
        one_sided, one_sided_body, true, cv::Scalar(0), 2);
    const auto one_sided_result =
        detector.detect(one_sided, "synthetic-one-sided", 0);
    assert(one_sided_result.regions.empty());

    // AP-DIAG-020: a body whose opposing notch pair sits on the top/bottom
    // axis must be rejected even though it satisfies the exact same
    // generic opposing-notch predicate (two significant convexity defects,
    // opposing sides) as the accepted left/right body above - this is the
    // literal transpose (x and y swapped) of that same accepted body, so
    // every other geometric property (area, notch depth, notch count,
    // fill ratio) is unchanged; only the axis differs. AP-DIAG-019
    // established this is exactly the axis real TRX300 false positives
    // (wire-color text labels, a diode symbol) share, as opposed to the
    // left/right axis both real TRX300 connectors share.
    cv::Mat top_bottom(120, 180, CV_8UC1, cv::Scalar(255));
    const std::vector<cv::Point> top_bottom_body{
        {45, 60}, {45, 115}, {58, 115}, {58, 105},
        {65, 105}, {65, 115}, {78, 115}, {78, 60},
        {65, 60}, {65, 70}, {58, 70}, {58, 60}
    };
    cv::polylines(
        top_bottom, top_bottom_body, true, cv::Scalar(0), 2);
    const auto top_bottom_result =
        detector.detect(top_bottom, "synthetic-top-bottom", 0);
    assert(top_bottom_result.regions.empty());

    // A second, structurally distinct top/bottom-notch silhouette: a
    // horizontal bar with a short tick/stub protruding above and another
    // below (the base of each stub forms a concave corner nearest the
    // bounding box's top or bottom edge respectively) - representative of
    // the general class of incidental ink (glyph strokes, a symbol's
    // apex) that bulges above/below a horizontal wire rather than forming
    // the TRX300 connector family's actual left/right interlock. Must
    // also be rejected.
    cv::Mat stub(120, 180, CV_8UC1, cv::Scalar(255));
    const std::vector<cv::Point> stub_body{
        {50, 58}, {85, 58}, {85, 45}, {95, 45},
        {95, 58}, {130, 58}, {130, 68}, {95, 68},
        {95, 81}, {85, 81}, {85, 68}, {50, 68}
    };
    cv::polylines(stub, stub_body, true, cv::Scalar(0), 2);
    const auto stub_result = detector.detect(stub, "synthetic-stub", 0);
    assert(stub_result.regions.empty());

    std::cout << "connector geometry detector tests passed\n";
    return 0;
}

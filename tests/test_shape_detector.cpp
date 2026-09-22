#include "eke_dx_wire/image/shape_detector.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cstdint>
#include <cassert>

using namespace eke::dx::wire;

int main() {
    cv::Mat image(220, 320, CV_8UC1, cv::Scalar(255));

    cv::rectangle(image, {20, 20}, {80, 70}, cv::Scalar(0), 2);
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
    assert(result.exclusion_mask.at<std::uint8_t>(50, 130) == 255);
    assert(result.exclusion_mask.at<std::uint8_t>(67, 220) == 255);

    return 0;
}

#include "eke_dx_wire/image/connector_geometry_detector.hpp"

#include <opencv2/imgproc.hpp>

#include <cassert>
#include <iostream>

using namespace eke::dx::wire;

int main() {
    cv::Mat image = cv::Mat::zeros(120, 180, CV_8UC1);

    // Synthetic inline connector body: a concave/notched rectangle with a
    // conductor running continuously through its footprint.
    const std::vector<cv::Point> body{
        {60, 45}, {115, 45}, {115, 58}, {105, 58},
        {105, 65}, {115, 65}, {115, 78}, {60, 78},
        {60, 65}, {70, 65}, {70, 58}, {60, 58}
    };
    cv::polylines(image, body, true, cv::Scalar(255), 2);
    cv::line(image, {35, 61}, {140, 61}, cv::Scalar(255), 2);

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
    cv::Mat attached = cv::Mat::zeros(120, 180, CV_8UC1);
    const std::vector<cv::Point> attached_body{
        {60, 45}, {115, 45}, {115, 58}, {105, 58},
        {105, 65}, {115, 65}, {115, 78}, {60, 78},
        {60, 65}, {70, 65}, {70, 58}, {60, 58}
    };
    cv::polylines(
        attached, attached_body, true, cv::Scalar(255), 2);

    const auto attached_result =
        detector.detect(attached, "synthetic-attached", 0);
    assert(!attached_result.regions.empty());
    assert(attached_result.regions.front().kind == ShapeKind::ConnectorBody);

    // A generic non-convex body without the connector-family notch must not
    // become a connector merely because it has multiple contour vertices.
    cv::Mat unrelated = cv::Mat::zeros(120, 180, CV_8UC1);
    const std::vector<cv::Point> unrelated_body{
        {55, 45}, {115, 45}, {115, 75}, {90, 75},
        {90, 65}, {80, 65}, {80, 75}, {55, 75}
    };
    cv::polylines(
        unrelated, unrelated_body, true, cv::Scalar(255), 2);
    const auto unrelated_result =
        detector.detect(unrelated, "synthetic-unrelated", 0);
    assert(unrelated_result.regions.empty());

    // A single-sided indentation can generate two convexity defects in a
    // rasterized stroke. It must still be rejected because it does not form
    // the opposing-side connector profile.
    cv::Mat one_sided = cv::Mat::zeros(120, 180, CV_8UC1);
    const std::vector<cv::Point> one_sided_body{
        {55, 45}, {115, 45}, {115, 58}, {105, 58},
        {105, 65}, {115, 65}, {115, 78}, {55, 78}
    };
    cv::polylines(
        one_sided, one_sided_body, true, cv::Scalar(255), 2);
    const auto one_sided_result =
        detector.detect(one_sided, "synthetic-one-sided", 0);
    assert(one_sided_result.regions.empty());

    std::cout << "connector geometry detector tests passed\n";
    return 0;
}

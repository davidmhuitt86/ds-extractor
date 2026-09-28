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

    ConnectorGeometryDetector detector;
    const auto first = detector.detect(image, "synthetic", 0);
    const auto second = detector.detect(image, "synthetic", 0);

    assert(!first.regions.empty());
    assert(first.regions.size() == second.regions.size());
    assert(first.regions.front().id == second.regions.front().id);
    assert(first.regions.front().bounds.x >= 50);
    assert(first.regions.front().bounds.x <= 65);
    assert(first.regions.front().kind == ShapeKind::ConnectorBody);
    assert(first.regions.front().confidence >= 0.65);

    std::cout << "connector geometry detector tests passed\n";
    return 0;
}

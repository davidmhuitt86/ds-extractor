#include "eke_dx_wire/image/text_region_detector.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cassert>
#include <iostream>

using namespace eke::dx::wire;

int main() {
    cv::Mat image(100, 240, CV_8UC1, cv::Scalar(255));
    cv::putText(
        image,
        "12V FEED",
        cv::Point(20, 45),
        cv::FONT_HERSHEY_SIMPLEX,
        0.7,
        cv::Scalar(0),
        1,
        cv::LINE_AA);

    TextDetectorConfig config;
    config.minimum_group_width = 8;

    TextRegionDetector detector(config);
    const auto artifacts = detector.detect(image, "fixture", 0);

    assert(!artifacts.regions.empty());

    bool has_excluded = false;
    for (const auto& region : artifacts.regions) {
        if (region.excluded_from_wire_detection) {
            has_excluded = true;
            break;
        }
    }

    assert(has_excluded);
    assert(!artifacts.exclusion_mask.empty());

    std::cout << "text region detector tests passed\n";
    return 0;
}

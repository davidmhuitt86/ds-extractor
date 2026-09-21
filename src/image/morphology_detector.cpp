#include "eke_dx_wire/image/morphology_detector.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <sstream>

namespace eke::dx::wire {

MorphologyWireDetector::MorphologyWireDetector(MorphologyConfig config)
    : config_(config) {}

DetectionArtifacts MorphologyWireDetector::detect(
    const cv::Mat& normalized,
    const std::string& source_id,
    int page) const {

    DetectionArtifacts result;

    cv::adaptiveThreshold(
        normalized, result.binary, 255,
        cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY_INV,
        config_.adaptive_block_size, config_.adaptive_c);

    const cv::Mat h_kernel = cv::getStructuringElement(
        cv::MORPH_RECT,
        cv::Size(config_.horizontal_kernel_length, 1));

    const cv::Mat v_kernel = cv::getStructuringElement(
        cv::MORPH_RECT,
        cv::Size(1, config_.vertical_kernel_length));

    cv::morphologyEx(result.binary, result.horizontal_mask, cv::MORPH_OPEN, h_kernel);
    cv::morphologyEx(result.binary, result.vertical_mask, cv::MORPH_OPEN, v_kernel);

    auto extract = [&](const cv::Mat& mask, bool horizontal) {
        cv::Mat labels, stats, centroids;
        const int count = cv::connectedComponentsWithStats(
            mask, labels, stats, centroids, 8, CV_32S);

        for (int i = 1; i < count; ++i) {
            const int x = stats.at<int>(i, cv::CC_STAT_LEFT);
            const int y = stats.at<int>(i, cv::CC_STAT_TOP);
            const int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
            const int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);
            const int area = stats.at<int>(i, cv::CC_STAT_AREA);
            const int major = horizontal ? w : h;

            if (area < config_.minimum_component_area ||
                major < config_.minimum_segment_length) {
                continue;
            }

            Segment2D geometry;
            if (horizontal) {
                const double cy = y + (h - 1) * 0.5;
                geometry = {{static_cast<double>(x), cy},
                            {static_cast<double>(x + w - 1), cy}};
            } else {
                const double cx = x + (w - 1) * 0.5;
                geometry = {{cx, static_cast<double>(y)},
                            {cx, static_cast<double>(y + h - 1)}};
            }

            std::ostringstream canonical;
            canonical << source_id << ":" << page << ":"
                      << geometry.a.x << "," << geometry.a.y << "-"
                      << geometry.b.x << "," << geometry.b.y;

            ConductorSegment segment;
            segment.id = stable_id("conductor-segment", canonical.str());
            segment.geometry = geometry;
            segment.thickness_px = horizontal ? h : w;
            segment.confidence = ConfidenceClass::Medium;
            segment.provenance.source_id = source_id;
            segment.provenance.page = page;
            segment.provenance.source_region = {x, y, w, h};
            segment.provenance.stage =
                horizontal ? "morphology.horizontal" : "morphology.vertical";

            result.conductor_segments.push_back(std::move(segment));
        }
    };

    extract(result.horizontal_mask, true);
    extract(result.vertical_mask, false);

    std::sort(result.conductor_segments.begin(),
              result.conductor_segments.end(),
              [](const ConductorSegment& a, const ConductorSegment& b) {
                  return a.id < b.id;
              });

    return result;
}

} // namespace eke::dx::wire

#include "eke_dx_wire/image/shape_detector.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace eke::dx::wire {
namespace {

BoundingBox to_box(const cv::Rect& r) {
    return {r.x, r.y, r.width, r.height};
}

void add_region(
    ShapeDetectionArtifacts& result,
    ShapeKind kind,
    const cv::Rect& bounds,
    double confidence,
    const std::string& source_id,
    int page) {

    if (bounds.width <= 0 || bounds.height <= 0)
        return;

    for (const auto& existing : result.regions) {
        const cv::Rect existing_rect(
            existing.bounds.x,
            existing.bounds.y,
            existing.bounds.width,
            existing.bounds.height);

        const cv::Rect intersection = existing_rect & bounds;
        const int overlap_area = intersection.area();
        const int smaller_area =
            (std::min)(existing_rect.area(), bounds.area());

        if (smaller_area > 0 &&
            static_cast<double>(overlap_area) /
                static_cast<double>(smaller_area) > 0.80) {
            return;
        }
    }

    std::ostringstream canonical;
    canonical << source_id << ":" << page << ":"
              << static_cast<int>(kind) << ":"
              << bounds.x << "," << bounds.y << ","
              << bounds.width << "," << bounds.height;

    ShapeRegion region;
    region.id = stable_id("shape-region", canonical.str());
    region.kind = kind;
    region.bounds = to_box(bounds);
    region.confidence = confidence;

    result.regions.push_back(std::move(region));
}

void detect_rectangles(
    const cv::Mat& binary,
    ShapeDetectionArtifacts& result,
    const ShapeDetectorConfig& config,
    const std::string& source_id,
    int page) {

    cv::Mat closed;
    const int k = (std::max)(3, config.contour_close_kernel | 1);
    cv::morphologyEx(
        binary, closed, cv::MORPH_CLOSE,
        cv::getStructuringElement(cv::MORPH_RECT, {k, k}));

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(
        closed, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    const double image_area =
        static_cast<double>(binary.cols) * binary.rows;

    for (const auto& contour : contours) {
        const double area = cv::contourArea(contour);
        if (area < config.rectangle_min_area)
            continue;

        const cv::Rect bounds = cv::boundingRect(contour);
        const double bounds_area =
            static_cast<double>(bounds.area());

        if (bounds_area <= 0.0 ||
            bounds_area > image_area * config.rectangle_max_area_ratio)
            continue;

        std::vector<cv::Point> polygon;
        cv::approxPolyDP(
            contour, polygon,
            config.rectangle_epsilon * cv::arcLength(contour, true),
            true);

        if (polygon.size() != 4 || !cv::isContourConvex(polygon))
            continue;

        const double fill_ratio = area / bounds_area;
        if (fill_ratio < config.rectangle_fill_ratio)
            continue;

        const double width = static_cast<double>(bounds.width);
        const double height = static_cast<double>(bounds.height);

        if (width < 8.0 || height < 8.0)
            continue;

        const double aspect = width / height;
        if (aspect < 0.08 || aspect > 12.0)
            continue;

        add_region(
            result, ShapeKind::Rectangle, bounds, 0.90,
            source_id, page);
    }
}

void detect_circles(
    const cv::Mat& normalized,
    ShapeDetectionArtifacts& result,
    const ShapeDetectorConfig& config,
    const std::string& source_id,
    int page) {

    cv::Mat blurred;
    cv::GaussianBlur(normalized, blurred, {5, 5}, 1.2);

    std::vector<cv::Vec3f> circles;
    cv::HoughCircles(
        blurred, circles, cv::HOUGH_GRADIENT,
        config.circle_dp,
        config.circle_min_dist,
        config.circle_param1,
        config.circle_param2,
        config.circle_min_radius,
        config.circle_max_radius);

    for (const auto& circle : circles) {
        const int x = static_cast<int>(std::lround(circle[0]));
        const int y = static_cast<int>(std::lround(circle[1]));
        const int r = static_cast<int>(std::lround(circle[2]));

        if (r <= 0)
            continue;

        const cv::Rect bounds(
            x - r - 1, y - r - 1,
            2 * r + 3, 2 * r + 3);

        const cv::Rect image_rect(
            0, 0, normalized.cols, normalized.rows);

        const cv::Rect clipped = bounds & image_rect;

        if (clipped.width < 2 || clipped.height < 2)
            continue;

        add_region(
            result, ShapeKind::Circle, clipped, 0.75,
            source_id, page);
    }
}

void detect_ground_symbols(
    const cv::Mat& binary,
    ShapeDetectionArtifacts& result,
    const ShapeDetectorConfig& config,
    const std::string& source_id,
    int page) {

    cv::Mat horizontal;
    const int kernel_length =
        (std::max)(3, config.ground_min_bar_length);

    cv::morphologyEx(
        binary, horizontal, cv::MORPH_OPEN,
        cv::getStructuringElement(
            cv::MORPH_RECT, {kernel_length, 1}));

    cv::Mat labels, stats, centroids;
    const int count = cv::connectedComponentsWithStats(
        horizontal, labels, stats, centroids, 8, CV_32S);

    struct Bar {
        cv::Rect bounds;
        int center_x;
    };

    std::vector<Bar> bars;

    for (int i = 1; i < count; ++i) {
        const int x = stats.at<int>(i, cv::CC_STAT_LEFT);
        const int y = stats.at<int>(i, cv::CC_STAT_TOP);
        const int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
        const int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);

        if (w < config.ground_min_bar_length ||
            w > config.ground_max_bar_length ||
            h > 5)
            continue;

        bars.push_back({{x, y, w, h}, x + w / 2});
    }

    std::sort(
        bars.begin(), bars.end(),
        [](const Bar& a, const Bar& b) {
            if (a.center_x != b.center_x)
                return a.center_x < b.center_x;
            return a.bounds.y < b.bounds.y;
        });

    for (std::size_t i = 0; i < bars.size(); ++i) {
        for (std::size_t j = i + 1; j < bars.size(); ++j) {
            if (bars[j].bounds.y - bars[i].bounds.y > config.ground_max_height)
                break;

            for (std::size_t k = j + 1; k < bars.size(); ++k) {
                const int y_span =
                    bars[k].bounds.y - bars[i].bounds.y;

                if (y_span > config.ground_max_height)
                    break;

                const double center_spread =
                    (std::max)({
                        std::abs(bars[i].center_x - bars[j].center_x),
                        std::abs(bars[i].center_x - bars[k].center_x),
                        std::abs(bars[j].center_x - bars[k].center_x)});

                if (center_spread > 6.0)
                    continue;

                const int w0 = bars[i].bounds.width;
                const int w1 = bars[j].bounds.width;
                const int w2 = bars[k].bounds.width;

                const bool descending =
                    (w0 >= w1 && w1 >= w2) ||
                    (w2 >= w1 && w1 >= w0);

                if (!descending)
                    continue;

                cv::Rect bounds = bars[i].bounds |
                                  bars[j].bounds |
                                  bars[k].bounds;

                bounds.x = (std::max)(0, bounds.x - 3);
                bounds.y = (std::max)(0, bounds.y - 5);
                bounds.width =
                    (std::min)(binary.cols - bounds.x, bounds.width + 6);
                bounds.height =
                    (std::min)(binary.rows - bounds.y, bounds.height + 10);

                add_region(
                    result, ShapeKind::ChassisGround,
                    bounds, 0.80, source_id, page);
            }
        }
    }
}

} // namespace

ShapeDetector::ShapeDetector(ShapeDetectorConfig config)
    : config_(config) {}

ShapeDetectionArtifacts ShapeDetector::detect(
    const cv::Mat& normalized,
    const std::string& source_id,
    int page) const {

    ShapeDetectionArtifacts result;
    if (normalized.empty())
        return result;

    cv::Mat binary;
    cv::threshold(
        normalized, binary, config_.contour_threshold,
        255, cv::THRESH_BINARY_INV);

    detect_rectangles(
        binary, result, config_, source_id, page);

    detect_circles(
        normalized, result, config_, source_id, page);

    detect_ground_symbols(
        binary, result, config_, source_id, page);

    result.exclusion_mask =
        cv::Mat::zeros(normalized.size(), CV_8UC1);

    for (const auto& region : result.regions) {
        const cv::Rect bounds(
            region.bounds.x,
            region.bounds.y,
            region.bounds.width,
            region.bounds.height);

        const cv::Rect clipped =
            bounds & cv::Rect(0, 0, normalized.cols, normalized.rows);

        if (clipped.empty())
            continue;

        cv::rectangle(
            result.exclusion_mask,
            clipped,
            cv::Scalar(255),
            cv::FILLED);
    }

    std::sort(
        result.regions.begin(),
        result.regions.end(),
        [](const ShapeRegion& a, const ShapeRegion& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

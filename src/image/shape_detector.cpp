#include "eke_dx_wire/image/shape_detector.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace eke::dx::wire {
namespace {

BoundingBox to_box(const cv::Rect& r) {
    return {r.x, r.y, r.width, r.height};
}

double region_density(const cv::Mat& binary, const cv::Rect& requested) {
    const cv::Rect image_rect(0, 0, binary.cols, binary.rows);
    const cv::Rect region = requested & image_rect;
    if (region.empty())
        return 0.0;

    return cv::mean(binary(region))[0] / 255.0;
}

double ring_density(
    const cv::Mat& binary,
    const cv::Rect& bounds,
    int thickness = 2) {

    const cv::Rect image_rect(0, 0, binary.cols, binary.rows);
    const cv::Rect clipped = bounds & image_rect;
    if (clipped.width <= 2 * thickness ||
        clipped.height <= 2 * thickness) {
        return 0.0;
    }

    const cv::Rect inner(
        clipped.x + thickness,
        clipped.y + thickness,
        clipped.width - 2 * thickness,
        clipped.height - 2 * thickness);

    const double outer_area = static_cast<double>(clipped.area());
    const double inner_area = static_cast<double>(inner.area());

    if (outer_area <= inner_area)
        return 0.0;

    const double outer_sum = cv::sum(binary(clipped))[0];
    const double inner_sum = cv::sum(binary(inner))[0];

    return (outer_sum - inner_sum) /
           ((outer_area - inner_area) * 255.0);
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
        if (bounds.width < config.rectangle_min_width ||
            bounds.height < config.rectangle_min_height) {
            continue;
        }

        const double bounds_area =
            static_cast<double>(bounds.area());

        if (bounds_area <= 0.0 ||
            bounds_area > image_area * config.rectangle_max_area_ratio) {
            continue;
        }

        std::vector<cv::Point> polygon;
        const double perimeter = cv::arcLength(contour, true);

        cv::approxPolyDP(
            contour, polygon,
            config.rectangle_epsilon * perimeter,
            true);

        if (polygon.size() != 4 || !cv::isContourConvex(polygon))
            continue;

        const double fill_ratio = area / bounds_area;
        if (fill_ratio < config.rectangle_fill_ratio)
            continue;

        const double expected_perimeter =
            2.0 * (bounds.width + bounds.height);

        if (expected_perimeter <= 0.0)
            continue;

        const double perimeter_ratio =
            perimeter / expected_perimeter;

        if (perimeter_ratio < config.rectangle_min_perimeter_ratio ||
            perimeter_ratio > config.rectangle_max_perimeter_ratio) {
            continue;
        }

        // A real schematic enclosure generally has a relatively quiet
        // interior and a stronger ink boundary. Dense wire/text clusters
        // tend to fail this contrast test.
        const int inset = (std::min)({
            3,
            bounds.width / 4,
            bounds.height / 4
        });

        if (inset < 1)
            continue;

        const cv::Rect interior(
            bounds.x + inset,
            bounds.y + inset,
            bounds.width - 2 * inset,
            bounds.height - 2 * inset);

        const double interior_density =
            region_density(binary, interior);
        const double border_density =
            ring_density(binary, bounds, inset);

        if (interior_density >
            config.rectangle_max_interior_ink_density) {
            continue;
        }

        if (border_density <
            config.rectangle_min_border_ink_density) {
            continue;
        }

        const double interior_score =
            1.0 - (std::min)(1.0, interior_density /
                config.rectangle_max_interior_ink_density);

        const double border_score =
            (std::min)(1.0, border_density /
                config.rectangle_min_border_ink_density);

        const double confidence =
            0.60 + 0.20 * interior_score + 0.20 * border_score;

        add_region(
            result, ShapeKind::Rectangle, bounds,
            (std::min)(0.99, confidence),
            source_id, page);
    }
}

double circle_edge_support(
    const cv::Mat& binary,
    int cx,
    int cy,
    int radius) {

    if (radius <= 0)
        return 0.0;

    const int samples = 72;
    int supported = 0;
    int valid = 0;

    for (int i = 0; i < samples; ++i) {
        const double angle =
            2.0 * CV_PI * static_cast<double>(i) / samples;

        const int x = cvRound(cx + radius * std::cos(angle));
        const int y = cvRound(cy + radius * std::sin(angle));

        if (x < 0 || y < 0 ||
            x >= binary.cols || y >= binary.rows) {
            continue;
        }

        ++valid;

        const int x0 = (std::max)(0, x - 1);
        const int y0 = (std::max)(0, y - 1);
        const int x1 = (std::min)(binary.cols, x + 2);
        const int y1 = (std::min)(binary.rows, y + 2);

        if (cv::countNonZero(binary(cv::Rect(
                x0, y0, x1 - x0, y1 - y0))) > 0) {
            ++supported;
        }
    }

    return valid > 0
        ? static_cast<double>(supported) / valid
        : 0.0;
}

void detect_circles(
    const cv::Mat& normalized,
    const cv::Mat& binary,
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

        if (r < config.circle_min_radius)
            continue;

        const cv::Rect bounds(
            x - r - 1, y - r - 1,
            2 * r + 3, 2 * r + 3);

        const cv::Rect image_rect(
            0, 0, normalized.cols, normalized.rows);

        const cv::Rect clipped = bounds & image_rect;

        if (clipped.width < 2 || clipped.height < 2)
            continue;

        const double edge_support =
            circle_edge_support(binary, x, y, r);

        if (edge_support < config.circle_min_edge_support)
            continue;

        const int inset = (std::max)(2, r / 3);
        const cv::Rect interior(
            x - inset, y - inset,
            2 * inset + 1, 2 * inset + 1);

        const double interior_density =
            region_density(binary, interior);

        if (interior_density >
            config.circle_max_interior_ink_density) {
            continue;
        }

        const double confidence =
            (std::min)(0.99,
                0.55 +
                0.25 * edge_support +
                0.20 * (1.0 - interior_density));

        add_region(
            result, ShapeKind::Circle, clipped,
            confidence, source_id, page);
    }
}

struct GroundBar {
    cv::Rect bounds;
    int center_x;
};

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

    std::vector<GroundBar> bars;

    for (int i = 1; i < count; ++i) {
        const int x = stats.at<int>(i, cv::CC_STAT_LEFT);
        const int y = stats.at<int>(i, cv::CC_STAT_TOP);
        const int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
        const int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);

        if (w < config.ground_min_bar_length ||
            w > config.ground_max_bar_length ||
            h > 5) {
            continue;
        }

        bars.push_back({{x, y, w, h}, x + w / 2});
    }

    std::sort(
        bars.begin(), bars.end(),
        [](const GroundBar& a, const GroundBar& b) {
            if (a.center_x != b.center_x)
                return a.center_x < b.center_x;
            return a.bounds.y < b.bounds.y;
        });

    for (std::size_t i = 0; i < bars.size(); ++i) {
        for (std::size_t j = i + 1; j < bars.size(); ++j) {
            const int gap1 =
                bars[j].bounds.y -
                (bars[i].bounds.y + bars[i].bounds.height);

            if (gap1 < config.ground_min_bar_spacing)
                continue;

            if (gap1 > config.ground_max_bar_spacing)
                break;

            for (std::size_t k = j + 1; k < bars.size(); ++k) {
                const int gap2 =
                    bars[k].bounds.y -
                    (bars[j].bounds.y + bars[j].bounds.height);

                if (gap2 < config.ground_min_bar_spacing)
                    continue;

                if (gap2 > config.ground_max_bar_spacing)
                    break;

                const double center_spread =
                    (std::max)({
                        std::abs(bars[i].center_x - bars[j].center_x),
                        std::abs(bars[i].center_x - bars[k].center_x),
                        std::abs(bars[j].center_x - bars[k].center_x)
                    });

                if (center_spread > 3.0)
                    continue;

                const double w0 =
                    static_cast<double>(bars[i].bounds.width);
                const double w1 =
                    static_cast<double>(bars[j].bounds.width);
                const double w2 =
                    static_cast<double>(bars[k].bounds.width);

                const double ratio01 = w1 / (std::max)(1.0, w0);
                const double ratio12 = w2 / (std::max)(1.0, w1);

                const bool decreasing =
                    w0 > w1 && w1 > w2 &&
                    ratio01 <= 1.0 - config.ground_width_ratio_tolerance &&
                    ratio12 <= 1.0 - config.ground_width_ratio_tolerance;

                if (!decreasing)
                    continue;

                // Require a short vertical stem immediately above the
                // widest bar. This eliminates most coincidental bar triples.
                const int stem_x = bars[i].center_x;
                const int stem_y0 =
                    (std::max)(0,
                        bars[i].bounds.y -
                        config.ground_stem_search_height);
                const int stem_y1 = bars[i].bounds.y;

                const int stem_width = 3;
                const int sx0 = (std::max)(0, stem_x - stem_width);
                const int sx1 =
                    (std::min)(binary.cols, stem_x + stem_width + 1);

                if (sx1 <= sx0 || stem_y1 <= stem_y0)
                    continue;

                const cv::Rect stem_region(
                    sx0, stem_y0,
                    sx1 - sx0, stem_y1 - stem_y0);

                const int stem_ink =
                    cv::countNonZero(binary(stem_region));

                if (stem_ink < 2)
                    continue;

                cv::Rect bounds =
                    bars[i].bounds |
                    bars[j].bounds |
                    bars[k].bounds;

                bounds.x = (std::max)(0, bounds.x - 3);
                bounds.y = (std::max)(
                    0, bounds.y - config.ground_stem_search_height);
                bounds.width =
                    (std::min)(
                        binary.cols - bounds.x,
                        bounds.width + 6);
                bounds.height =
                    (std::min)(
                        binary.rows - bounds.y,
                        bounds.height + 10);

                add_region(
                    result, ShapeKind::ChassisGround,
                    bounds, 0.90, source_id, page);
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
        normalized, binary, result, config_, source_id, page);

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
            bounds & cv::Rect(
                0, 0, normalized.cols, normalized.rows);

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

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
    const cv::Rect region = requested &
        cv::Rect(0, 0, binary.cols, binary.rows);
    if (region.empty())
        return 0.0;
    return cv::mean(binary(region))[0] / 255.0;
}

double ring_density(const cv::Mat& binary, const cv::Rect& bounds, int thickness) {
    const cv::Rect clipped = bounds &
        cv::Rect(0, 0, binary.cols, binary.rows);

    if (clipped.width <= 2 * thickness ||
        clipped.height <= 2 * thickness)
        return 0.0;

    const cv::Rect inner(
        clipped.x + thickness,
        clipped.y + thickness,
        clipped.width - 2 * thickness,
        clipped.height - 2 * thickness);

    const double outer_area = static_cast<double>(clipped.area());
    const double inner_area = static_cast<double>(inner.area());

    if (outer_area <= inner_area)
        return 0.0;

    return (cv::sum(binary(clipped))[0] -
            cv::sum(binary(inner))[0]) /
           ((outer_area - inner_area) * 255.0);
}

double side_support(
    const cv::Mat& binary,
    const cv::Point& a,
    const cv::Point& b,
    int band) {

    const int length = static_cast<int>(
        std::ceil(std::hypot(
            static_cast<double>(b.x - a.x),
            static_cast<double>(b.y - a.y))));

    if (length < 2)
        return 0.0;

    int supported = 0;
    int samples = 0;

    for (int i = 0; i <= length; ++i) {
        const double t = static_cast<double>(i) / length;
        const int x = cvRound(a.x + t * (b.x - a.x));
        const int y = cvRound(a.y + t * (b.y - a.y));

        const int x0 = (std::max)(0, x - band);
        const int y0 = (std::max)(0, y - band);
        const int x1 = (std::min)(binary.cols, x + band + 1);
        const int y1 = (std::min)(binary.rows, y + band + 1);

        if (x1 <= x0 || y1 <= y0)
            continue;

        ++samples;
        if (cv::countNonZero(
                binary(cv::Rect(x0, y0, x1 - x0, y1 - y0))) > 0) {
            ++supported;
        }
    }

    return samples > 0
        ? static_cast<double>(supported) / samples
        : 0.0;
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
        closed, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    const double image_area =
        static_cast<double>(binary.cols) * binary.rows;

    for (const auto& contour : contours) {
        const double area = cv::contourArea(contour);
        if (area < config.rectangle_min_area)
            continue;

        const cv::Rect bounds = cv::boundingRect(contour);

        if (bounds.width < config.rectangle_min_width ||
            bounds.height < config.rectangle_min_height)
            continue;

        const double bounds_area =
            static_cast<double>(bounds.area());

        if (bounds_area <= 0.0 ||
            bounds_area > image_area * config.rectangle_max_area_ratio)
            continue;

        const double perimeter = cv::arcLength(contour, true);
        if (perimeter <= 0.0)
            continue;

        std::vector<cv::Point> polygon;
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
        const double perimeter_ratio =
            perimeter / expected_perimeter;

        if (perimeter_ratio < config.rectangle_min_perimeter_ratio ||
            perimeter_ratio > config.rectangle_max_perimeter_ratio)
            continue;

        // Validate the four actual enclosure sides. A contour created by
        // crossing wires or text often has a rectangular bounding box but
        // does not have continuous support on all four sides.
        const cv::Point tl(bounds.x, bounds.y);
        const cv::Point tr(bounds.x + bounds.width - 1, bounds.y);
        const cv::Point br(
            bounds.x + bounds.width - 1,
            bounds.y + bounds.height - 1);
        const cv::Point bl(bounds.x, bounds.y + bounds.height - 1);

        const double top = side_support(
            binary, tl, tr, 2);
        const double right = side_support(
            binary, tr, br, 2);
        const double bottom = side_support(
            binary, bl, br, 2);
        const double left = side_support(
            binary, tl, bl, 2);

        const double minimum_side =
            (std::min)({top, right, bottom, left});

        if (minimum_side < 0.65)
            continue;

        const int inset =
            (std::min)({3, bounds.width / 4, bounds.height / 4});

        if (inset < 1)
            continue;

        const cv::Rect interior(
            bounds.x + inset,
            bounds.y + inset,
            bounds.width - 2 * inset,
            bounds.height - 2 * inset);

        const double interior_density =
            region_density(binary, interior);

        if (interior_density > config.rectangle_max_interior_ink_density ||
            interior_density < config.rectangle_min_interior_ink_density)
            continue;

        // Require actual disconnected content inside the enclosure. This
        // is a strong discriminator against empty rectangular wire loops:
        // component labels/symbols normally leave at least one compact
        // interior connected component.
        cv::Mat interior_image = binary(interior);
        cv::Mat interior_labels;
        cv::Mat interior_stats;
        cv::Mat interior_centroids;
        const int interior_count = cv::connectedComponentsWithStats(
            interior_image,
            interior_labels,
            interior_stats,
            interior_centroids,
            8,
            CV_32S);

        int isolated_components = 0;
        for (int component = 1; component < interior_count; ++component) {
            const int cx = interior_stats.at<int>(
                component, cv::CC_STAT_LEFT);
            const int cy = interior_stats.at<int>(
                component, cv::CC_STAT_TOP);
            const int cw = interior_stats.at<int>(
                component, cv::CC_STAT_WIDTH);
            const int ch = interior_stats.at<int>(
                component, cv::CC_STAT_HEIGHT);
            const int component_area = interior_stats.at<int>(
                component, cv::CC_STAT_AREA);

            if (component_area < 2 ||
                cx <= 0 ||
                cy <= 0 ||
                cx + cw >= interior_image.cols ||
                cy + ch >= interior_image.rows)
                continue;

            const int major = (std::max)(cw, ch);
            const int minor = (std::max)(1, (std::min)(cw, ch));

            if (static_cast<double>(major) / minor >
                config.rectangle_max_interior_component_aspect)
                continue;

            ++isolated_components;
        }

        if (isolated_components <
            config.rectangle_min_interior_components)
            continue;

        // Long internal horizontal/vertical structures are characteristic
        // of wire fields and table/grid regions rather than clean component
        // enclosures. Reject candidates dominated by such structures.
        cv::Mat hline;
        cv::Mat vline;

        cv::morphologyEx(
            interior_image, hline, cv::MORPH_OPEN,
            cv::getStructuringElement(
                cv::MORPH_RECT, {9, 1}));

        cv::morphologyEx(
            interior_image, vline, cv::MORPH_OPEN,
            cv::getStructuringElement(
                cv::MORPH_RECT, {1, 9}));

        const double internal_line_density =
            static_cast<double>(
                cv::countNonZero(hline) +
                cv::countNonZero(vline)) /
            (2.0 * static_cast<double>(interior.area()));

        if (internal_line_density > 0.08)
            continue;

        const double confidence =
            (std::min)(
                0.99,
                0.55 +
                0.25 * minimum_side +
                0.10 * (1.0 - interior_density) +
                0.05 * (1.0 - internal_line_density) +
                0.10 * (std::min)(
                    1.0,
                    static_cast<double>(isolated_components) /
                    (std::max)(1, config.rectangle_min_interior_components)));

        add_region(
            result, ShapeKind::Rectangle, bounds,
            confidence, source_id, page);
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
            x >= binary.cols || y >= binary.rows)
            continue;

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
            config.circle_max_interior_ink_density)
            continue;

        const double confidence =
            (std::min)(
                0.99,
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
            h > 5)
            continue;

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

                const int stem_x = bars[i].center_x;
                const int stem_y0 =
                    (std::max)(
                        0,
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

                if (cv::countNonZero(binary(stem_region)) < 2)
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

#include "eke_dx_wire/image/connector_geometry_detector.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <cmath>
#include <sstream>

namespace eke::dx::wire {
namespace {

BoundingBox to_box(const cv::Rect& r) {
    return {r.x, r.y, r.width, r.height};
}

double row_continuity(
    const cv::Mat& binary,
    int y,
    int x0,
    int x1,
    int band) {

    int total = 0;
    int supported = 0;
    for (int x = x0; x < x1; ++x) {
        if (x < 0 || x >= binary.cols)
            continue;
        ++total;
        bool found = false;
        for (int dy = -band; dy <= band && !found; ++dy) {
            const int yy = y + dy;
            if (yy >= 0 && yy < binary.rows &&
                binary.at<std::uint8_t>(yy, x) != 0) {
                found = true;
            }
        }
        if (found)
            ++supported;
    }
    return total > 0 ? static_cast<double>(supported) / total : 0.0;
}

double column_continuity(
    const cv::Mat& binary,
    int x,
    int y0,
    int y1,
    int band) {

    int total = 0;
    int supported = 0;
    for (int y = y0; y < y1; ++y) {
        if (y < 0 || y >= binary.rows)
            continue;
        ++total;
        bool found = false;
        for (int dx = -band; dx <= band && !found; ++dx) {
            const int xx = x + dx;
            if (xx >= 0 && xx < binary.cols &&
                binary.at<std::uint8_t>(y, xx) != 0) {
                found = true;
            }
        }
        if (found)
            ++supported;
    }
    return total > 0 ? static_cast<double>(supported) / total : 0.0;
}

double pass_through_continuity(
    const cv::Mat& binary,
    const cv::Rect& r,
    const ConnectorGeometryDetectorConfig& config) {

    const int cx = r.x + r.width / 2;
    const int cy = r.y + r.height / 2;
    const int probe = config.pass_through_probe;

    const double horizontal = row_continuity(
        binary, cy, r.x - probe, r.x + r.width + probe,
        config.pass_through_band);
    const double vertical = column_continuity(
        binary, cx, r.y - probe, r.y + r.height + probe,
        config.pass_through_band);

    return (std::max)(horizontal, vertical);
}

} // namespace

ConnectorGeometryDetector::ConnectorGeometryDetector(
    ConnectorGeometryDetectorConfig config)
    : config_(std::move(config)) {}

ConnectorGeometryDetectionArtifacts ConnectorGeometryDetector::detect(
    const cv::Mat& normalized,
    const std::string& source_id,
    int page) const {

    ConnectorGeometryDetectionArtifacts result;
    if (normalized.empty())
        return result;

    cv::Mat gray;
    if (normalized.channels() == 1)
        gray = normalized;
    else
        cv::cvtColor(normalized, gray, cv::COLOR_BGR2GRAY);

    cv::Mat binary;
    cv::threshold(
        gray, binary, config_.threshold, 255, cv::THRESH_BINARY_INV);

    const int kernel = (std::max)(3, config_.close_kernel | 1);
    if (kernel > 1) {
        cv::morphologyEx(
            binary, binary, cv::MORPH_CLOSE,
            cv::getStructuringElement(
                cv::MORPH_RECT, {kernel, kernel}));
    }

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(
        binary, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

    const double image_area =
        static_cast<double>(binary.cols) * binary.rows;

    for (const auto& contour : contours) {
        const double area = cv::contourArea(contour);
        if (area < config_.min_area)
            continue;

        const cv::Rect bounds = cv::boundingRect(contour);
        if (bounds.width < config_.min_width ||
            bounds.height < config_.min_height ||
            bounds.width > config_.max_width ||
            bounds.height > config_.max_height)
            continue;

        const double bounds_area =
            static_cast<double>(bounds.area());
        if (bounds_area <= 0.0 ||
            bounds_area > image_area * config_.max_area_ratio)
            continue;

        const double aspect =
            static_cast<double>(bounds.width) / bounds.height;
        if (aspect < config_.min_aspect_ratio ||
            aspect > config_.max_aspect_ratio)
            continue;

        const double fill = area / bounds_area;
        if (fill < config_.min_fill_ratio ||
            fill > config_.max_fill_ratio)
            continue;

        const double perimeter = cv::arcLength(contour, true);
        if (perimeter <= 0.0)
            continue;

        std::vector<cv::Point> polygon;
        cv::approxPolyDP(
            contour, polygon,
            config_.polygon_epsilon * perimeter,
            true);

        if (polygon.size() < 4 ||
            static_cast<int>(polygon.size()) >
                config_.max_polygon_vertices) {
            continue;
        }

        const bool non_convex =
            polygon.size() >= 4 && !cv::isContourConvex(polygon);

        bool interior_void = false;
        cv::Mat local_mask = cv::Mat::zeros(bounds.size(), CV_8UC1);
        std::vector<cv::Point> shifted;
        shifted.reserve(contour.size());
        for (const auto& p : contour)
            shifted.push_back({p.x - bounds.x, p.y - bounds.y});
        cv::drawContours(
            local_mask,
            std::vector<std::vector<cv::Point>>{shifted},
            0, cv::Scalar(255), cv::FILLED);

        const int border = 2;
        if (bounds.width > 2 * border && bounds.height > 2 * border) {
            const cv::Rect inner(
                border, border,
                bounds.width - 2 * border,
                bounds.height - 2 * border);
            const double inner_fill =
                cv::mean(local_mask(inner))[0] / 255.0;
            interior_void = inner_fill < 0.92;
        }

        const double continuity =
            pass_through_continuity(binary, bounds, config_);

        // Connector identity comes from the characteristic connector-body
        // geometry, not from a mandatory pass-through conductor. The TRX300
        // contains both inline connectors and connectors physically attached
        // to modules/components, where conductors terminate rather than pass
        // through the connector body.
        //
        // Continuity remains evidence and contributes to confidence, but it
        // is no longer a hard recognition gate. Terminal/interaction
        // classification is performed downstream from independent conductor
        // and component evidence.
        const bool body_evidence =
            non_convex || interior_void || polygon.size() > 4;
        if (!body_evidence) {
            continue;
        }

        std::ostringstream key;
        key << source_id << ":" << page << ":"
            << bounds.x << "," << bounds.y << ","
            << bounds.width << "," << bounds.height << ":"
            << polygon.size();

        ShapeRegion region;
        region.id = stable_id("connector-body", key.str());
        region.kind = ShapeKind::ConnectorBody;
        region.role = ShapeRole::Primitive;
        region.bounds = to_box(bounds);
        region.confidence =
            (std::min)(
                0.99,
                0.45 +
                0.25 * continuity +
                0.15 * (non_convex ? 1.0 : 0.0) +
                0.10 * (interior_void ? 1.0 : 0.0));

        bool duplicate = false;
        for (const auto& existing : result.regions) {
            const cv::Rect e(
                existing.bounds.x, existing.bounds.y,
                existing.bounds.width, existing.bounds.height);
            const int overlap = (e & bounds).area();
            const int smaller = (std::min)(e.area(), bounds.area());
            if (smaller > 0 &&
                static_cast<double>(overlap) / smaller > 0.80) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            result.regions.push_back(std::move(region));
    }

    std::sort(
        result.regions.begin(), result.regions.end(),
        [](const ShapeRegion& a,
           const ShapeRegion& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

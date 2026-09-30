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

        // AP-DIAG-FIX-011: the TRX300 connector family has a characteristic
        // notch/interlock. Generic non-convexity is insufficient because
        // merged wire crossings and unrelated symbol geometry can also form
        // non-convex contours. Convexity defects provide a geometry-local
        // measure of actual inward notch depth without requiring pass-through
        // continuity.
        std::vector<int> hull_indices;
        cv::convexHull(contour, hull_indices, false, false);
        std::vector<cv::Vec4i> defects;
        if (hull_indices.size() >= 4 && contour.size() >= 4) {
            cv::convexityDefects(contour, hull_indices, defects);
        }

        int significant_notches = 0;
        double deepest_notch = 0.0;
        bool notch_left = false;
        bool notch_right = false;

        for (const auto& defect : defects) {
            // OpenCV stores defect depth in fixed-point units (1/256 px).
            const double depth = static_cast<double>(defect[3]) / 256.0;
            if (depth < config_.min_notch_depth)
                continue;

            ++significant_notches;
            deepest_notch = (std::max)(deepest_notch, depth);

            // A TRX300 connector body uses an interlocking/notched profile
            // on opposing sides. Classify each significant defect by the
            // location of its farthest contour point relative to the body.
            // This rejects one-sided generic concavities that can generate
            // multiple OpenCV convexity defects around a single indentation.
            const cv::Point far_point = contour[defect[2]];
            const double left_distance =
                static_cast<double>(far_point.x - bounds.x);
            const double right_distance =
                static_cast<double>(bounds.x + bounds.width - far_point.x);
            const double top_distance =
                static_cast<double>(far_point.y - bounds.y);
            const double bottom_distance =
                static_cast<double>(bounds.y + bounds.height - far_point.y);

            const double nearest = (std::min)(
                (std::min)(left_distance, right_distance),
                (std::min)(top_distance, bottom_distance));

            // AP-DIAG-020: only a defect whose nearest side is left or
            // right is evidence toward the TRX300 connector family's
            // actual notch/interlock axis (AP-DIAG-019: 7/7, zero
            // exceptions). A defect nearest the top or bottom is still
            // excluded from notch_left/notch_right here rather than
            // mis-attributed to whichever of left/right happens to be
            // closer - it is simply not opposing-notch evidence.
            if (nearest == left_distance)
                notch_left = true;
            else if (nearest == right_distance)
                notch_right = true;
        }

        // AP-DIAG-020: AP-DIAG-019 measured this directly against the real
        // TRX300 source (7/7 connector candidates, zero exceptions): both
        // genuine connector bodies show their opposing notch pair
        // exclusively on the left/right axis, while every false positive
        // (wire-color text glyphs and a diode symbol sitting on a
        // horizontal wire, plus one lower-confidence case) shows its
        // opposing pair exclusively on the top/bottom axis - ink that
        // happens to bulge above/below a horizontal baseline, not the
        // TRX300 connector family's actual notch/interlock profile. The
        // top/bottom axis is therefore not accepted as equivalent evidence.
        const bool opposing_notches = notch_left && notch_right;

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
        const bool notch_evidence =
            significant_notches >= config_.min_notch_count &&
            opposing_notches;

        // Connector identity requires a characteristic body feature.
        // The TRX300 family presents opposing interlocking/notched sides;
        // a single-sided concavity is insufficient because ordinary symbols
        // and merged wire geometry can produce multiple defects around one
        // indentation. A contour being merely non-convex, hollow, or
        // multi-vertex is not sufficient because those properties occur
        // throughout the wiring diagram.
        const bool body_evidence = notch_evidence;
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
                0.15 * (notch_evidence ? 1.0 : 0.0) +
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

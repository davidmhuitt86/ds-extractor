#include "eke_dx_wire/image/shape_detector.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
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
    ShapeRole role,
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
              << static_cast<int>(role) << ":"
              << bounds.x << "," << bounds.y << ","
              << bounds.width << "," << bounds.height;

    ShapeRegion region;
    region.id = stable_id("shape-region", canonical.str());
    region.kind = kind;
    region.role = role;
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

        const bool enclosure =
            static_cast<double>(bounds.area()) >=
                config.rectangle_min_exclusion_area &&
            bounds.width >= config.rectangle_min_exclusion_width &&
            bounds.height >= config.rectangle_min_exclusion_height;

        add_region(
            result,
            ShapeKind::Rectangle,
            enclosure ? ShapeRole::Enclosure : ShapeRole::Primitive,
            bounds,
            confidence,
            source_id,
            page);
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

// Fraction of columns in [x0, x1) at row `y` where ink (within `binary`)
// is present somewhere in a band of half-thickness `half_thickness`
// centered on that row. Used to test whether a bounding-box side is
// itself a short piece of a much longer straight line.
double row_line_continuity(
    const cv::Mat& binary, int y, int x0, int x1, int half_thickness) {

    int total = 0;
    int supported = 0;
    for (int x = x0; x < x1; ++x) {
        if (x < 0 || x >= binary.cols)
            continue;
        ++total;
        bool found = false;
        for (int t = -half_thickness; t <= half_thickness && !found; ++t) {
            const int yy = y + t;
            if (yy >= 0 && yy < binary.rows &&
                binary.at<std::uint8_t>(yy, x) != 0)
                found = true;
        }
        if (found)
            ++supported;
    }
    return total > 0 ? static_cast<double>(supported) / total : 0.0;
}

// Column analogue of row_line_continuity.
double col_line_continuity(
    const cv::Mat& binary, int x, int y0, int y1, int half_thickness) {

    int total = 0;
    int supported = 0;
    for (int y = y0; y < y1; ++y) {
        if (y < 0 || y >= binary.rows)
            continue;
        ++total;
        bool found = false;
        for (int t = -half_thickness; t <= half_thickness && !found; ++t) {
            const int xx = x + t;
            if (xx >= 0 && xx < binary.cols &&
                binary.at<std::uint8_t>(y, xx) != 0)
                found = true;
        }
        if (found)
            ++supported;
    }
    return total > 0 ? static_cast<double>(supported) / total : 0.0;
}

// AP-WIRE-FIX-002: distinguishes a small enclosed gap between two pairs
// of crossing straight conductors (a bus crossing drop wires, or a
// table/legend grid) from a genuinely drawn circular symbol. A crossing
// gap's bounding box is literally defined by the crossing lines
// themselves, so every one of its four sides continues as a thin
// straight line for a long distance beyond the box in both directions.
// A real drawn circle's boundary is self-contained ink that does not do
// this on all four sides at once - even when the circle sits inside a
// bordered table cell, at least one side is the circle's own curved
// stroke, not a continuing straight border. Only ink already excluded
// as the shape's own boundary-adjacent evidence is being consulted here
// (the same normalized image the rest of this function already uses) -
// nothing from a later pipeline stage is required.
bool bounded_by_crossing_lines(
    const cv::Mat& binary,
    const cv::Rect& bounds,
    const ShapeDetectorConfig& config) {

    const int far = config.circle_crossing_probe_distance;
    const int thickness = config.circle_crossing_probe_thickness;

    const double top = (std::min)(
        row_line_continuity(binary, bounds.y, bounds.x - far, bounds.x, thickness),
        row_line_continuity(binary, bounds.y, bounds.x + bounds.width, bounds.x + bounds.width + far, thickness));
    const double bottom = (std::min)(
        row_line_continuity(binary, bounds.y + bounds.height, bounds.x - far, bounds.x, thickness),
        row_line_continuity(binary, bounds.y + bounds.height, bounds.x + bounds.width, bounds.x + bounds.width + far, thickness));
    const double left = (std::min)(
        col_line_continuity(binary, bounds.x, bounds.y - far, bounds.y, thickness),
        col_line_continuity(binary, bounds.x, bounds.y + bounds.height, bounds.y + bounds.height + far, thickness));
    const double right = (std::min)(
        col_line_continuity(binary, bounds.x + bounds.width, bounds.y - far, bounds.y, thickness),
        col_line_continuity(binary, bounds.x + bounds.width, bounds.y + bounds.height, bounds.y + bounds.height + far, thickness));

    const double weakest_side =
        (std::min)((std::min)(top, bottom), (std::min)(left, right));

    return weakest_side >= config.circle_crossing_min_line_continuity;
}

void detect_circles(
    const cv::Mat& normalized,
    const cv::Mat& binary,
    ShapeDetectionArtifacts& result,
    const ShapeDetectorConfig& config,
    const std::string& source_id,
    int page) {

    (void)normalized;

    // HoughCircles is intentionally not used here. On wiring diagrams it
    // readily interprets wire intersections, connector holes, text glyphs,
    // and other repeated geometry as circles. Closed contour geometry gives
    // us stronger evidence that the circle is an actual drawn symbol.
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(
        binary, contours, cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

    for (const auto& contour : contours) {
        const double area = cv::contourArea(contour);
        if (area < config.circle_min_area)
            continue;

        const double perimeter = cv::arcLength(contour, true);
        if (perimeter <= 0.0)
            continue;

        const double circularity =
            4.0 * CV_PI * area / (perimeter * perimeter);

        if (circularity < config.circle_min_circularity)
            continue;

        const cv::Rect bounds = cv::boundingRect(contour);
        if (bounds.width <= 0 || bounds.height <= 0)
            continue;

        const double aspect =
            static_cast<double>((std::max)(bounds.width, bounds.height)) /
            static_cast<double>((std::min)(bounds.width, bounds.height));

        if (aspect > config.circle_max_aspect_ratio)
            continue;

        float radius = 0.0F;
        cv::Point2f center;
        cv::minEnclosingCircle(contour, center, radius);

        if (radius < config.circle_min_radius ||
            radius > config.circle_max_radius)
            continue;

        const int cx = cvRound(center.x);
        const int cy = cvRound(center.y);
        const int r = cvRound(radius);

        const double edge_support =
            circle_edge_support(binary, cx, cy, r);

        if (edge_support < config.circle_min_edge_support)
            continue;

        if (bounded_by_crossing_lines(binary, bounds, config))
            continue;

        const int inset = (std::max)(2, r / 3);
        const cv::Rect interior(
            cx - inset, cy - inset,
            2 * inset + 1, 2 * inset + 1);

        const double interior_density =
            region_density(binary, interior);

        if (interior_density >
            config.circle_max_interior_ink_density)
            continue;

        const cv::Rect image_rect(
            0, 0, normalized.cols, normalized.rows);

        const cv::Rect clipped = bounds & image_rect;
        if (clipped.width < 2 || clipped.height < 2)
            continue;

        const double confidence =
            (std::min)(
                0.99,
                0.50 +
                0.20 * circularity +
                0.20 * edge_support +
                0.10 * (1.0 - interior_density));

        add_region(
            result,
            ShapeKind::Circle,
            ShapeRole::Primitive,
            clipped,
            confidence,
            source_id,
            page);
    }
}

struct GroundBar {
    cv::Rect bounds;
    int center_x;
};

bool ground_bar_width_sequence(
    const std::vector<GroundBar>& bars,
    std::size_t begin,
    std::size_t end,
    const ShapeDetectorConfig& config) {

    if (end <= begin || end - begin < 2)
        return false;

    for (std::size_t i = begin + 1; i < end; ++i) {
        const double previous =
            static_cast<double>(bars[i - 1].bounds.width);
        const double current =
            static_cast<double>(bars[i].bounds.width);

        if (current >= previous)
            return false;

        const double ratio =
            current / (std::max)(1.0, previous);

        if (ratio > 1.0 - config.ground_width_ratio_tolerance)
            return false;

        const double difference =
            (previous - current) /
            (std::max)(1.0, previous);

        if (difference < config.ground_min_width_difference)
            return false;
    }

    return true;
}

// AP-DIAG-FIX-003: a real ground symbol is one continuous drawn glyph, so
// its bars are spaced with a consistent vertical rhythm. Unrelated ink
// (a text label sitting above a box edge, a diode row's leads) that
// coincidentally forms a decreasing-width sequence tends not to share
// that rhythm - see the header comment on ground_max_bar_spacing_ratio
// for the confirmed evidence this is based on.
bool ground_bar_spacing_uniform(
    const std::vector<GroundBar>& bars,
    std::size_t begin,
    std::size_t end,
    const ShapeDetectorConfig& config) {

    if (end <= begin || end - begin < 3)
        return true;

    int min_gap = std::numeric_limits<int>::max();
    int max_gap = 0;
    for (std::size_t i = begin + 1; i < end; ++i) {
        const int gap =
            bars[i].bounds.y -
            (bars[i - 1].bounds.y + bars[i - 1].bounds.height);
        min_gap = (std::min)(min_gap, gap);
        max_gap = (std::max)(max_gap, gap);
    }

    if (min_gap <= 0)
        return false;

    return static_cast<double>(max_gap) / static_cast<double>(min_gap) <=
        config.ground_max_bar_spacing_ratio;
}

// AP-DIAG-FIX-003: evaluates ONE spatially-contiguous run of candidate
// bars (see the Y-locality split in detect_ground_symbols below) for a
// valid chassis-ground bar-and-stem pattern, adding at most one accepted
// region for it.
void evaluate_ground_run(
    const std::vector<GroundBar>& run,
    const cv::Mat& binary,
    ShapeDetectionArtifacts& result,
    const ShapeDetectorConfig& config,
    const std::string& source_id,
    int page) {

    if (run.size() < static_cast<std::size_t>(config.ground_min_bars))
        return;

    const std::size_t max_bars =
        (std::min)(
            run.size(),
            static_cast<std::size_t>(config.ground_max_bars));

    for (std::size_t begin = 0;
         begin + config.ground_min_bars <= max_bars;
         ++begin) {

        // Prefer the longest valid sequence, then allow a shorter
        // sequence if image quality has erased one of the bars.
        for (std::size_t length = max_bars - begin;
             length >= static_cast<std::size_t>(
                 config.ground_min_bars);
             --length) {

            const std::size_t end = begin + length;

            bool spacing_ok = true;
            for (std::size_t n = begin + 1; n < end; ++n) {
                const int gap =
                    run[n].bounds.y -
                    (run[n - 1].bounds.y +
                     run[n - 1].bounds.height);

                if (gap < config.ground_min_bar_spacing ||
                    gap > config.ground_max_bar_spacing) {
                    spacing_ok = false;
                    break;
                }
            }

            if (!spacing_ok ||
                !ground_bar_width_sequence(
                    run, begin, end, config) ||
                !ground_bar_spacing_uniform(
                    run, begin, end, config)) {
                if (length ==
                    static_cast<std::size_t>(
                        config.ground_min_bars))
                    break;
                continue;
            }

            const int first_center = run[begin].center_x;
            const int stem_y0 =
                (std::max)(
                    0,
                    run[begin].bounds.y -
                    config.ground_stem_search_height);
            const int stem_y1 = run[begin].bounds.y;

            // The stem may be represented by the wire itself. We
            // therefore accept either explicit ink in the stem
            // corridor or a conductor-like connection immediately
            // above the first bar.
            const int stem_width = 4;
            const int sx0 =
                (std::max)(0, first_center - stem_width);
            const int sx1 =
                (std::min)(
                    binary.cols,
                    first_center + stem_width + 1);

            if (sx1 <= sx0 || stem_y1 <= stem_y0)
                return;

            const cv::Rect stem_region(
                sx0, stem_y0,
                sx1 - sx0, stem_y1 - stem_y0);

            if (cv::countNonZero(binary(stem_region)) < 2)
                return;

            cv::Rect bounds = run[begin].bounds;
            for (std::size_t n = begin + 1; n < end; ++n)
                bounds |= run[n].bounds;

            // AP-DIAG-FIX-003: the stem-search corridor above (up to
            // ground_stem_search_height, 14px) is deliberately generous
            // for the presence CHECK above - a real symbol's stem is
            // often visually indistinguishable from the wire approaching
            // it, so a wide search window is the right way to confirm
            // "something connects here". But that same 14px used as an
            // EXCLUSION width was found (AP-DIAG-AUDIT-002 follow-up
            // during this fix) to erase real approach-wire ink for
            // several genuine ground symbols packed close to other
            // components - the wire's own drawn path continues right
            // through most of that corridor before reaching the bars.
            // Masking is therefore restricted to the one region we can
            // be sure is the symbol's own exclusive ink: the bars
            // themselves, plus a small fixed margin for anti-aliasing.
            // The wire leading up to the bars stays visible to conductor
            // detection and is classified `ground` by
            // TerminalLocationDetector's own distance-to-component check
            // once it reaches this (now tight) boundary - exactly the
            // mechanism that already correctly classifies every endpoint
            // that reaches a ChassisGround component's bounds.
            constexpr int kGroundExclusionMargin = 2;
            bounds.x = (std::max)(0, bounds.x - kGroundExclusionMargin);
            bounds.y = (std::max)(0, bounds.y - kGroundExclusionMargin);
            bounds.width = (std::min)(
                binary.cols - bounds.x,
                bounds.width + 2 * kGroundExclusionMargin);
            bounds.height = (std::min)(
                binary.rows - bounds.y,
                bounds.height + 2 * kGroundExclusionMargin);

            const double confidence =
                length >= 3 ? 0.95 : 0.82;

            add_region(
                result,
                ShapeKind::ChassisGround,
                ShapeRole::Exclusion,
                bounds,
                confidence,
                source_id,
                page);

            // One accepted ground candidate is sufficient for this
            // run. Do not emit overlapping shorter variants.
            return;
        }
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

    std::vector<GroundBar> bars;
    bars.reserve(static_cast<std::size_t>(count));

    for (int i = 1; i < count; ++i) {
        const int x = stats.at<int>(i, cv::CC_STAT_LEFT);
        const int y = stats.at<int>(i, cv::CC_STAT_TOP);
        const int w = stats.at<int>(i, cv::CC_STAT_WIDTH);
        const int h = stats.at<int>(i, cv::CC_STAT_HEIGHT);

        if (w < config.ground_min_bar_length ||
            w > config.ground_max_bar_length ||
            h > config.ground_max_height)
            continue;

        bars.push_back({{x, y, w, h}, x + w / 2});
    }

    // Group candidate bars by near-common centerline. Sorting by X first
    // makes the grouping deterministic; within a centerline group bars are
    // ordered from top to bottom.
    std::sort(
        bars.begin(), bars.end(),
        [](const GroundBar& a, const GroundBar& b) {
            if (a.center_x != b.center_x)
                return a.center_x < b.center_x;
            return a.bounds.y < b.bounds.y;
        });

    for (std::size_t i = 0; i < bars.size();) {
        std::vector<GroundBar> group;
        group.push_back(bars[i]);

        std::size_t j = i + 1;
        while (j < bars.size() &&
               std::abs(bars[j].center_x - bars[i].center_x) <= 3) {
            group.push_back(bars[j]);
            ++j;
        }

        std::sort(
            group.begin(), group.end(),
            [](const GroundBar& a, const GroundBar& b) {
                if (a.bounds.y != b.bounds.y)
                    return a.bounds.y < b.bounds.y;
                return a.bounds.width > b.bounds.width;
            });

        // AP-DIAG-FIX-003: the x-tolerance scan above only constrains
        // center_x - it says nothing about how close together in Y the
        // bars actually are. Forensic evidence (AP-DIAG-AUDIT-002,
        // docs/AP-DIAG-FIX-003_ChassisGround_Evidence_Classification.md)
        // found this let a bar over 100px away (from an entirely
        // unrelated part of the diagram) merge into the same candidate
        // group purely because its rounded center_x happened to land
        // within the +/-3px tolerance - corrupting the sort-by-Y order
        // and making the real, closely-spaced bar sequence unreachable.
        // A single drawn ground glyph has all its bars close together in
        // Y (within ground_max_bar_spacing of each other); splitting the
        // x-tolerance group into maximal Y-contiguous runs and evaluating
        // each run independently keeps that real spatial constraint
        // without weakening the x-tolerance itself (still needed to
        // tolerate +/-1px center_x rounding between bars of different
        // odd/even width - see the two known genuine symbols, whose own
        // three bars round to center_x values one pixel apart).
        std::vector<std::vector<GroundBar>> runs;
        for (const auto& bar : group) {
            if (!runs.empty()) {
                const auto& prev = runs.back().back();
                const int gap = bar.bounds.y - (prev.bounds.y + prev.bounds.height);
                if (gap >= -2 && gap <= config.ground_max_bar_spacing) {
                    runs.back().push_back(bar);
                    continue;
                }
            }
            runs.push_back({bar});
        }

        for (const auto& run : runs) {
            evaluate_ground_run(
                run, binary, result, config, source_id, page);
        }

        i = j;
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
        // Candidate geometry and wire exclusion are deliberately separate.
        // Primitive symbols remain visible in SHAPES but do not erase the
        // conductor field.
        if (region.role == ShapeRole::Primitive)
            continue;

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

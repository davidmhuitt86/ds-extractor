#include "eke_dx_wire/image/ground_approach_conductor_recovery.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <opencv2/imgproc.hpp>
#include <opencv2/geometry/2d.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace eke::dx::wire {
namespace {

double point_distance(const Point2D& a, const Point2D& b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

bool already_reached(
    const Point2D& anchor,
    const std::vector<ConductorSegment>& existing_conductor_segments,
    double max_distance) {

    for (const auto& segment : existing_conductor_segments) {
        if (point_distance(anchor, segment.geometry.a) <= max_distance ||
            point_distance(anchor, segment.geometry.b) <= max_distance) {
            return true;
        }
    }
    return false;
}

// One contiguous run of foreground pixels within a single row (or, for the
// near-horizontal part of a bend, effectively a single row of the search
// window). Multiple disjoint runs in the same row are the recovery's
// ambiguity signal: a real, unbranched approach wire never presents two
// separate ink runs side by side within its own local window.
struct Run {
    int start = 0;
    int end = 0;
};

std::vector<Run> foreground_runs(
    const cv::Mat& row_labels,
    int label) {

    std::vector<Run> runs;
    bool in_run = false;
    int run_start = 0;

    for (int col = 0; col < row_labels.cols; ++col) {
        const bool is_label = row_labels.at<int>(0, col) == label;
        if (is_label && !in_run) {
            in_run = true;
            run_start = col;
        } else if (!is_label && in_run) {
            in_run = false;
            runs.push_back({run_start, col - 1});
        }
    }
    if (in_run) {
        runs.push_back({run_start, row_labels.cols - 1});
    }
    return runs;
}

} // namespace

GroundApproachConductorRecovery::GroundApproachConductorRecovery(
    GroundApproachRecoveryConfig config)
    : config_(config) {}

GroundApproachRecoveryArtifacts GroundApproachConductorRecovery::recover(
    const cv::Mat& binary,
    const std::vector<ShapeRegion>& shapes,
    const std::vector<ConductorSegment>& existing_conductor_segments,
    const cv::Mat& exclusion_mask,
    const std::string& source_id,
    int page) const {

    GroundApproachRecoveryArtifacts result;

    if (binary.empty())
        return result;

    for (const auto& region : shapes) {
        if (region.kind != ShapeKind::ChassisGround ||
            region.role != ShapeRole::Exclusion) {
            continue;
        }

        const int anchor_x = region.bounds.x + region.bounds.width / 2;
        const int anchor_y = region.bounds.y;
        const Point2D anchor{
            static_cast<double>(anchor_x),
            static_cast<double>(anchor_y)};

        // The standard detector already reached this symbol - this is a
        // gap-filling mechanism only, never a competing or duplicate
        // extraction of ink the pipeline already has evidence for.
        if (already_reached(
                anchor, existing_conductor_segments,
                config_.already_covered_distance)) {
            continue;
        }

        const int x0 = (std::max)(0, anchor_x - config_.corridor_half_width);
        const int x1 = (std::min)(
            binary.cols, anchor_x + config_.corridor_half_width + 1);
        const int y0 = (std::max)(0, anchor_y - config_.search_height);
        const int y1 = (std::min)(binary.rows, anchor_y);

        if (x1 <= x0 || y1 <= y0)
            continue;

        const cv::Rect roi_rect(x0, y0, x1 - x0, y1 - y0);
        cv::Mat roi = binary(roi_rect).clone();

        if (!exclusion_mask.empty() &&
            exclusion_mask.size() == binary.size()) {
            roi.setTo(0, exclusion_mask(roi_rect) > 0);
        }

        cv::Mat labels, stats, centroids;
        const int count = cv::connectedComponentsWithStats(
            roi, labels, stats, centroids, 8, CV_32S);

        // Identify the component(s) touching the ROI's bottom row (the
        // ground symbol's own anchor edge). A real approach conductor must
        // be physically adjacent to the symbol it approaches.
        const int bottom_row = roi.rows - 1;
        int best_label = 0;
        int best_center_distance = std::numeric_limits<int>::max();
        const int corridor_center = roi.cols / 2;

        for (int label = 1; label < count; ++label) {
            const auto runs =
                foreground_runs(labels.row(bottom_row), label);
            for (const auto& run : runs) {
                const int center = (run.start + run.end) / 2;
                const int distance = std::abs(center - corridor_center);
                if (distance < best_center_distance) {
                    best_center_distance = distance;
                    best_label = label;
                }
            }
        }

        if (best_label == 0)
            continue;

        const int pixel_count =
            stats.at<int>(best_label, cv::CC_STAT_AREA);
        if (pixel_count < config_.min_component_pixels)
            continue;

        // Trace a centerline for the selected component starting at the
        // ground symbol itself (the one certain reference point) and
        // moving row by row away from it, toward the far end. A bend or
        // diagonal jog can momentarily present more than one disjoint ink
        // run in a single row (a rounded corner, an anti-aliasing gap);
        // when that happens the run nearest the already-traced path is
        // taken - the same principle as visually following a single wire
        // through a corner, not a choice between two competing wires. Two
        // safety limits bound this: too many disjoint runs in one row, or
        // too large a jump between consecutive rows, means this is not a
        // single unbranched approach wire, and recovery is abandoned for
        // this symbol rather than guessed.
        std::vector<cv::Point2f> centerline_near_to_far;
        bool ambiguous = false;
        double last_x = static_cast<double>(anchor_x);

        for (int row = roi.rows - 1; row >= 0 && !ambiguous; --row) {
            const auto runs = foreground_runs(labels.row(row), best_label);
            if (runs.empty())
                continue;
            if (static_cast<int>(runs.size()) > config_.max_disjoint_runs_per_row) {
                ambiguous = true;
                break;
            }

            const Run* nearest = nullptr;
            double nearest_distance = std::numeric_limits<double>::max();
            for (const auto& run : runs) {
                const double center_x =
                    static_cast<double>(x0) + (run.start + run.end) / 2.0;
                const double distance = std::abs(center_x - last_x);
                if (distance < nearest_distance) {
                    nearest_distance = distance;
                    nearest = &run;
                }
            }

            const int width = nearest->end - nearest->start + 1;
            if (width > config_.max_run_width ||
                nearest_distance > config_.max_row_to_row_jump) {
                ambiguous = true;
                break;
            }

            const double center_col =
                (nearest->start + nearest->end) / 2.0;
            const double center_x = static_cast<double>(x0) + center_col;
            centerline_near_to_far.emplace_back(
                static_cast<float>(center_x), static_cast<float>(y0 + row));
            last_x = center_x;
        }

        if (ambiguous || centerline_near_to_far.size() < 2)
            continue;

        // Build far-to-near order (matches the geometry direction a
        // standard-detected conductor would use) and snap the near end
        // exactly to the anchor point, so the recovered path cleanly
        // meets the symbol rather than stopping a fraction of a pixel
        // short of it.
        std::vector<cv::Point2f> centerline(
            centerline_near_to_far.rbegin(), centerline_near_to_far.rend());
        centerline.back() = cv::Point2f(
            static_cast<float>(anchor_x), static_cast<float>(anchor_y));

        std::vector<cv::Point2f> simplified;
        cv::approxPolyDP(
            centerline, simplified,
            config_.polyline_simplify_epsilon, /*closed=*/false);

        if (simplified.size() < 2)
            continue;

        const double thickness =
            static_cast<double>(pixel_count) /
            (std::max)(1.0, static_cast<double>(centerline.size()));

        for (std::size_t i = 0; i + 1 < simplified.size(); ++i) {
            ConductorSegment segment;
            segment.geometry = Segment2D{
                Point2D{simplified[i].x, simplified[i].y},
                Point2D{simplified[i + 1].x, simplified[i + 1].y}};
            segment.thickness_px = thickness;
            segment.confidence = ConfidenceClass::Medium;
            segment.provenance.source_id = source_id;
            segment.provenance.page = page;
            segment.provenance.source_region = {
                roi_rect.x, roi_rect.y, roi_rect.width, roi_rect.height};
            segment.provenance.stage = "ground_approach_recovery";

            std::ostringstream canonical;
            canonical << source_id << ":" << page << ":" << region.id << ":"
                      << i << ":" << segment.geometry.a.x << ","
                      << segment.geometry.a.y << "-" << segment.geometry.b.x
                      << "," << segment.geometry.b.y;
            segment.id = stable_id("conductor-segment", canonical.str());

            result.conductor_segments.push_back(std::move(segment));
        }
    }

    std::sort(
        result.conductor_segments.begin(),
        result.conductor_segments.end(),
        [](const ConductorSegment& a, const ConductorSegment& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

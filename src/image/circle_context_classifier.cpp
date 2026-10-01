#include "eke_dx_wire/image/circle_context_classifier.hpp"

#include <algorithm>
#include <cmath>

namespace eke::dx::wire {
namespace {

double point_to_box_distance(const Point2D& p, const BoundingBox& box) {
    const double left = static_cast<double>(box.x);
    const double right = static_cast<double>(box.x + box.width);
    const double top = static_cast<double>(box.y);
    const double bottom = static_cast<double>(box.y + box.height);

    const double dx =
        p.x < left ? left - p.x :
        p.x > right ? p.x - right : 0.0;
    const double dy =
        p.y < top ? top - p.y :
        p.y > bottom ? p.y - bottom : 0.0;

    return std::hypot(dx, dy);
}

bool segment_intersects_box(
    const Segment2D& segment,
    const BoundingBox& box) {

    const double xmin = static_cast<double>(box.x);
    const double xmax = static_cast<double>(box.x + box.width);
    const double ymin = static_cast<double>(box.y);
    const double ymax = static_cast<double>(box.y + box.height);

    double t0 = 0.0;
    double t1 = 1.0;
    const double dx = segment.b.x - segment.a.x;
    const double dy = segment.b.y - segment.a.y;

    const auto clip = [&](double p, double q) {
        if (std::abs(p) < 1e-12)
            return q >= 0.0;

        const double r = q / p;
        if (p < 0.0) {
            if (r > t1)
                return false;
            t0 = (std::max)(t0, r);
        } else {
            if (r < t0)
                return false;
            t1 = (std::min)(t1, r);
        }
        return true;
    };

    return
        clip(-dx, segment.a.x - xmin) &&
        clip( dx, xmax - segment.a.x) &&
        clip(-dy, segment.a.y - ymin) &&
        clip( dy, ymax - segment.a.y);
}

bool has_conductor_attachment(
    const ComponentCandidate& candidate,
    const std::vector<ConductorSegment>& conductors,
    double endpoint_distance) {

    for (const auto& conductor : conductors) {
        if (segment_intersects_box(conductor.geometry, candidate.bounds))
            return true;

        if (point_to_box_distance(
                conductor.geometry.a, candidate.bounds) <= endpoint_distance ||
            point_to_box_distance(
                conductor.geometry.b, candidate.bounds) <= endpoint_distance)
            return true;
    }

    return false;
}

} // namespace

CircleContextClassifier::CircleContextClassifier(CircleContextConfig config)
    : config_(config) {}

std::vector<ComponentCandidate> CircleContextClassifier::classify(
    const std::vector<ComponentCandidate>& candidates,
    const std::vector<ConductorSegment>& conductors) const {

    std::vector<ComponentCandidate> result;
    result.reserve(candidates.size());

    for (const auto& candidate : candidates) {
        if (candidate.kind != ComponentCandidateKind::CircularSymbol) {
            result.push_back(candidate);
            continue;
        }

        // AP-DIAG-028: corrected forensic evidence shows all five
        // source-confirmed genuine circles have max_run_fraction below
        // 0.75, while 14 of 17 false survivors reach 1.0. This rejects
        // line/grid artifacts without using confidence as a proxy.
        if (candidate.circle_probe_max_run_fraction >=
            config_.max_run_fraction) {
            continue;
        }

        // The three remaining ambiguous survivors have no conductor
        // geometry that intersects their candidate bounds and no conductor
        // endpoint within 5 px. All five source-confirmed genuine circles
        // have either an intersection or a conductor endpoint within that
        // distance. This is a geometry-level attachment test; it does not
        // depend on later terminal recognition.
        if (!has_conductor_attachment(
                candidate,
                conductors,
                config_.max_conductor_endpoint_distance_px)) {
            continue;
        }

        result.push_back(candidate);
    }

    return result;
}

} // namespace eke::dx::wire

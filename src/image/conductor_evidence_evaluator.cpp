#include "eke_dx_wire/image/conductor_evidence_evaluator.hpp"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <string>

namespace eke::dx::wire {
namespace {

double supporting_density(
    const cv::Mat& image,
    const Segment2D& segment,
    double half_width) {

    if (image.empty() || segment.length() <= 0.0)
        return 0.0;

    const int width = std::max(
        1, static_cast<int>(std::ceil(segment.length())) + 1);
    const int radius = std::max(
        0, static_cast<int>(std::ceil(half_width)));

    int ink = 0;
    int samples = 0;

    const bool horizontal =
        std::abs(segment.b.y - segment.a.y) <=
        std::abs(segment.b.x - segment.a.x);

    for (int i = 0; i < width; ++i) {
        const double t =
            width == 1 ? 0.0 :
            static_cast<double>(i) / static_cast<double>(width - 1);

        const int x = static_cast<int>(std::lround(
            segment.a.x + (segment.b.x - segment.a.x) * t));
        const int y = static_cast<int>(std::lround(
            segment.a.y + (segment.b.y - segment.a.y) * t));

        for (int offset = -radius; offset <= radius; ++offset) {
            const int sx = horizontal ? x : x + offset;
            const int sy = horizontal ? y + offset : y;

            if (sx < 0 || sy < 0 ||
                sx >= image.cols || sy >= image.rows) {
                continue;
            }

            ++samples;
            if (image.at<std::uint8_t>(sy, sx) < 200)
                ++ink;
        }
    }

    return samples == 0
        ? 0.0
        : static_cast<double>(ink) / static_cast<double>(samples);
}

} // namespace

ConductorEvidenceEvaluator::ConductorEvidenceEvaluator(
    ConductorEvidenceConfig config)
    : config_(config) {}

ConductorEvidenceArtifacts ConductorEvidenceEvaluator::evaluate(
    const std::vector<ConductorSegment>& candidates,
    const cv::Mat& normalized) const {

    ConductorEvidenceArtifacts result;

    for (const auto& candidate : candidates) {
        const double length = candidate.geometry.length();

        if (length < config_.minimum_continuous_length) {
            RejectedGeometryEvidence evidence;
            evidence.id = candidate.id;
            evidence.geometry = candidate.geometry;
            evidence.reason = "below_minimum_continuous_length";
            evidence.measurement = length;
            evidence.provenance = candidate.provenance;
            result.rejected.push_back(std::move(evidence));
            continue;
        }

        const double density = supporting_density(
            normalized,
            candidate.geometry,
            config_.support_corridor_half_width);

        if (density < config_.minimum_supporting_ink_density) {
            RejectedGeometryEvidence evidence;
            evidence.id = candidate.id;
            evidence.geometry = candidate.geometry;
            evidence.reason = "insufficient_supporting_ink";
            evidence.measurement = density;
            evidence.provenance = candidate.provenance;
            result.rejected.push_back(std::move(evidence));
            continue;
        }

        result.accepted.push_back(candidate);
    }

    return result;
}

} // namespace eke::dx::wire

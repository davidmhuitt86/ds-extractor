#include "eke_dx_wire/image/conductor_evidence_evaluator.hpp"

#include <opencv2/core.hpp>

#include <cassert>

using namespace eke::dx::wire;

static ConductorSegment candidate(
    const char* id, Point2D a, Point2D b) {
    ConductorSegment s;
    s.id = id;
    s.geometry = {a, b};
    s.provenance.source_id = "fixture";
    s.provenance.page = 0;
    return s;
}

int main() {
    cv::Mat image(40, 80, CV_8UC1, cv::Scalar(255));

    // A supported 30 px conductor.
    for (int x = 5; x <= 34; ++x)
        image.at<unsigned char>(20, x) = 0;

    const std::vector<ConductorSegment> candidates{
        candidate("long", {5, 20}, {34, 20}),
        candidate("short", {50, 10}, {55, 10})
    };

    ConductorEvidenceConfig config;
    config.minimum_continuous_length = 12.0;
    config.minimum_supporting_ink_density = 0.08;
    config.support_corridor_half_width = 1.0;

    const auto result =
        ConductorEvidenceEvaluator(config).evaluate(candidates, image);

    assert(result.accepted.size() == 1);
    assert(result.accepted.front().id == "long");
    assert(result.rejected.size() == 1);
    assert(result.rejected.front().id == "short");
    assert(result.rejected.front().reason ==
           "below_minimum_continuous_length");

    return 0;
}

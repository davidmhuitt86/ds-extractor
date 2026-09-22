#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <opencv2/core.hpp>
#include <vector>

namespace eke::dx::wire {

struct ConductorEvidenceConfig {
    double minimum_continuous_length = 12.0;
    double minimum_supporting_ink_density = 0.08;
    double support_corridor_half_width = 2.0;
};

struct ConductorEvidenceArtifacts {
    std::vector<ConductorSegment> accepted;
    std::vector<RejectedGeometryEvidence> rejected;
};

class ConductorEvidenceEvaluator {
public:
    explicit ConductorEvidenceEvaluator(ConductorEvidenceConfig config = {});

    [[nodiscard]] ConductorEvidenceArtifacts evaluate(
        const std::vector<ConductorSegment>& candidates,
        const cv::Mat& normalized) const;

private:
    ConductorEvidenceConfig config_;
};

} // namespace eke::dx::wire

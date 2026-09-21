#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <opencv2/core.hpp>
#include <vector>

namespace eke::dx::wire {

struct GapInterpretationConfig {
    double maximum_gap = 18.0;
    double collinear_tolerance = 1.5;
    double minimum_gap = 1.5;
    double minimum_ink_density = 0.08;
};

struct GapInterpretationArtifacts {
    std::vector<TopologyEdge> inferred_edges;
};

class GapInterpreter {
public:
    explicit GapInterpreter(GapInterpretationConfig config = {});

    [[nodiscard]] GapInterpretationArtifacts interpret(
        std::vector<TopologyNode>& nodes,
        const std::vector<TopologyEdge>& physical_edges,
        const cv::Mat& normalized_source,
        const std::string& source_id,
        int page = 0) const;

private:
    GapInterpretationConfig config_;
};

} // namespace eke::dx::wire

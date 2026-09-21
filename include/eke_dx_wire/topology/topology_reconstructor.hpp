#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

struct TopologyConfig {
    double snap_tolerance = 4.0;
    double intersection_tolerance = 0.75;
};

struct TopologyArtifacts {
    std::vector<TopologyNode> nodes;
    std::vector<TopologyEdge> edges;
};

class TopologyReconstructor {
public:
    explicit TopologyReconstructor(TopologyConfig config = {});

    [[nodiscard]] TopologyArtifacts reconstruct(
        const std::vector<ConductorSegment>& segments,
        const std::string& source_id,
        int page = 0) const;

private:
    TopologyConfig config_;
};

} // namespace eke::dx::wire

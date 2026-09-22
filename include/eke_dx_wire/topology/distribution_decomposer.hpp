#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

struct DistributionDecompositionConfig {
    // Distribution is decomposed only when a unique semantic anchor is
    // available. Geometric/component endpoints alone are intentionally
    // insufficient to choose a source branch.
    bool allow_ground_anchor = true;
    bool allow_external_anchor = true;
};

struct DistributionDecompositionArtifacts {
    std::vector<ElectricalNet> nets;
    std::vector<Wire> wires;
};

class DistributionDecomposer {
public:
    explicit DistributionDecomposer(
        DistributionDecompositionConfig config = {});

    [[nodiscard]] DistributionDecompositionArtifacts decompose(
        const std::vector<TopologyNode>& nodes,
        const std::vector<TopologyEdge>& edges,
        const std::vector<EndpointCandidate>& endpoints,
        const std::vector<ConductorSegment>& conductors,
        const std::string& source_id,
        int page = 0) const;

private:
    DistributionDecompositionConfig config_;
};

} // namespace eke::dx::wire

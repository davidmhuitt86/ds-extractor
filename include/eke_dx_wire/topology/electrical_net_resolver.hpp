#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/topology/distribution_decomposer.hpp"
#include "eke_dx_wire/topology/circuit_role_resolver.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

struct ElectricalNetResolutionArtifacts {
    std::vector<ElectricalNet> nets;
    std::vector<Wire> wires;
    std::vector<std::string> unresolved_net_ids;
};

class ElectricalNetResolver {
public:
    explicit ElectricalNetResolver(
        DistributionDecompositionConfig distribution_config = {});

    [[nodiscard]] ElectricalNetResolutionArtifacts resolve(
        const std::vector<TopologyNode>& nodes,
        const std::vector<TopologyEdge>& edges,
        const std::vector<EndpointCandidate>& endpoints,
        const std::vector<ConductorSegment>& conductors,
        const std::vector<CircuitRoleEvidence>& observation_evidence,
        const std::string& source_id,
        int page = 0) const;

private:
    DistributionDecompositionConfig distribution_config_;
};

} // namespace eke::dx::wire

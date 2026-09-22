#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

struct CircuitRoleEvidence {
    std::string endpoint_id;
    DistributionRole role = DistributionRole::Unknown;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    std::string source;
};

struct CircuitRoleResolutionArtifacts {
    std::vector<ElectricalNet> nets;
    std::vector<std::string> unresolved_net_ids;
};

class CircuitRoleResolver {
public:
    [[nodiscard]] CircuitRoleResolutionArtifacts resolve(
        const std::vector<ElectricalNet>& nets,
        const std::vector<EndpointCandidate>& endpoints,
        const std::vector<CircuitRoleEvidence>& evidence) const;
};

} // namespace eke::dx::wire

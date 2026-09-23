#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

class EngineeringObjectSemanticApplier {
public:
    // Applies already-resolved semantic observations to the corresponding
    // engineering objects. This stage may enrich semantic fields, but it
    // never changes geometry, topology, wire identity, or net connectivity.
    void apply(
        std::vector<ComponentCandidate>& components,
        std::vector<EndpointCandidate>& endpoints,
        const std::vector<EngineeringObjectSemanticResolution>& resolutions) const;
};

} // namespace eke::dx::wire

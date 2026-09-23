#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

class ComponentIdentityEvidenceBuilder {
public:
    [[nodiscard]] std::vector<ComponentIdentityEvidence> build(
        const std::vector<EngineeringObjectSemanticResolution>& resolutions) const;
};

} // namespace eke::dx::wire

#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

class ComponentIdentityResolver {
public:
    [[nodiscard]] std::vector<ComponentIdentityResolution> resolve(
        const std::vector<ComponentIdentityEvidence>& evidence) const;
};

} // namespace eke::dx::wire

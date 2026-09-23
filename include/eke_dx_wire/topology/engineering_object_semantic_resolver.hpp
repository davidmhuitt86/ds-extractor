#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {


class EngineeringObjectSemanticResolver {
public:
    [[nodiscard]] std::vector<EngineeringObjectSemanticResolution> resolve(
        const std::vector<TextSemanticEvidence>& semantic_evidence,
        const std::vector<SemanticAssociation>& associations) const;
};
} // namespace eke::dx::wire

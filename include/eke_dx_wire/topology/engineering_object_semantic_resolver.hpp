#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

struct EngineeringObjectSemanticResolution {
    std::string id;
    std::string text_region_id;
    std::string target_id;
    SemanticAssociationTargetKind target_kind =
        SemanticAssociationTargetKind::Endpoint;
    TextSemanticKind semantic_kind = TextSemanticKind::Unknown;
    std::string raw_text;
    std::string normalized_text;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    double distance = 0.0;
    std::string source;
};

class EngineeringObjectSemanticResolver {
public:
    [[nodiscard]] std::vector<EngineeringObjectSemanticResolution> resolve(
        const std::vector<TextSemanticEvidence>& semantic_evidence,
        const std::vector<SemanticAssociation>& associations) const;
};

} // namespace eke::dx::wire

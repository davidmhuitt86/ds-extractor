#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

struct SemanticEvidenceAssociationConfig {
    double high_confidence_distance = 12.0;
    double medium_confidence_distance = 30.0;
    double maximum_association_distance = 80.0;
};

class SemanticEvidenceAssociator {
public:
    explicit SemanticEvidenceAssociator(
        SemanticEvidenceAssociationConfig config = {});

    [[nodiscard]] std::vector<SemanticAssociation> associate(
        const std::vector<TextRegion>& text_regions,
        const std::vector<ComponentCandidate>& components,
        const std::vector<EndpointCandidate>& endpoints) const;

private:
    SemanticEvidenceAssociationConfig config_;
};

} // namespace eke::dx::wire

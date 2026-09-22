#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/topology/terminal_semantic_resolver.hpp"

#include <vector>

namespace eke::dx::wire {

class TerminalSemanticEvidenceBuilder {
public:
    [[nodiscard]] std::vector<TerminalSemanticEvidence> build(
        const std::vector<TerminalCandidate>& terminal_candidates) const;
};

} // namespace eke::dx::wire

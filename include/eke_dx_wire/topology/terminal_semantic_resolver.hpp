#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

struct TerminalSemanticEvidence {
    std::string endpoint_id;
    TerminalRole role = TerminalRole::Unknown;
    EndpointKind endpoint_kind = EndpointKind::Unresolved;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    std::string component_id;
    std::string terminal_name;
    std::string function_label;
    std::string wire_color;
};

class TerminalSemanticResolver {
public:
    [[nodiscard]] std::vector<EndpointCandidate> resolve(
        const std::vector<EndpointCandidate>& candidates,
        const std::vector<TerminalSemanticEvidence>& evidence) const;
};

} // namespace eke::dx::wire

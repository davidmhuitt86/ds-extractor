#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

enum class TopologyConnectionState {
    Unknown,
    Connected,
    NotConnected
};

struct TopologyConnectionEvidence {
    std::string node_id;
    TopologyConnectionState state = TopologyConnectionState::Unknown;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    std::string source;
};

class TopologySemanticResolver {
public:
    [[nodiscard]] std::vector<TopologyNode> resolve(
        const std::vector<TopologyNode>& nodes,
        const std::vector<TopologyConnectionEvidence>& evidence) const;
};

} // namespace eke::dx::wire

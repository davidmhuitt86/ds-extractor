#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

struct ConnectorModelArtifacts {
    std::vector<ConnectorCandidate> connectors;
    std::vector<ConnectorTerminal> terminals;
};

class ConnectorTerminalModelBuilder {
public:
    [[nodiscard]] ConnectorModelArtifacts build(
        const std::vector<ComponentCandidate>& components,
        const std::vector<TerminalCandidate>& terminal_candidates,
        const std::vector<EndpointCandidate>& endpoints) const;
};

} // namespace eke::dx::wire

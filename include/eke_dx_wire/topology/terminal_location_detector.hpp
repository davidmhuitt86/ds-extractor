#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

struct TerminalLocationArtifacts {
    std::vector<TerminalCandidate> candidates;
};

struct TerminalLocationConfig {
    double boundary_tolerance = 8.0;
    double high_confidence_distance = 2.5;
    double medium_confidence_distance = 5.0;
    // When true, an endpoint inside a component candidate's bounds is
    // considered attached to that component. This handles symbols whose
    // terminal geometry is represented inside the detected primitive rather
    // than exactly on its geometric bounding box.
    bool allow_interior_attachment = true;
    bool require_component_boundary_proximity = true;
};

class TerminalLocationDetector {
public:
    explicit TerminalLocationDetector(TerminalLocationConfig config = {});

    [[nodiscard]] TerminalLocationArtifacts detect(
        const std::vector<ComponentCandidate>& components,
        const std::vector<EndpointCandidate>& endpoints) const;

private:
    TerminalLocationConfig config_;
};

} // namespace eke::dx::wire

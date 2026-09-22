#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

enum class TerminalCandidateKind {
    ComponentBoundary,
    ConnectorBoundary,
    GroundConnection,
    Unknown
};

struct TerminalCandidate {
    std::string id;
    std::string endpoint_id;
    std::string component_candidate_id;
    TerminalCandidateKind kind = TerminalCandidateKind::Unknown;
    Point2D position {};
    double distance_to_component = 0.0;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
};

struct TerminalLocationArtifacts {
    std::vector<TerminalCandidate> candidates;
};

struct TerminalLocationConfig {
    double boundary_tolerance = 8.0;
    double high_confidence_distance = 2.5;
    double medium_confidence_distance = 5.0;
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

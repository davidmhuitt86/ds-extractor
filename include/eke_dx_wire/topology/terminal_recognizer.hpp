#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

// AP-WIRE-024: recognizes terminal associations from existing endpoint
// geometry plus AP-WIRE-023 symbol geometry. This stage never creates an
// EndpointCandidate and never mutates topology, wires, or electrical nets.
struct TerminalRecognitionConfig {
    double terminal_lead_max_distance = 6.0;
    double aligned_boundary_max_distance = 16.0;
    double minimum_alignment_cosine = 0.85;
};

struct TerminalRecognitionArtifacts {
    std::vector<TerminalCandidate> candidates;
};

class TerminalRecognizer {
public:
    explicit TerminalRecognizer(TerminalRecognitionConfig config = {});

    [[nodiscard]] TerminalRecognitionArtifacts recognize(
        const std::vector<ComponentCandidate>& components,
        const std::vector<ComponentSymbolGeometry>& geometries,
        const std::vector<SymbolPrimitive>& primitives,
        const std::vector<EndpointCandidate>& endpoints,
        const std::vector<TopologyNode>& nodes,
        const std::vector<TopologyEdge>& edges,
        const std::vector<TerminalCandidate>& existing_candidates) const;

private:
    TerminalRecognitionConfig config_;
};

} // namespace eke::dx::wire

#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

struct WireSemanticResolutionArtifacts {
    std::vector<WireSemanticResolution> resolutions;
    WireSemanticCoverage coverage;
};

/**
 * AP-WIRE-025: attaches defensible engineering semantics to already-
 * reconstructed Wire objects using evidence already present in the
 * extraction model. This is a pure read-only projection: it consumes
 * Wire/EndpointCandidate/EndpointSemanticReconstruction/ConnectorTerminal/
 * ElectricalNet and never mutates any of them, never creates or deletes
 * a Wire/endpoint/topology object, and never invents evidence. Where
 * evidence is absent the result is Unresolved; where independent evidence
 * disagrees the result is Conflicted - never a guessed winner.
 */
class WireSemanticResolver {
public:
    [[nodiscard]] WireSemanticResolutionArtifacts resolve(
        const std::vector<Wire>& wires,
        const std::vector<EndpointCandidate>& endpoints,
        const std::vector<EndpointSemanticReconstruction>& reconstructions,
        const std::vector<ConnectorTerminal>& connector_terminals,
        const std::vector<ElectricalNet>& electrical_nets) const;
};

// Shared by WireSemanticResolver::resolve() and ExtractionAudit building,
// so the two never compute divergent coverage numbers from the same
// resolutions.
[[nodiscard]] WireSemanticCoverage build_wire_semantic_coverage(
    const std::vector<WireSemanticResolution>& resolutions);

} // namespace eke::dx::wire

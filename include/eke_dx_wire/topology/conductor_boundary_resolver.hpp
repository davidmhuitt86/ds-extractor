#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

struct ConductorBoundaryResolutionArtifacts {
    std::vector<ConductorBoundaryEvidence> evidence;
    std::vector<ConductorBoundaryResolution> resolutions;
    ConductorBoundaryCoverage coverage;
};

/**
 * AP-WIRE-030: resolves the engineering meaning of each already-detected
 * EndpointCandidate - what Conductor Boundary it is (AP-WIRE-029's
 * taxonomy: ComponentTerminal / ConnectorTerminal / GroundTerminal /
 * ExternalConnection / GeometricConductorEnd / Unresolved), and what
 * component/connector/terminal identity, if any, it represents.
 *
 * This is a pure read-only semantic layer over already-produced evidence
 * (TerminalCandidate from AP-WIRE-024, EndpointSemanticReconstruction
 * from AP-WIRE-019, ConnectorTerminal from AP-WIRE-020). It:
 *
 *   - never creates, deletes, or reclassifies a topology node or edge;
 *   - never creates a component, connector, or terminal candidate;
 *   - never fabricates a specific terminal/pin identifier;
 *   - never uses electrical-net membership as boundary evidence;
 *   - never assembles a Wire or decides which boundaries share a Wire
 *     (that is AP-WIRE-031's question, not this one);
 *   - never treats Splice/Junction/Crossing/Continuation as a Conductor
 *     Boundary (AP-WIRE-029's critical invariant);
 *   - keeps component association and terminal identity as independently
 *     tracked Resolved/Unresolved/Conflicted statuses for the same
 *     endpoint (AP-WIRE-030 Sec 15) - this is the one representational
 *     gap EndpointSemanticReconstruction's single combined status cannot
 *     close, and the reason this resolver exists as a distinct layer
 *     rather than a change to that resolver.
 *
 * Existing AP-WIRE-024 TerminalCandidates and AP-WIRE-019
 * EndpointSemanticReconstructions are consumed as evidence, not
 * replaced or recomputed from scratch; a ConnectorTerminal's own
 * ConnectorTerminalStatus is adopted directly for the connector/pin
 * facts, never re-decided.
 */
class ConductorBoundaryResolver {
public:
    [[nodiscard]] ConductorBoundaryResolutionArtifacts resolve(
        const std::vector<EndpointCandidate>& endpoints,
        const std::vector<TerminalCandidate>& terminal_candidates,
        const std::vector<EndpointSemanticReconstruction>&
            endpoint_semantic_reconstructions,
        const std::vector<ConnectorTerminal>& connector_terminals) const;
};

// Shared by ConductorBoundaryResolver::resolve() and ExtractionAudit
// building, so the two never compute divergent coverage numbers from the
// same resolutions.
[[nodiscard]] ConductorBoundaryCoverage build_conductor_boundary_coverage(
    const std::vector<ConductorBoundaryResolution>& resolutions);

} // namespace eke::dx::wire

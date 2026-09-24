#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/topology/wire_reconstructor.hpp"

#include <vector>

namespace eke::dx::wire {

struct PhysicalWireIdentityArtifacts {
    std::vector<Wire> wires;
    // Preserved from the internal WireReconstructor pass, unchanged in
    // meaning (AP-WIRE-028 finding #9: previously computed but never
    // exported - now surfaced here so a caller can inspect it).
    std::vector<std::string> unresolved_nodes;
};

/**
 * AP-WIRE-031: the pipeline's authoritative physical-Wire-identity
 * reconstruction stage, per docs/AP-WIRE-029_Conductor_Boundary_and_Wire_
 * Identity.md and docs/AP-WIRE-030_Conductor_Boundary_and_Terminal_
 * Resolution.md. This is the only stage whose output is assigned to
 * WireModel::wires.
 *
 * It runs in two passes:
 *
 *   1. The existing WireReconstructor's conservative degree-1 ->
 *      degree-2-Continuation* -> degree-1 walk, unchanged, for every
 *      pairing it can already establish without crossing a distribution
 *      node or a Crossing. This pass's endpoint eligibility rule
 *      (unchanged, historical AP-WIRE-013 behavior) is retained exactly
 *      as WireReconstructor already implements it.
 *
 *   2. For every endpoint the first pass left with no Wire, attempt to
 *      extend physical identity through one or more Splice/Junction/
 *      Crossing nodes using explicit conductor-segment-sharing evidence
 *      only: at a node with more than two incident edges, the walk may
 *      continue only when exactly one OTHER incident edge references the
 *      same ConductorSegment as the edge just arrived on (the
 *      representation-level signal that the source geometry was detected
 *      as one continuous conductor through that point). If zero or more
 *      than one edge share that segment, the walk stops there - the
 *      Splice/Junction/Crossing node itself is never treated as a Wire
 *      boundary (AP-WIRE-029's critical invariant), and no branch
 *      pairing is ever picked by convenience. Both endpoints of an
 *      extended Wire must additionally carry a Resolved AP-WIRE-030
 *      ConductorBoundaryResolution - a bare geometric conductor end is
 *      never treated as a new Wire boundary by this second pass.
 *
 * If, after this second pass, more than one independently-walked
 * candidate pairing claims the same endpoint as its far side (a genuine
 * evidence contradiction - two different physical paths both claim the
 * same single conductor termination), every candidate touching that
 * endpoint is still added to the result, but with
 * WireIdentityStatus::Conflicted rather than Resolved - no winner is
 * ever picked.
 *
 * This stage never creates, deletes, or reclassifies a topology node or
 * edge, never consults ElectricalNet membership, and never invents
 * evidence: identity_evidence_ids only ever references real, existing
 * ConductorBoundaryResolution and ConductorSegment ids.
 */
class PhysicalWireIdentityReconstructor {
public:
    [[nodiscard]] PhysicalWireIdentityArtifacts reconstruct(
        const std::vector<TopologyNode>& nodes,
        const std::vector<TopologyEdge>& edges,
        const std::vector<EndpointCandidate>& endpoints,
        const std::vector<ConductorSegment>& conductors,
        const std::vector<ConductorBoundaryResolution>& boundary_resolutions,
        const std::string& source_id,
        int page = 0) const;
};

} // namespace eke::dx::wire

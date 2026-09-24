#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

// AP-WIRE-013: reconstructs the conservative special case of physical
// Wire identity - an unbroken chain of degree-1 endpoint -> zero or more
// degree-2 Continuation nodes -> degree-1 endpoint, with no Splice,
// Junction, or Crossing node anywhere along the path. This class's
// behavior is intentionally NOT the authoritative definition of Wire
// identity (see docs/AP-WIRE-029_Conductor_Boundary_and_Wire_Identity.md
// Sec 19, which names this exact class and this exact limitation). It
// remains correct and useful as-is for the unambiguous case it covers -
// no explicit evidence is needed to justify continuing through a
// Continuation node, since a degree-2 node has no alternative path - but
// it never attempts to cross a distribution node (Splice/Junction) or a
// Crossing, because doing so without evidence would guess at physical
// continuity. `PhysicalWireIdentityReconstructor` (AP-WIRE-031,
// include/eke_dx_wire/topology/physical_wire_identity_reconstructor.hpp)
// is the pipeline's authoritative Wire-producing stage: it uses this
// class internally for the simple case, then extends physical Wire
// identity through distribution/crossing nodes only where explicit
// conductor-segment-sharing evidence justifies it, and is the only stage
// whose output is assigned to WireModel::wires.
struct WireReconstructionConfig {
    bool require_semantic_endpoints = true;
};

struct WireReconstructionArtifacts {
    std::vector<Wire> wires;
    std::vector<std::string> unresolved_nodes;
};

class WireReconstructor {
public:
    explicit WireReconstructor(WireReconstructionConfig config = {});

    [[nodiscard]] WireReconstructionArtifacts reconstruct(
        const std::vector<TopologyNode>& nodes,
        const std::vector<TopologyEdge>& edges,
        const std::vector<EndpointCandidate>& endpoints,
        const std::vector<ConductorSegment>& conductors,
        const std::string& source_id,
        int page = 0) const;

private:
    WireReconstructionConfig config_;
};

} // namespace eke::dx::wire

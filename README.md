# EKE-DX-WIRE — Deterministic Wire Reconstruction Engine

Version: 0.1.1-foundation  
Language: C++23  
Build: CMake 3.24+  
Primary CV dependency: OpenCV 5.x

## Foundation model

The detector extracts observable **conductor geometry**. A detected line
segment is not automatically a wire.

The domain model distinguishes:

- ConductorSegment — observed/drawn conductor geometry.
- TopologyNode — a point in the connectivity graph.
- TopologyEdge — graph connectivity backed by conductor geometry.
- Wire — an endpoint-to-endpoint engineering trace.

### Normative wire identity rule

A wire is one continuous conductor traced from one true endpoint to another
true endpoint.

A splice/junction is an internal topology node. It does not terminate or
divide a wire.

Example:

    Terminal A ---------o--------- Terminal B
                        |
                        +--------- Terminal C

The intended wire identities are:

    Wire 1: Terminal A -> Terminal B
    Wire 2: Terminal A -> Terminal C

The shared conductor section may therefore participate in multiple
endpoint-to-endpoint wire traces.

This is why wire identity cannot be assigned during pixel detection.

## Current implementation

Implemented:

- C++23/CMake project structure.
- raster image inspection/extraction.
- adaptive thresholding.
- horizontal/vertical morphology.
- connected-component conductor candidates.
- centerline generation.
- vector SVG projection.
- deterministic candidate IDs.
- separate geometry and ID tests.

Not yet implemented:

- PDF-native source rendering.
- non-wire region detection/clipping.
- collinear merging.
- endpoint snapping.
- topology graph construction.
- true component/terminal endpoint attachment.
- endpoint-to-endpoint wire reconstruction.
- junction/crossing semantics.
- heavy-cable classification.
- complete validation/overlay.
- GUI.

The implementation must keep conductor detection, topology reconstruction,
and wire identity as separate stages.

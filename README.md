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


## Development GUI

The Windows build includes an optional development instrument:

    dx-extractor-gui

It uses OpenCV HighGUI and the native Windows file picker. It displays the
source diagram, overlays detected conductor segments, and can display topology
nodes. The GUI is a thin client over libeke_dx_wire and contains no extraction
algorithms.

This is not the final OEP/EKE user interface.


## Release automation

The Windows development GUI includes a **RELEASE / PR** action.

The action closes the GUI and launches:

    tools/dx-release.ps1

The release pipeline:

1. verifies Git and GitHub CLI authentication.
2. configures the CMake Release build.
3. builds all Release targets.
4. runs the complete Release CTest suite.
5. stops immediately if configuration, compilation, or tests fail.
6. creates a feature branch when invoked from `main`/ `master`.
7. commits working-tree changes.
8. pushes the feature branch.
9. creates a GitHub pull request targeting `main`.

The pipeline never merges a pull request.

The same script can be run directly:

    powershell -ExecutionPolicy Bypass -File tools/dx-release.ps1

For build/test only:

    powershell -ExecutionPolicy Bypass -File tools/dx-release.ps1 -BuildOnly

GitHub CLI (`gh`) must be installed and authenticated for automatic PR creation.

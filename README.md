# EKE-DX-WIRE — Deterministic Wire Reconstruction Engine

Version: 0.1.1-foundation  
Language: C++23  
Build: CMake 3.24+  
Primary CV dependency: OpenCV 5.x
Also requires: libcurl (for the optional Anthropic vision recognition provider)

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

The extractor has progressed from conductor geometry extraction into a multi-stage engineering reconstruction pipeline.

Implemented:

- C++23/CMake project structure.
- OpenCV-based raster processing.
- Native Windows PDF first-page rendering.
- adaptive thresholding.
- horizontal/vertical morphology.
- conductor candidate detection and normalization.
- shape detection and non-wire geometry exclusion.
- topology graph construction.
- gap interpretation.
- endpoint reconstruction.
- endpoint semantic reconstruction.
- connector and connector-terminal modeling.
- component identity evidence and deterministic resolution.
- injectable component identity registry/canonicalization boundary.
- component symbol recognition model boundary.
- electrical-net resolution.
- deterministic wire reconstruction.
- semantic evidence and recognition-provider boundaries.
- canonical extraction artifacts.
- structured conductor SVG projection.
- visual extraction review layers.
- automatic extraction-review publication to main.

The current system distinguishes observed conductor geometry from engineering wire identity. It does not treat every detected line as a wire.

### Current engineering-model limitation

AP-WIRE-021 currently establishes the component-symbol recognition boundary but does not yet perform deep visual recognition of specific electrical symbols. The latest TRX300 extraction demonstrates that component regions are being detected substantially more reliably than their internal symbol geometry.

The next engineering correction is internal symbol geometry extraction followed by true symbol recognition.

For the complete project history, current metrics, architecture, and remaining gaps, see:

    docs/PROJECT_STATUS_AND_GAP_ANALYSIS.md

## Development GUI

The Windows build includes an optional development instrument:

    dx-extractor-gui

It uses OpenCV HighGUI and the native Windows file picker. It displays the
source diagram, overlays detected conductor segments, and can display topology
nodes. The GUI is a thin client over libeke_dx_wire and contains no extraction
algorithms.

This is not the final OEP/EKE user interface.


## Release automation

The Windows development GUI includes a RELEASE action.

The action launches:

    tools/dx-release.ps1

The current release validation pipeline:

1. verifies the repository and main branch.
2. pulls the latest main.
3. configures the CMake Release build.
4. builds all Release targets serially.
5. runs the complete Release CTest suite.
6. verifies that the Release GUI exists.
7. relaunches the GUI.

The release validation workflow does not commit, push, create pull requests, stash changes, or reset the working tree.

### Extraction review publication

Each successful extraction generates:

    artifacts/extraction_review/

with source, wire, wire-color, symbol, terminal, connector, splice, ground, label, topology, bounds, endpoint, recognition, and combined review images plus review_manifest.json.

The review directory is regenerated for each extraction.

The review publisher is:

    tools/dx-publish-review.ps1

It stages only the extraction-review artifacts and publishes the review commit to main. It is intended to make extraction AARs repeatable without manually taking GUI screenshots.


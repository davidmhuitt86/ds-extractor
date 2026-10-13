# AP-DIAG-049 — Connector Boundary / Conductor Ownership Investigation

Status: baseline finding validated on Windows/OpenCV 5.0.0; AP-DIAG-FIX-012 implementation is proposed on a branch stacked on this diagnostic.

## User-provided source interpretation

The user identified the rectangular body in the focus crop as a connector and the symbol immediately above it as a diode. The region in the canonical samples/trx300ODG.png image is near (347, 378). The post-AP-DIAG-FIX-009 production model has a conductor-end node at (347, 378) on the extracted segment with geometry (347, 351)--(347, 378).

This is a source-specific ground-truth observation, not an assumption that every similar rectangle or neighboring line is non-electrical.

## Question being investigated

Does the pre-topology GeometryOwnershipClassifier have the detected connector-body geometry available when deciding whether a morphology line is an electrical conductor, and, for the segment near (347, 378), does that line trace a connector-body boundary without tracing any of the earlier shape-derived ComponentCandidate boundaries?

No causal conclusion is accepted until the AP-DIAG-049 report is inspected. The diagnostic deliberately reports both possibilities and does not change production extraction logic.

## Existing production stage relationship

The validated baseline used this order:

1. ShapeDetector and TextRegionDetector
2. MorphologyWireDetector
3. ComponentCandidateClassifier, CircleContextClassifier, and DiagramFurnitureClassifier
4. GeometryOwnershipClassifier
5. ConductorEvidenceEvaluator and ConductorNormalizer
6. TopologyReconstructor, gap interpretation, endpoint reconstruction
7. ConnectorGeometryDetector and downstream connector-native processing

AP-DIAG-FIX-012 proposes moving ConnectorGeometryDetector before GeometryOwnershipClassifier and supplying its regions to that classifier. The classifier reconstructs an expanded housing envelope only when raw morphology contains two compatible vertical strokes and both connecting horizontal caps. The same resolved bounds are then reused for connector terminal observation. The updated stage-order fields and connector_boundary_resolutions_near_focus array make that behavior auditable.

## Diagnostic contents

The target dx-audit-connector-boundary-ownership emits JSON describing:

- Canonical image dimensions and focus region (default x=320, y=335, width=70, height=65, configurable on the command line).
- Shape regions and shape-derived component candidates intersecting that region.
- Text regions, independently detected connector-body regions, and final production connector candidates in the same region.
- All raw morphology segments intersecting the focus region, their ownership retention/rejection result and reason, component-boundary trace matches, connector-body-boundary trace matches, connector bounding-box overlap, conductor-evidence outcome, and exact-geometry retention through normalization.
- Production normalized conductor segments, topology nodes/edges, and endpoint candidates near the region.
- Total stage populations for context and reproducibility.

The report now exercises the same connector-region-aware ownership classifier used by the production pipeline. Its local boundary-trace fields are observational; the actual accept/reject decision is made by GeometryOwnershipClassifier. This AP does not merge topology nodes, alter snap tolerances, or use coordinate-specific rules.

The diagnostic recreates the relevant pre-topology stage inputs using default ExtractionConfig values. It separately runs the complete production pipeline under the same source path/source ID and reports that final model for comparison. No report count is hard-coded as an acceptance oracle.

## Build and run (Windows PowerShell)

Configure if needed:

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

Build the AP-specific executable:

    cmake --build build --config Release --target dx-audit-connector-boundary-ownership --parallel 1

Run against the canonical TRX300 image:

    .\build\Release\dx-audit-connector-boundary-ownership.exe samples\trx300ODG.png artifacts\audit\AP-DIAG-049_connector_boundary_ownership.json

The focus rectangle can be overridden by four numeric arguments after the output path:

    .\build\Release\dx-audit-connector-boundary-ownership.exe samples\trx300ODG.png artifacts\audit\AP-DIAG-049_connector_boundary_ownership.json 320 335 70 65

Validate that the output parses:

    $r = Get-Content "artifacts\audit\AP-DIAG-049_connector_boundary_ownership.json" -Raw | ConvertFrom-Json
    $r.population | Format-List
    $r.shape_regions_near_focus | Format-Table
    $r.component_candidates_near_focus | Format-Table
    $r.connector_geometry_regions_near_focus | Format-Table
    $r.raw_morphology_segments_near_focus | Format-List
    $r.production_topology_near_focus | ConvertTo-Json -Depth 8

## Review questions / acceptance requirements

1. Confirm source width/height are 898x549 and the report declares production_logic_modified=false for the diagnostic itself, plus production_logic_modified_by_ap_diag_fix_012_branch=true when run on the stacked implementation branch.
2. Identify the candidate segment(s) with endpoints or geometry matching the reported connector-left boundary around (347, 351)--(347, 378).
3. Determine whether the matching segment was retained by the actual shape-derived component ownership classifier and, if not rejected, whether it is later accepted as conductor evidence and present in the production normalized segments.
4. Compare component_boundary_trace_ids with connector_geometry_boundary_trace_ids. If the segment traces a connector region but not a component candidate and is retained by ownership, that supports a stage-availability gap as the explanation. If it is already rejected or a component candidate boundary owns it, investigate the exact reason instead of presuming a defect.
5. Verify the production topology edges and endpoint at (347, 378) against the underlying source image; IDs alone are not evidence because topology IDs can change when representative coordinates change.
6. Do not infer that the diode itself has been electrically identified; current ComponentCandidate kinds are coarse geometry classifications, not symbol-family recognition.
7. Run a clean Release build and all CTest tests after adding the diagnostic. Review JSON validity and confirm the working tree contains no accidental generated artifacts.

## Scope / non-goals

- AP-DIAG-049 itself remains a diagnostic; the production correction is separately scoped as AP-DIAG-FIX-012.
- No coordinate-specific rule for (347, 378).
- No snap-tolerance adjustment or node merge.
- The implementation may reject only a line-supported four-sided connector housing frame; conductor leads ending at that frame must remain.
- AP-DIAG-FIX-009 remains a separate source-ID invariance change and still needs its own acceptance gate.

# AP-DIAG-049 — Connector Boundary / Conductor Ownership Investigation

Status: diagnostic-only implementation on a branch stacked on AP-DIAG-FIX-009; local Windows/OpenCV 5.0.0 build and report review pending.

## User-provided source interpretation

The user identified the rectangular body in the focus crop as a connector and the symbol immediately above it as a diode. The region in the canonical samples/trx300ODG.png image is near (347, 378). The post-AP-DIAG-FIX-009 production model has a conductor-end node at (347, 378) on the extracted segment with geometry (347, 351)--(347, 378).

This is a source-specific ground-truth observation, not an assumption that every similar rectangle or neighboring line is non-electrical.

## Question being investigated

Does the pre-topology GeometryOwnershipClassifier have the detected connector-body geometry available when deciding whether a morphology line is an electrical conductor, and, for the segment near (347, 378), does that line trace a connector-body boundary without tracing any of the earlier shape-derived ComponentCandidate boundaries?

No causal conclusion is accepted until the AP-DIAG-049 report is inspected. The diagnostic deliberately reports both possibilities and does not change production extraction logic.

## Existing production stage relationship

In ExtractionPipeline::run() the current production order is:

1. ShapeDetector and TextRegionDetector
2. MorphologyWireDetector
3. ComponentCandidateClassifier, CircleContextClassifier, and DiagramFurnitureClassifier
4. GeometryOwnershipClassifier
5. ConductorEvidenceEvaluator and ConductorNormalizer
6. TopologyReconstructor, gap interpretation, endpoint reconstruction
7. ConnectorGeometryDetector and downstream connector-native processing

Therefore, connector regions from the separate ConnectorGeometryDetector are not passed into the earlier GeometryOwnershipClassifier call. Earlier shape-derived component candidates may still represent some connector-like bodies, so the diagnostic measures that rather than assuming the rectangle is absent from those candidates.

## Diagnostic contents

The target dx-audit-connector-boundary-ownership emits JSON describing:

- Canonical image dimensions and focus region (default x=320, y=335, width=70, height=65, configurable on the command line).
- Shape regions and shape-derived component candidates intersecting that region.
- Text regions, independently detected connector-body regions, and final production connector candidates in the same region.
- All raw morphology segments intersecting the focus region, their ownership retention/rejection result and reason, component-boundary trace matches, connector-body-boundary trace matches, connector bounding-box overlap, conductor-evidence outcome, and exact-geometry retention through normalization.
- Production normalized conductor segments, topology nodes/edges, and endpoint candidates near the region.
- Total stage populations for context and reproducibility.

The diagnostic's local boundary-trace calculation mirrors the narrow AP-DIAG-FIX-001 axis-aligned boundary check. This is observational only; it does not pass connector regions into the classifier, modify ownership, change a tolerance, suppress a segment, or merge topology nodes.

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

1. Confirm source width/height are 898x549 and the report declares production_logic_modified=false.
2. Identify the candidate segment(s) with endpoints or geometry matching the reported connector-left boundary around (347, 351)--(347, 378).
3. Determine whether the matching segment was retained by the actual shape-derived component ownership classifier and, if not rejected, whether it is later accepted as conductor evidence and present in the production normalized segments.
4. Compare component_boundary_trace_ids with connector_geometry_boundary_trace_ids. If the segment traces a connector region but not a component candidate and is retained by ownership, that supports a stage-availability gap as the explanation. If it is already rejected or a component candidate boundary owns it, investigate the exact reason instead of presuming a defect.
5. Verify the production topology edges and endpoint at (347, 378) against the underlying source image; IDs alone are not evidence because topology IDs can change when representative coordinates change.
6. Do not infer that the diode itself has been electrically identified; current ComponentCandidate kinds are coarse geometry classifications, not symbol-family recognition.
7. Run a clean Release build and all CTest tests after adding the diagnostic. Review JSON validity and confirm the working tree contains no accidental generated artifacts.

## Scope / non-goals

- Diagnostic only; no changes to production classes, thresholds, ownership logic, connector recognition, topology, or wire identity.
- No coordinate-specific rule for (347, 378).
- No snap-tolerance adjustment or node merge.
- No deletion of connector-boundary-like geometry until real conductor leads to connector pins are distinguished from the housing outline.
- This AP does not accept AP-DIAG-FIX-009 or replace its source-ID invariance regression. A future production correction requires evidence from this report and a separate narrowly scoped implementation/test AP.

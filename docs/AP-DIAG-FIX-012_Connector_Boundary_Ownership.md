# AP-DIAG-FIX-012 — Connector Boundary Ownership Before Topology

## Status

Implementation proposed; awaiting local Windows Release build, full CTest, and canonical TRX300 extraction/artifact review.

## Root cause established by AP-DIAG-049

The connector detector's region near the source connector is bounded at x=355..372 even though morphology yields an outer rectangular frame extending to x=347. The production ownership classifier ran before connector detection, so it could not use the connector body as a boundary owner. In the baseline report, both vertical housing strokes and their connecting horizontal caps survived ownership and entered topology.

The defect is not a reason to erase all geometry within a connector bounding box. Real horizontal conductors terminate at the connector boundary and must remain conductor evidence.

## Implementation

1. Run ConnectorGeometryDetector before GeometryOwnershipClassifier.
2. Pass connector body regions into the ownership classifier.
3. Reconstruct an effective connector housing envelope only when raw morphology provides one near-edge vertical stroke, a parallel outer vertical stroke with matching top/bottom extent, and separate horizontal strokes connecting the same two verticals at both ends.
4. Exclude only candidate segments that trace one of the reconstructed rectangle's four edges, using the existing narrow boundary-trace tolerance. No proximity-only suppression is added.
5. Reuse the resolved bounds in downstream connector and pin observations, including leads that terminate exactly at the boundary and therefore have zero in-bounds overlap.
6. Extend AP-DIAG-049 output to report detected versus effective connector bounds and to run the boundary-aware ownership path. The diagnostic itself does not modify production logic.

If the four-sided frame cannot be established, the detector bounds are not expanded and no additional connector-boundary suppression is applied.

## Unit-test acceptance

The regression fixture reproduces the observed geometry generically: detected bounds x=355/y=352/17x26; vertical strokes at x=347 and x=371; top/bottom caps connecting them; genuine leads that meet the left/right boundary perpendicularly; and a nearby unconnected parallel line.

Required assertions:
- effective bounds expand to the full observed frame;
- exactly the four housing strokes are rejected as ConnectorAssociated;
- every lead is retained;
- the unrelated nearby line is retained;
- removing the cap evidence disables expansion and leaves all candidate segments retained.

## Required local validation

Run from the repository root in Windows PowerShell:

    git pull --ff-only
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release --clean-first --parallel 1
    if ($LASTEXITCODE -ne 0) { throw "Clean Release build failed." }
    ctest --test-dir build -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Full CTest suite failed." }
    .\build\Release\dx-audit-connector-boundary-ownership.exe samples\trx300ODG.png artifacts\audit\AP-DIAG-049_connector_boundary_ownership.json
    if ($LASTEXITCODE -ne 0) { throw "AP-DIAG-049 runtime failed." }
    $r = Get-Content "artifacts\audit\AP-DIAG-049_connector_boundary_ownership.json" -Raw | ConvertFrom-Json
    $r.population | Format-List
    $r.connector_geometry_regions_near_focus | Format-Table
    $r.connector_boundary_resolutions_near_focus | Format-List
    $r.raw_morphology_segments_near_focus | Format-List
    $r.production_topology_near_focus | ConvertTo-Json -Depth 8
    git diff --check
    git status --short

## Extraction review requirements

- Verify boundary resolution reports the detected bounds and the evidence-supported effective bounds.
- Confirm the connector's housing strokes no longer appear in normalized conductor segments or topology edges.
- Confirm lead segments ending on the boundary remain present and are observed as connector interactions.
- Compare all counts and IDs against the AP-DIAG-049 baseline. Explain every changed node/edge/wire/endpoint in the focus area; raw count reductions alone are not proof.
- Inspect connector and topology review images; verify pin observations correspond to actual conductors, not the removed housing strokes.
- Run the mandatory main-only AP gate after normal integration. Do not merge based only on the synthetic unit test or a passing CTest suite.

## Scope restrictions

No coordinate-specific special case for (347,378), no global topology snap changes, no forced node merges, no change to Wire identity rules, and no blanket masking of connector bounds. Connector semantic materialization must not invent endpoints or wires.

# AP-DIAG-FIX-012 — Connector Boundary Ownership Before Topology

## Status

Connector-boundary ownership implementation and diagnostic audit extension are committed on a branch stacked on AP-DIAG-049. The maintainer reported a clean Release build and full CTest passing before the latest diagnostic-only audit extension. Rebuild and rerun the complete validation sequence against the latest branch head; this branch remains unaccepted pending evidence and topology review.

## Root cause established by AP-DIAG-049

The connector detector's region near the source connector is bounded at x=355..372 even though morphology yields an outer rectangular frame extending to x=347. The production ownership classifier ran before connector detection, so it could not use the connector body as a boundary owner. In the baseline report, both vertical housing strokes and their connecting horizontal caps survived ownership and entered topology.

The defect is not a reason to erase all geometry within a connector bounding box. Real horizontal conductors terminate at the connector boundary and must remain conductor evidence.

## Implementation

1. Run ConnectorGeometryDetector before GeometryOwnershipClassifier.
2. Pass connector body regions into the ownership classifier.
3. Reconstruct an effective connector housing envelope only when raw morphology provides one near-edge vertical stroke, a parallel outer vertical stroke with matching top/bottom extent, and separate horizontal strokes connecting the same two verticals at both ends.
4. Exclude only candidate segments that trace one of the reconstructed rectangle's four edges, using the existing narrow boundary-trace tolerance. No proximity-only suppression is added.
5. Reuse the resolved bounds in downstream connector and pin observations, including leads that terminate exactly at the boundary and therefore have zero in-bounds overlap.
6. Extend AP-DIAG-049 output to report detected versus effective connector bounds and to run the boundary-aware ownership path. Also serialize production pins, conductor-crossing observations, terminal associations, and connector terminals so every interaction can be traced to its source segment and endpoint. The diagnostic itself does not modify production logic.

If the four-sided frame cannot be established, the detector bounds are not expanded and no additional connector-boundary suppression is applied.

## Unit-test acceptance

The regression fixture reproduces the observed geometry generically: detected bounds x=355/y=352/17x26; vertical strokes at x=347 and x=371; top/bottom caps connecting them; genuine leads that meet the left/right boundary perpendicularly; and a nearby unconnected parallel line.

Required assertions:
- effective bounds expand to the full observed frame;
- exactly the four housing strokes are rejected as ConnectorAssociated;
- every lead is retained;
- the unrelated nearby line is retained;
- removing the cap evidence disables expansion and leaves all candidate segments retained.

## Association materialization audit added after first review

The diagnostic now emits `production_connector_associations_resolved`,
`production_connector_associations_unresolved`,
`production_connector_associations_conflicted`, and
`production_connector_associations_materialized` in the population record.
The nested `production_connector_association_materialization_outcomes` records
why an association did not produce a terminal, including unresolved/conflicted
association status, an empty endpoint reference, missing connector/pin/endpoint
references, or a resolved association whose expected terminal object was not
found.

The new `connector_terminal_materialization_audit` array audits every
connector-terminal association across the source and records `pin_in_focus` so
the investigated connector can be separated from other source locations. It records the expected stable
terminal ID, whether that terminal actually exists, association and pin
confidence, connector/pin/endpoint reference presence, available crossing
evidence (segment ID, interaction, confidence, location, provenance stage),
and nearby endpoint candidates within the production association stage's 6 px
distance gate. For each nearby endpoint it lists incident topology edges that
refer to a conductor segment observed at the pin. Endpoint kind/role/confidence
are labelled as snapshots after later semantic stages; they must not be
mistaken for the earlier endpoint values consumed by the terminal builder.

This is diagnostic-only. It does not adjust confidence, status, topology,
terminal creation, or extraction thresholds. In particular, it records the
low-confidence/unknown-interaction situation rather than changing it. Review
the association status counts and evidence before deciding whether a production
semantic rule needs a separate AP.

## Global connector-candidate gate audit added 2026-10-14

The diagnostic now serializes full-source inventories (not only objects near the focus region):

- `connector_geometry_regions_all`
- `connector_boundary_resolutions_all`
- `production_connector_candidates_all`
- `production_connector_pins_all`
- `production_connector_crossings_all`
- `production_connector_terminal_associations_all`
- `production_connector_terminals_all`

The new `connector_observation_gate_audit_all` array has one record per detected connector geometry region. It maps the detector region to the expected stable production connector ID, reports detected and effective bounds, and indicates whether a rectangular-stroke boundary was reconstructed and whether a production connector candidate exists.

For each region it compares three observation gates over the current post-ownership normalized conductor segments:
- `legacy_gate_interaction_count`: detected bounds and the previous minimum 3 px overlap rule.
- `effective_bounds_strict_gate_interaction_count`: resolved effective bounds but still the previous minimum 3 px overlap rule.
- `current_gate_interaction_count`: effective bounds and the new rule that also admits a terminating endpoint within the existing 2 px expanded-boundary margin even when in-bounds overlap is under 3 px.

The record contains `newly_admitted_segments` with each segment ID, overlap values, endpoint-nearness flags, and an admission reason. This is a diagnostic counterfactual, not a baseline replay: `counterfactual_reuses_current_normalized_segments` is deliberately true. It isolates the gate/bounds difference while holding the current normalized conductor population constant. It must not be represented as a complete reconstruction of the pre-fix production model.

Use these results to determine whether an off-focus candidate existed under the legacy gate or is admitted only by the new termination-contact exception. Do not change production code based only on the global candidate count.

## Required local validation

Run from the repository root in Windows PowerShell:

    git fetch origin
    git switch --track origin/ap/ap-diag-fix-012-connector-boundary-ownership
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
    $r.production_connector_pins_near_focus | Format-Table
    $r.production_connector_crossings_near_focus | Format-List
    $r.production_connector_terminal_associations_near_focus | Format-List
    $r.production_connector_terminals_near_focus | Format-List
    $r.population | Select-Object production_connector_associations_resolved, production_connector_associations_unresolved, production_connector_associations_conflicted, production_connector_associations_materialized, production_connector_association_materialization_outcomes | Format-List
    $r.connector_terminal_materialization_audit | ConvertTo-Json -Depth 10
    $r.raw_morphology_segments_near_focus | Format-List
    $r.production_topology_near_focus | ConvertTo-Json -Depth 8
    git diff --check
    git status --short

## Extraction review requirements

- Verify boundary resolution reports the detected bounds and the evidence-supported effective bounds. The diagnostic JSON should also state production_logic_modified=false for the diagnostic itself and production_logic_modified_by_ap_diag_fix_012_branch=true for this stacked implementation branch.
- Confirm the connector's housing strokes no longer appear in normalized conductor segments or topology edges.
- Confirm lead segments ending on the boundary remain present and are observed as connector interactions in production_connector_crossings_near_focus.
- Review every local pin's crossing confidence/interaction, the association status, materialization outcome/reason, expected terminal ID, nearby endpoints within 6 px, and incident-edge evidence tying each endpoint to a pin-observed conductor segment. A resolved status alone is not proof that a terminal object was materialized correctly.
- Reconcile global association status counts with materialization outcomes. Every resolved association should either have its exact expected terminal object or a reported missing-materialization discrepancy. Treat endpoint kind/role in the diagnostic as a later-stage snapshot, not a reconstruction of the earlier builder-stage endpoint.
- Compare all counts and IDs against the AP-DIAG-049 baseline. Explain every changed node/edge/wire/endpoint in the focus area; raw count reductions alone are not proof.
- Inspect connector and topology review images; verify pin observations correspond to actual conductors, not the removed housing strokes.
- Run the mandatory main-only AP gate after normal integration. Do not merge based only on the synthetic unit test or a passing CTest suite.

## Scope restrictions

No coordinate-specific special case for (347,378), no global topology snap changes, no forced node merges, no change to Wire identity rules, and no blanket masking of connector bounds. Connector semantic materialization must not invent endpoints or wires.

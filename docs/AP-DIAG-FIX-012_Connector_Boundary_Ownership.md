# AP-DIAG-FIX-012 — Connector Boundary Ownership Before Topology

## Status

Connector-boundary ownership, the global connector-observation gate audit, and the evidence-scoped short-contact policy are committed on a branch stacked on AP-DIAG-049. The supplied global audit showed that the unrestricted sub-3 px termination exception admitted a second off-focus interaction without reconstructed rectangular-frame evidence. The production gate and regression assertions are now narrowed accordingly. The latest source and diagnostic changes have not been built or executed locally; rebuild and rerun the complete validation sequence against the exact latest branch head. This branch remains unaccepted pending that evidence and topology review.

## Root cause established by AP-DIAG-049

The connector detector's region near the source connector is bounded at x=355..372 even though morphology yields an outer rectangular frame extending to x=347. The production ownership classifier ran before connector detection, so it could not use the connector body as a boundary owner. In the baseline report, both vertical housing strokes and their connecting horizontal caps survived ownership and entered topology.

The defect is not a reason to erase all geometry within a connector bounding box. Real horizontal conductors terminate at the connector boundary and must remain conductor evidence.

## Implementation

1. Run ConnectorGeometryDetector before GeometryOwnershipClassifier.
2. Pass connector body regions into the ownership classifier.
3. Reconstruct an effective connector housing envelope only when raw morphology provides one near-edge vertical stroke, a parallel outer vertical stroke with matching top/bottom extent, and separate horizontal strokes connecting the same two verticals at both ends.
4. Exclude only candidate segments that trace one of the reconstructed rectangle's four edges, using the existing narrow boundary-trace tolerance. No proximity-only suppression is added.
5. Reuse the resolved bounds downstream for connector and pin observations. Permit a sub-3 px termination contact only when the complete rectangular-stroke housing boundary was reconstructed; otherwise retain the legacy 3 px overlap floor.
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

The association-materialization audit itself is diagnostic-only. It does not
adjust confidence/status or terminal creation. The separate gate correction
requires reconstructed rectangular-stroke boundary evidence before admitting
sub-3 px termination contacts; otherwise, the legacy 3 px overlap floor remains
in force.

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

For each region it compares four gate populations over the current post-ownership normalized conductor segments:
- `legacy_gate_interaction_count`: detected bounds and the prior minimum 3 px overlap rule.
- `effective_bounds_strict_gate_interaction_count`: resolved effective bounds with the minimum 3 px overlap rule still enforced.
- `prior_unrestricted_gate_interaction_count`: the previous AP-DIAG-FIX-012 behavior, which admitted a termination contact under the effective bounds even if the overlap was under 3 px, regardless of whether a rectangular frame had been reconstructed.
- `current_gate_interaction_count`: the corrected production policy. An overlap of at least 3 px is accepted normally; below that threshold, termination contact is accepted only when the connector housing boundary was reconstructed from the complete rectangular-stroke evidence.

The first execution of this audit identified the candidate at detected bounds `(90,98,20,21)` as having no reconstructed boundary, with one legacy interaction and two interactions under the unrestricted termination-contact rule. Its second interaction came from `normalized-conductor-segment-008f738f891842bc` at only 2 px overlap. That segment caused the region to meet the two-interaction candidate threshold only under the unrestricted exception. This is not enough evidence to assert the geometry can never be a real connector; it is enough to show that the exception was broadening recognition without the independent boundary evidence required for this AP.

Production now uses the shared `connector_observation_passes_overlap_gate` policy: the short-contact exception requires `reconstructed_from_rectangular_strokes=true`. The target connector continues to qualify because its frame was reconstructed; regions without that evidence retain the legacy 3 px floor. A focused regression assertion was added to `test_connector_geometry_detector` for short contacts both with and without reconstructed-boundary evidence.

The audit also reports `suppressed_without_reconstructed_boundary_count` and `suppressed_without_reconstructed_boundary_segments` so the former unrestricted behavior can be directly inspected. `newly_admitted_segments` records segments newly admitted by the corrected policy relative to the legacy detected-bounds gate.

This is a diagnostic counterfactual, not a baseline replay: `counterfactual_reuses_current_normalized_segments` is deliberately true. It isolates the overlap/bounds policy while holding the current normalized conductor population constant. It must not be represented as a complete reconstruction of the pre-fix production model.

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

# AP-DIAG-FIX-007 — Ground Symbol Approach-Conductor Detection

This is a production fix. It adds one new, narrowly-scoped recovery class
(`GroundApproachConductorRecovery`) and wires it into the existing
extraction pipeline between `GeometryOwnershipClassifier` and
`ConductorEvidenceEvaluator`. No global morphology parameter, no
classifier threshold, and no downstream topology/endpoint/net stage was
modified.

## 1. Starting SHA

- `git rev-parse HEAD` (before this fix): `6e8f2d07daf538b54e6226dd71d864e9cd97c040`
  (AP-DIAG-AUDIT-006 — audit-only, no production change).
- `git branch --show-current`: `main`
- `git status --short` (before work): empty (clean).

## 2. Audit Finding This Fix Addresses

AP-DIAG-AUDIT-006 established that two genuine `ChassisGround` components —
`component-candidate-shape-region-5aa211846bb7891d` and
`component-candidate-shape-region-6b6ccc2d59afe578` — never received a
resolved `Ground` endpoint, while 4 other genuine `ChassisGround`
components did. The audit traced this to `MorphologyWireDetector`
producing **zero** conductor geometry for the final approach segment
connecting each of these two symbols to its parent circuit element, even
though the ink is present, continuous, and unambiguous in
`samples/trx300ODG.png`. `TerminalLocationDetector`, `TerminalRecognizer`,
`ConductorBoundaryResolver`, `EndpointSemanticReconstructor`, and
`boundary_tolerance` were all confirmed correct given their (incomplete)
inputs.

## 3. Root Cause (Unchanged From the Audit)

`MorphologyWireDetector` extracts conductor geometry using exactly two
fixed-orientation morphological openings — a 25×1 horizontal
`MORPH_RECT` kernel and a 1×25 vertical `MORPH_RECT` kernel — plus a
`minimum_segment_length = 12` post-filter. Two shapes cannot survive
this:

- **5aa211846bb7891d**: a diagonal jog (~(707,533)→(719,538)) into a short
  (~10px) vertical run down to the ground bar. No horizontal-or-vertical
  kernel can preserve diagonal ink at any length, and the vertical run
  alone is under the 12px minimum.
- **6b6ccc2d59afe578**: an L-bend into a ~17-20px vertical run. The
  audit's control experiment replicating the vertical opening exactly
  showed 134 raw ink pixels reduced to 0 surviving pixels — the run is
  simply too short and too close to the bend for a 1×25 opening to keep
  any of it.

## 4. Design Constraints Honored

- Global morphology parameters (`horizontal_kernel_length`,
  `vertical_kernel_length`, `minimum_segment_length`) are **unchanged**.
  No diagram-wide diagonal detector was added.
- The fix is **local**: it only runs once per already-detected
  `ChassisGround` region, over a small bounded region-of-interest (ROI)
  anchored to that region's own geometry, never over the whole diagram.
- Recovered geometry is fed into the pre-existing
  `ConductorSegment` model and flows through the pre-existing
  `ConductorEvidenceEvaluator` → boundary/topology → endpoint semantics
  pipeline unmodified. No `EndpointCandidate` or `Ground` endpoint is
  manufactured directly.

## 5. Existing Architecture Reused

- `ShapeDetector`'s already-committed `ChassisGround` `ShapeRegion` output
  (kind, role, bounds) is the anchor — no new symbol detection was added.
- The existing binary threshold image (`detected.binary`, the same
  adaptive threshold `MorphologyWireDetector` itself consumes) and the
  existing combined exclusion mask are reused as-is — no new
  binarization.
- `cv::connectedComponentsWithStats` (already used elsewhere in this
  codebase, e.g. `MorphologyWireDetector` itself) drives the local
  connectivity analysis instead of a bespoke parallel wire model.
- `cv::approxPolyDP` (already used in `ShapeDetector`, requiring the same
  non-standard `<opencv2/geometry/2d.hpp>` include this OpenCV build
  needs) simplifies the traced centerline to a small polyline that
  preserves real bends instead of collapsing it to one straight line.
- Recovered segments are ordinary `ConductorSegment` values with
  content-addressed IDs from the existing `stable_id()` helper, merged
  into `ownership.conductor_candidates` before
  `ConductorEvidenceEvaluator::evaluate()` — the same evidence-evaluation,
  boundary-resolution, topology-reconstruction, and endpoint-semantic
  stages that process every other conductor in the pipeline process these
  identically.

### Investigation of `ground_stem_search_height`

Before designing new logic, the existing 14px `ground_stem_search_height`
corridor in `ShapeDetector::evaluate_ground_run()` was inspected. It is a
**presence-check** only (`cv::countNonZero(binary(stem_region)) < 2`
confirms "something connects here" while classifying the ground symbol
itself) — it is never used to extract or preserve conductor geometry, and
AP-DIAG-FIX-003 already narrowed the actual *exclusion* mask to just the
bars plus a 2px margin, which does not reach the approach-conductor ink
for either affected component. Reusing or widening this corridor would
not have produced conductor geometry even if it were repurposed, so a
new, purpose-built recovery path was required.

## 6. Recovery Algorithm

`GroundApproachConductorRecovery::recover()`, for each `ChassisGround`
`ShapeRegion`:

1. Skip if an existing `ConductorSegment` (from `MorphologyWireDetector`)
   already reaches within `already_covered_distance` (8px) of the
   symbol's anchor point (top-center of its bounding box) — this is a
   gap-filler only, never a competing extraction.
2. Crop a small ROI: `corridor_half_width` (12px) on each side of the
   anchor x-coordinate, `search_height` (20px) above the anchor y.
3. Apply the existing exclusion mask to the ROI (so text, other symbols,
   and already-claimed component geometry are never read).
4. Run `cv::connectedComponentsWithStats` on the ROI; select the
   connected component whose bottom-row run is nearest the ROI's
   horizontal center (the component actually touching the ground
   symbol's approach point).
5. Trace a centerline bottom-up (anchor-first), row by row, choosing the
   nearest run to the previous row's position at each step. This
   tolerates the disjoint per-row runs a diagonal jog or an L-bend
   produces near the bend itself (observed up to 3 disjoint runs in one
   row for the real fixtures) via `max_disjoint_runs_per_row` (6) and
   `max_row_to_row_jump` (24px) bounds — exceeding either aborts the
   trace as ambiguous rather than guessing.
6. Reject (as ambiguous) if any row's matched run is wider than
   `max_run_width` (30px) — a wide blob (component outline, filled
   shape) is out of scope for this "thin approach wire" recovery.
7. Simplify the traced centerline with `cv::approxPolyDP`
   (`polyline_simplify_epsilon = 1.5`) into a small polyline that keeps
   real bends instead of forcing one straight segment.
8. Emit one `ConductorSegment` per polyline edge, each with a
   content-addressed `stable_id()`, `Medium` confidence, and
   `provenance.stage = "ground_approach_recovery"`.

All segments for all `ChassisGround` regions are sorted by ID before
being returned, so output ordering is deterministic and independent of
detection order.

### Integration point

The recovery call happens **after** `GeometryOwnershipClassifier` and
**before** `ConductorEvidenceEvaluator`, not immediately after
`MorphologyWireDetector`. `GeometryOwnershipClassifier`'s generic
component/text bounding-box overlap check is calibrated for ordinary long
conductor runs; it incorrectly classified the recovered segment for
6b6ccc2d59afe578 as `text_associated` because that short segment
geometrically passes beneath the bounding box of a nearby, unrelated text
region (`text-region-51`, the "M" label inside a motor circle drawn just
above the ground symbol) — a real ambiguous overlap in the source
diagram's layout, not a defect in the recovered geometry. The recovery's
own bounded, connectivity-based trace — anchored to a genuine
`ChassisGround` symbol and requiring an unbroken ink path reaching it — is
itself the ownership determination for this narrow case, so recovered
segments bypass only that one generic overlap check. They still pass
through `ConductorEvidenceEvaluator`'s raster ink-support check and every
stage after it exactly like any other conductor; that evaluator correctly
trims weaker sub-segments of the recovered polyline unchanged (see
Section 12, `rejected_geometry`).

## 7. Raster Evidence Basis

All recovered geometry derives from `cv::connectedComponentsWithStats`
run directly on the same binary threshold image the rest of the pipeline
uses — there is no synthetic geometry from known coordinates. The unit
test suite includes an explicit "no ink" negative control
(`tests/test_ground_approach_conductor_recovery.cpp`) proving recovery
produces nothing when the source pixels are absent, and a "disconnected
ink" negative control proving unrelated nearby ink not itself connected
to the anchor is never adopted.

## 8. Affected Component 1 — `component-candidate-shape-region-5aa211846bb7891d`

- Shape: diagonal jog into a short vertical run (the audit's Case 1).
- Recovered geometry terminates exactly at the ground symbol's own anchor
  point `(719, 547)`.
- Resulting `Ground` endpoint: `endpoint-candidate-489024c295007fa9`,
  `kind: ground`, `terminal_role: ground_terminal`, `confidence: high`,
  at `(719, 547)`.
- New wire `wire-5f577a77a7d7f3d1` connects this ground endpoint to a
  geometric endpoint at `(719, 534)` (the far end of the recovered
  approach conductor, where it meets pre-existing topology), confidence
  `medium`, `identity_status: resolved`.
- New `ElectricalNet` `electrical-net-ad8ebe90235978f9`: `role: ground`,
  `confidence: high`, anchored at the new ground endpoint.
- The recovered polyline is **not** collapsed to a single straight line —
  it preserves the diagonal-into-vertical bend, satisfying the geometric
  fidelity requirement.

## 9. Affected Component 2 — `component-candidate-shape-region-6b6ccc2d59afe578`

- Shape: L-bend into a short vertical run (the audit's Case 2, completely
  erased by the vertical morphological opening in isolation).
- Resulting `Ground` endpoint: `endpoint-candidate-ee69f47db68cb6bf`,
  `kind: ground`, `terminal_role: ground_terminal`, `confidence: high`,
  at `(759, 547)`.
- New wire `wire-cc1a42c6ede95cf1` connects this ground endpoint to a
  geometric endpoint at `(758, 532)`, confidence `medium`,
  `identity_status: resolved`.
- New `ElectricalNet` `electrical-net-647be111f38562cc`: `role: ground`,
  `confidence: high`, anchored at the new ground endpoint.
- The recovered polyline preserves the L-bend-into-vertical shape, not an
  artificial vertical line, and was correctly recovered despite being
  fully erased by the global vertical opening in isolation — confirming
  the local recovery, not any morphology-parameter change, produced this
  result.

## 10. Four Previously-Successful Ground Regressions

All 4 previously-resolved `ChassisGround` components are byte-identical
in the fixed output's `endpoint_candidates` compared to the pre-fix
baseline (verified by full-collection ID-indexed diff, Section 12):

| Component | x | y | confidence |
|---|---|---|---|
| `0edf5ce35037fec3` | 585.5 | 585 | high |
| `1acbaeb7ac6ac87a` | 629.5 | 585 | high |
| `a434a925670e9b65` | 935 | 542 | medium |
| `791276441805b2e0` | 656 | 546 | high |

No existing `Ground` endpoint disappeared, no existing `Wire` was
redirected, and no unrelated component gained a new endpoint.

## 11. False-Positive Controls

`tests/test_ground_approach_conductor_recovery.cpp` covers, against
synthetic fixtures reproducing both real shapes:

- Diagonal-jog case recovers a real, multi-segment (non-straight-line)
  path reaching the anchor.
- L-bend case recovers a real, multi-segment path.
- **Already covered**: an existing `ConductorSegment` reaching the anchor
  suppresses recovery entirely (gap-filler only, never competing).
- **No ink**: nothing is fabricated when no raster evidence exists.
- **Disconnected ink**: an isolated mark not connected to the anchor is
  never adopted.
- **Wide blob**: a filled rectangle crossing the corridor (with a tight
  `max_run_width` config) is rejected as out of scope rather than picking
  an arbitrary path through it.
- **Exclusion mask**: ink lying inside the exclusion mask (as for
  already-claimed component/text geometry) is not read at all.
- **Non-`ChassisGround` shapes**: a `Rectangle` shape with identical ink
  underneath it is never examined.
- **Determinism**: identical input produces identical segment IDs and
  geometry across repeated calls.

All 9 cases pass.

## 12. Before/After Metrics (Unscoped, Full Diagram)

Full `topology.json` diffed by ID between an isolated `git worktree`
rebuild of the pre-fix commit (`6e8f2d0`) and the fixed working tree,
both run against `samples/trx300ODG.png`:

| Collection | Before | After | Added | Removed | Changed |
|---|---|---|---|---|---|
| `component_candidates` | 81 | 81 | 0 | 0 | 0 |
| `component_symbol_recognitions` | 81 | 81 | 0 | 0 | 0 |
| `symbol_primitives` | 34 | 34 | 0 | 0 | 0 |
| `connector_candidates` | 0 | 0 | 0 | 0 | 0 |
| `connector_terminals` | 0 | 0 | 0 | 0 | 0 |
| `nodes` | 686 | 690 | 4 | 0 | 0 |
| `edges` | 876 | 878 | 2 | 0 | 0 |
| `endpoint_candidates` | 203 | 207 | 4 | 0 | 0 |
| `rejected_geometry` | 10 | 13 | 3 | 0 | 0 |
| `wires` | 35 | 37 | 2 | 0 | 0 |
| `wire_semantics` | 35 | 37 | 2 | 0 | 0 |
| `electrical_nets` | 10 | 12 | 2 | 0 | 0 |
| `conductor_boundary_resolutions` | 203 | 207 | 4 | 0 | 0 |

Every added item is fully explained by the two recovered approach
conductors: 2 new wires (one per affected component), 2 new ground-role
`ElectricalNet`s, 4 new endpoint candidates (a `ground` endpoint plus one
geometric far-end endpoint per component), 4 new topology nodes, 2 new
topology edges, 4 new `conductor_boundary_resolutions`, and 3 new
`rejected_geometry` entries (weaker sub-segments of the recovered
polylines correctly trimmed by the unmodified
`ConductorEvidenceEvaluator`). Zero removals, zero changes to any
existing item in any collection.

CLI summary line comparison:

| Metric | Before | After |
|---|---|---|
| topology edges | 876 | 878 |
| reconstructed wires | 35 | 37 |
| electrical nets | 10 | 12 |
| endpoint candidates | 203 | 207 |
| validation errors | 0 | 0 |
| validation warnings | 34 | 34 |

## 13. Scoped/Unscoped Results

Ran with `--scope fixtures/trx300/scope_production.json`:

| Metric | Before (scoped) | After (scoped) |
|---|---|---|
| topology edges | 641 | 643 |
| reconstructed wires | 35 | 37 |
| electrical nets | 10 | 12 |
| endpoint candidates | 185 | 189 |
| validation errors | 0 | 0 |
| validation warnings | 34 | 34 |

The same +2 wires / +2 nets / +4 endpoints delta appears in both scoped
and unscoped runs — behavior is consistent, with no scope-specific
fabrication. All 6 genuine `ChassisGround` components resolve a `Ground`
endpoint in both modes.

## 14. Wire/Topology/Electrical Net Effects

Both new `ElectricalNet`s are `role: ground`, `confidence: high`, each
anchored at its component's new `Ground` endpoint — matching the
semantics of the 4 pre-existing ground nets exactly (AP-WIRE-029/030/031
semantics, unmodified). Both new `Wire`s are `identity_status: resolved`,
`confidence: medium` (reflecting the `Medium` confidence assigned to
recovered `ConductorSegment`s themselves), each with two
`identity_evidence_ids` pointing at the corresponding
`conductor_boundary_resolutions`. Every new topology object is physically
explained by exactly the two recovered conductor paths described in
Sections 8-9; `ElectricalNetResolver` itself was not modified.

## 15. Determinism

- Unscoped: 2 independent extraction runs against the fixed pipeline
  produced deep-equal (`==`) `topology.json` output.
- Scoped: 2 independent extraction runs against the fixed pipeline with
  `scope_production.json` produced deep-equal `topology.json` output.
- The pre-fix baseline (isolated `git worktree` rebuild) was likewise
  confirmed deep-equal across 2 runs, ruling out any pre-existing
  nondeterminism this fix could have inherited.

## 16. Performance

The recovery pass runs at most once per `ChassisGround` `ShapeRegion` (6
regions in this diagram, 4 of which are skipped immediately by the
already-covered check in step 1). Each of the remaining candidates
performs `cv::connectedComponentsWithStats` and a bottom-up row scan over
a single 20×25px ROI — not a diagram-wide scan, and asymptotically
independent of image size or total conductor count. No measurable
increase in wall-clock extraction time was observed against the existing
sample.

## 17. Tests

- Before this fix: 58/58 passing (AP-DIAG-AUDIT-006 baseline), assertions
  active, 8 compiler warnings.
- After this fix: 60/60 passing (2 new: `dx-wire-test-ground-approach-conductor-recovery`,
  `dx-wire-test-ground-approach-recovery-production`), assertions active,
  **8** compiler warnings (clean rebuild, 0 new).
- Test-first verification: with the pipeline-integration changes
  stashed out (recovery class present but unwired), the unit test
  (`dx-wire-test-ground-approach-conductor-recovery`) still passed
  (it exercises the class directly), while the production-path test
  (`dx-wire-test-ground-approach-recovery-production`) failed with
  `Assertion 'resolved_count == 6' failed` — confirming the pipeline
  wiring, not merely the new class's existence, is what fixes the gap.

## 18. Explicit Non-Changes

No changes were made to: `TerminalLocationDetector` semantics,
`TerminalRecognizer` thresholds, `ConductorBoundaryResolver`,
`EndpointSemanticReconstructor`, `ElectricalNetResolver`, `ChassisGround`
classification logic in `ShapeDetector`, global morphology kernel lengths
or `minimum_segment_length`, Wire identity or deduplication logic, source
scoping, OCR/text recognition, or connector taxonomy.

## 19. Final Commit

Committed directly on `main` (no branch created), message:
"AP-DIAG-FIX-007: recover ground approach conductor evidence".

## 20. Push Status

Pushed to `origin/main`. `git status --short` clean after push.

## Conclusion

Both previously-unresolved `Ground` endpoints
(`component-candidate-shape-region-5aa211846bb7891d` and
`component-candidate-shape-region-6b6ccc2d59afe578`) now resolve at
`High` confidence, using real, raster-derived conductor geometry that
preserves each symbol's actual approach-wire shape (diagonal-into-vertical
and L-bend-into-vertical respectively) rather than a fabricated straight
line. This was achieved without changing any global morphology parameter,
without weakening `MorphologyWireDetector`'s extraction anywhere else in
the diagram (verified: zero changes to any of 81 component candidates, 34
symbol primitives, or any of the 4 previously-successful ground
endpoints), and without manufacturing a `Ground` endpoint directly — the
fix supplies missing physical conductor evidence to the existing
topology/boundary/endpoint pipeline, which resolves it through its own
unmodified machinery.

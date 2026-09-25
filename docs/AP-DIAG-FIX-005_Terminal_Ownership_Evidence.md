# AP-DIAG-FIX-005 — Terminal Attribution Ownership Evidence

## 1. Original Failure

AP-DIAG-AUDIT-004 found that terminal (`ComponentTerminal`) attribution in
this pipeline could be established from geometric proximity to a
component's bounding box alone, with no requirement that the candidate
component actually own any evidence connecting it to real symbol/terminal
geometry. Three wire endpoints across two Wires
(`wire-ded6cca62fbe3cd9`, both endpoints; `wire-e49784ea9fbccace`, one
endpoint) were confirmed attributed to the same 7x7px component
(`component-candidate-shape-region-c2335cc575d107b1`, `circular_symbol`,
`geometrically_classified`, **zero owned `SymbolPrimitive`s**), which
direct visual inspection of `samples/trx300ODG.png` proved to be the
pointer/dot glyph of the "SUB FUSE 15A" annotation leader line — not an
engineering symbol.

## 2. Exact Source Evidence (reproduced before implementation)

Per `artifacts/audit/terminal_attribution_inventory.json` and
`terminal_attribution_findings.json`:

- `wire-ded6cca62fbe3cd9`: endpoints `endpoint-candidate-7b1232ec85583772`
  (880.5,416) and `endpoint-candidate-8b9c74742885946f` (880.5,442), both
  attributed `component_terminal` to
  `component-candidate-shape-region-c2335cc575d107b1` (874,430,7,7).
- `wire-e49784ea9fbccace`: `endpoint-candidate-e9392250710fc19a`
  (885.5,435), a **third, independent topology node**
  (`topology-node-cf10dd596d2bfc16`), also attributed to the same
  spurious component.
- `wire-fd53d84a92e53bb4` (the ambiguous case):
  `endpoint-candidate-87250d4701edd231` (629.5,556) attributed to
  `component-candidate-shape-region-d1aefcaca5503ad9` (629,543,8,10),
  also zero owned `SymbolPrimitive`s.
- All three attributed components confirmed to own **zero**
  `SymbolPrimitive`s (`symbol_primitives[].component_id` never matches
  their IDs) and **zero** associated `RejectedGeometryEvidence`.

## 3. Root Cause

Two independent `TerminalCandidate` producers feed the same downstream
evidence pool, and **neither validated the other's output nor required
ownership evidence of its own**:

- `TerminalLocationDetector::detect()` (`src/topology/terminal_location_detector.cpp`)
  computed attribution purely from `attachment_distance`/
  `point_to_rect_boundary` — geometric distance to a component's bounding
  box, gated only by `TerminalLocationConfig::boundary_tolerance` (8.0px).
  It never checked whether the candidate component owned any
  `SymbolPrimitive`.
- `TerminalRecognizer::recognize()` (`src/topology/terminal_recognizer.cpp`)
  already has a genuine ownership-evidence-based path (`SymbolPrimitiveKind::TerminalLead`
  matching), but its *fallback* path for "symbols with no usable internal
  lead" checked only geometric distance plus conductor-direction alignment
  (`alignment_to_component`) — it did **not** require the component to own
  *any* `SymbolPrimitive` at all before applying that fallback.

Tracing which producer generated each false candidate confirmed **both**
producers were independently implicated:
`endpoint-candidate-7b1232ec85583772`'s only evidence came from
`TerminalRecognizer`'s alignment fallback (the annotation dot happens to
sit almost exactly on the conductor's path, so alignment cosine ≈ 1.0);
`endpoint-candidate-8b9c74742885946f`, `endpoint-candidate-e9392250710fc19a`,
and `endpoint-candidate-87250d4701edd231` all came from
`TerminalLocationDetector`'s pure bounding-box distance check. A fix
targeting only one producer would have left the other endpoint(s)
misattributed (confirmed by inspecting each endpoint's
`ConductorBoundaryEvidence.source_object_id` prefix — `terminal-recognition-*`
vs. `terminal-candidate-*`).

## 4. Why Proximity Alone Was Insufficient

Both mechanisms conflated "geometry is nearby" with "geometry is owned by
a real terminal." A component that was itself a misclassification of
non-symbol geometry (an annotation leader-line glyph) has nothing
distinguishing it, under a pure-distance rule, from a real symbol whose
terminal/lead geometry simply wasn't independently detected as a
`SymbolPrimitive`. Proximity is necessary but not sufficient; the
missing ingredient was **positive ownership evidence**.

## 5. Ownership Requirement Implemented

`TerminalLocationDetector::detect()` and `TerminalRecognizer::recognize()`'s
boundary-alignment fallback now both require the candidate component to
have **at least one piece of ownership evidence already present in the
architecture** before a `ComponentTerminal`/`GroundConnection`/
`ConnectorBoundary` attribution is produced:

- an owned `SymbolPrimitive` (of **any** kind — not only `TerminalLead`;
  a component may legitimately own primitives never classified
  `TerminalLead`, as the real
  `component-candidate-shape-region-bfae05a427189376` case demonstrates:
  6 legitimate `ComponentTerminal` endpoints are backed entirely by
  `Unknown`-kind primitives), **or**
- (in `TerminalLocationDetector` only, since it already receives
  `rejected_geometry` as a parameter) an explicit `ComponentAssociated`/
  `ConnectorAssociated` `RejectedGeometryEvidence` entry referencing the
  component — geometry independently attributed to that component's
  boundary by `GeometryOwnershipClassifier`, for symbols whose terminal
  geometry was not captured as a `SymbolPrimitive` at all.

This reuses evidence structures already present in the codebase exactly
as required — no new scoring system, no new confidence heuristic, and no
change to `boundary_tolerance`, `high_confidence_distance`,
`medium_confidence_distance`, `terminal_lead_max_distance`,
`aligned_boundary_max_distance`, or `minimum_alignment_cosine`. Distance
and alignment remain exactly as they were; they are now a **necessary
but no longer sufficient** condition.

`TerminalRecognizer`'s first phase (matching an owned `SymbolPrimitiveKind::TerminalLead`)
was already correctly evidence-based and is unchanged.

## 6. Why Reversed/Shared/Legitimate Cases Are Unaffected

- A component with **any** owned `SymbolPrimitive` (regardless of kind)
  is unaffected — this is deliberately not narrowed to `TerminalLead`
  specifically, since 5 of the 11 remaining legitimate
  `component_terminal` endpoints in the real model are backed by
  `unknown`/`rectangle`-kind primitives, not `TerminalLead`.
- A genuine `ChassisGround` is unaffected: all 6 in the current model own
  at least one `TerminalLead` `SymbolPrimitive` (verified directly, not
  assumed — see §12).
- The `ConnectorAssociated` `RejectedGeometryEvidence` path
  (AP-GEOMETRY-005) is unaffected and remains the ownership evidence for
  that case.

## 7. Implementation Location

- `include/eke_dx_wire/topology/terminal_location_detector.hpp`: added a
  `symbol_primitives` parameter (defaulted to `{}`) to `detect()`.
- `src/topology/terminal_location_detector.cpp`: added
  `has_ownership_evidence()` (checks owned `SymbolPrimitive` OR associated
  `RejectedGeometryEvidence`); the per-component loop in `detect()` now
  skips any component lacking this evidence before computing distance.
- `src/topology/terminal_recognizer.cpp`: the boundary-alignment fallback
  now `continue`s when `owned_primitives.empty()`, immediately after the
  existing `has_terminal_lead` check.
- `src/pipeline/extraction_pipeline.cpp`: the `TerminalLocationDetector::detect()`
  call site now passes `model.symbol_primitives` (already populated
  earlier in the pipeline by `SymbolGeometryExtractor`, before this stage
  runs).

No change to `ShapeDetector`, `PhysicalWireIdentityReconstructor`, the
Wire identity key, `ElectricalNetResolver`, `SourceScoper`, ground
classification/`ChassisGround` logic, AP-WIRE-029/030/031, or any core
model type. The false `circular_symbol` candidate geometry itself was not
touched or removed — only the unsupported downstream attribution.

### A pre-existing, unrelated observation (not fixed)

While tracing `TerminalLocationDetector`'s call site,
`src/pipeline/extraction_pipeline.cpp` was found to call
`model.rejected_geometry = std::move(rejected_geometry);` **before**
passing the same (now moved-from) local `rejected_geometry` variable into
`terminal_detector.detect(...)`. This predates this AP and is outside its
scope (it does not touch `ShapeDetector`, `ElectricalNetResolver`,
ground classification, or any forbidden area, and fixing it would change
production behavior for the `RejectedGeometryEvidence`-based distance
narrowing in ways this AP's test plan does not cover). Its practical
effect on this fix: in the actual pipeline, `TerminalLocationDetector`
currently receives an empty `rejected_geometry` vector regardless of
what `model.rejected_geometry` actually contains, so the
`RejectedGeometryEvidence`-based half of the new ownership check is
presently only exercised by direct unit tests of `TerminalLocationDetector`
(where a real vector is passed), not by the full pipeline. This is
reported here for transparency, not fixed, and is the reason
`endpoint-candidate-9da9c73ab52013a3` (§10) loses its attribution below —
see that section.

## 8. Tests

Test-first reproduction, per Section 8: `wire-ded6cca62fbe3cd9` (both
endpoints) and `wire-e49784ea9fbccace` were reproduced as failing before
any production change, using the exact real coordinates and IDs from the
audit. Rather than a runtime assertion failure, the reproduction attempt
against pre-fix code failed to **compile** — `TerminalLocationDetector::detect()`'s
pre-fix signature had no `symbol_primitives` parameter at all, so the new
test could not even call it with the evidence the fix requires. This is a
stronger demonstration of the defect than a runtime failure: the
interface itself lacked the capacity to distinguish evidenced from
unevidenced attribution. Confirmed via `git stash` of only the four
production files, rebuilding, observing the compile failure, then
restoring the fix (`git stash pop`) and confirming a clean build and all
assertions passing.

`tests/test_terminal_ownership_evidence.cpp` (new): reproduces
`wire-ded6cca62fbe3cd9`'s endpoint A via `TerminalRecognizer` (alignment
fallback path) and endpoint B via `TerminalLocationDetector` (bounding-box
path) using the real component bounds and endpoint coordinates;
reproduces `wire-e49784ea9fbccace`'s endpoint via `TerminalLocationDetector`;
positive controls confirm a component owning a non-`TerminalLead`
primitive still resolves through both producers, and a genuine
`ChassisGround` (owning a `TerminalLead`) is unaffected; the ambiguous
case (`endpoint-candidate-87250d4701edd231`) is confirmed to resolve to
**no** candidate (never guessed).

`tests/test_terminal_location_detector.cpp` (updated): the two existing
legitimate cases (`component-1`, `ground-1`) now supply an owned
`SymbolPrimitive` so they continue to test what they were designed to
test (distance/interior-attachment math) rather than incidentally
depending on the now-closed proximity-only path; a new explicit
negative-control case proves a zero-evidence component near an endpoint
produces no candidate.

`tests/test_terminal_recognizer.cpp` (updated): the boundary-alignment
fallback tests now supply an owned (non-`TerminalLead`) primitive to the
relevant components so they continue to isolate the alignment/kind-mapping
logic under test; a new case reproduces the exact real defect (perfect
alignment, zero owned primitives) and confirms it now yields no candidate.

Full suite: **57/57 passing** (56 pre-existing + 1 new test executable),
clean from-scratch rebuild, assertion-enabled configuration intact
(`dx-wire-test-assertions-enabled` passing), exactly the same 8
pre-existing compiler warnings, 0 new.

## 9. Positive Controls

- A component owning any `SymbolPrimitive` (not `TerminalLead`) still
  resolves through `TerminalLocationDetector` and through
  `TerminalRecognizer`'s alignment fallback (`tests/test_terminal_ownership_evidence.cpp`,
  `tests/test_terminal_recognizer.cpp`).
- A genuine `ChassisGround` owning a `TerminalLead` is unaffected
  (`tests/test_terminal_ownership_evidence.cpp`).
- In the real TRX300 model: 11 of the 16 pre-fix `component_terminal`
  endpoints remain `component_terminal` after the fix, all backed by an
  owned `SymbolPrimitive` — confirmed by direct comparison, not assumed
  (§10).

## 10. Negative Controls / Before-After Inventory

Direct comparison of a fresh unscoped extraction against an isolated
pre-fix rebuild of `9b9f8f4` (via `git worktree`, removed after use):

| Metric | Before | After |
|---|---:|---:|
| `component_terminal` endpoints | 16 | 11 |
| `geometric` endpoints | 183 | 188 |
| `ground` endpoints | 4 | 4 (unchanged) |
| Physical Wire records | 35 | 35 (unchanged, byte-identical) |
| `component_candidates` / `component_symbol_recognitions` / `symbol_primitives` | — | byte-identical |
| `nodes` / `edges` | 686 / 876 | byte-identical |
| `electrical_nets` | 10 | byte-identical (all 4 ground-role, 6 unknown-role unchanged) |
| `connector_candidates` / `connector_terminals` | 0 / 0 | 0 / 0 (unchanged) |
| `rejected_geometry` | 10 | byte-identical |
| Validation errors | 0 | 0 |
| Validation warnings | 32 | 34 (+2, fully explained — see §11) |

Scoped extraction (`fixtures/trx300/scope_production.json`) shows the
identical delta (`component_terminal` 16→11, `geometric` 165→170, all
else unchanged) — the fix is not scope-dependent.

Exactly the 5 endpoints AP-DIAG-AUDIT-004 flagged as owning zero
ownership evidence lost their `component_terminal` attribution, and only
those 5:

- `endpoint-candidate-7b1232ec85583772`, `endpoint-candidate-8b9c74742885946f`
  (`wire-ded6cca62fbe3cd9`, both) → `geometric`, `component_id=""`.
- `endpoint-candidate-e9392250710fc19a` (`wire-e49784ea9fbccace`) →
  `geometric`, `component_id=""`.
- `endpoint-candidate-87250d4701edd231` (the ambiguous case,
  `wire-fd53d84a92e53bb4`) → `geometric`, `component_id=""` — **not
  guessed either way**, per Section 17's explicit instruction.
- `endpoint-candidate-9da9c73ab52013a3` (the LOW dangling-endpoint
  finding, not attached to any Wire) → also `geometric`,
  `component_id=""`. This was not specially targeted — AP-DIAG-AUDIT-004
  itself classified this endpoint's attribution as PLAUSIBLE/AMBIGUOUS,
  never confirmed correct, and it satisfied the identical zero-evidence
  condition as the confirmed defects. Per §7, the pre-existing
  `rejected_geometry` move-ordering issue means the
  `RejectedGeometryEvidence`-based half of the ownership check (which, if
  reachable, would have preserved this specific endpoint via its two
  `component_associated` conductor-segment evidence entries) is not
  currently exercised in the full pipeline. Applying the same rule
  uniformly, without special-casing, and reporting the side effect
  honestly here, is preferred over carving out an exception for one
  endpoint.

## 11. Downstream Impact

- **Wire records**: all 35 physical Wires, including
  `wire-ded6cca62fbe3cd9`, `wire-e49784ea9fbccace`, and
  `wire-fd53d84a92e53bb4`, remain byte-identical (`Wire::id`,
  `start_endpoint`, `end_endpoint`, `topology_edges`,
  `conductor_segments`, `identity_status`, `identity_evidence_ids` all
  unchanged) — physical conductor evidence and semantic terminal evidence
  are confirmed separate layers, exactly as AP-DIAG-AUDIT-004 established.
- **`wire_semantics`**: `start_component_id`/`end_component_id` for the 3
  affected wires now correctly read `""`/`unresolved` instead of the
  wrong component, while every other field (wire color, function label,
  connector fields, `electrical_net_id`/`status`/`confidence`) is
  unchanged. `wire-fd53d84a92e53bb4`'s `electrical_net_id`
  (`electrical-net-5de51cd3d5bac578`, `high` confidence, ground-role) is
  **unchanged** — the net's role and membership were never dependent on
  this endpoint's component attribution.
- **Validation warnings** (+2, `WIRE-GEOMETRIC-ENDPOINTS`): this code
  fires only when *both* of a wire's endpoints are `GeometricConductorEnd`.
  `wire-ded6cca62fbe3cd9` now has both endpoints geometric (newly
  triggers); `wire-e49784ea9fbccace` already had one geometric endpoint
  and now has both (newly triggers); `wire-fd53d84a92e53bb4` has one
  geometric endpoint but its other endpoint remains a confirmed
  `ChassisGround` (not geometric), so it does **not** trigger. This
  accounts for the entire +2 delta and is the intended, correct
  consequence of no longer manufacturing false semantic certainty.
- **Topology**: `nodes`/`edges` byte-identical — splice, junction,
  crossing, and continuation semantics are untouched (they were never
  computed from terminal attribution).
- **Electrical nets**: byte-identical, including net role and membership.

## 12. Deterministic Results

Re-running the unscoped extraction twice with the fix applied produced
byte-identical `endpoint_candidates`, `conductor_boundary_resolutions`,
`conductor_boundary_evidence`, `endpoint_semantic_reconstructions`, and
`wires`. The only file that differed between runs was
`artifacts/extraction_review/review_manifest.json`, and only in its
documented volatile `generated_at` timestamp field.

## 13. Remaining Known Findings (unchanged, out of scope for this AP)

- **Ground endpoint coverage gap** (AP-DIAG-AUDIT-003/004, MEDIUM): the 2
  ChassisGround components without a resolved Ground endpoint are
  unaffected — both are confirmed distance/geometry gaps unrelated to
  ownership evidence (their nearest endpoints are 16.3px/37.0px away,
  beyond even the unchanged `boundary_tolerance`). Not touched.
- **Pre-existing `rejected_geometry` move-ordering issue** in
  `extraction_pipeline.cpp` (§7): identified, documented, not fixed — it
  predates this AP and fixing it would be a separate, independently-scoped
  change.

Every genuine `ChassisGround` (6) and ground-role net (4) is unchanged;
`connector_candidates`/`connector_terminals` remain 0/0.

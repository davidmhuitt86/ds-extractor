# AP-DIAG-AUDIT-006 — Ground Endpoint Coverage Forensics

This is an audit document. **No production classifier, extractor,
topology, terminal-resolver, Wire, ground, scope, or model code was
modified to produce it.** All forensic work was read-only inspection of
the current committed model output, direct pixel inspection of
`samples/trx300ODG.png`, and a standalone, uncommitted Python/OpenCV
script that replicates (without touching) `MorphologyWireDetector`'s
exact algorithm to test causality. `git status --short` was empty
throughout the investigation, before this document and its JSON artifact
were added.

## 1. Executive Summary

The two genuine `ChassisGround` components that never receive a resolved
`Ground` endpoint
(`component-candidate-shape-region-5aa211846bb7891d` and
`component-candidate-shape-region-6b6ccc2d59afe578`) are **not** failing
because of the 8px `boundary_tolerance` in `TerminalLocationDetector`, nor
because of `TerminalRecognizer`'s alignment logic, nor because of
`ConductorBoundaryResolver` or `EndpointSemanticReconstructor` — all four
of those stages behave completely correctly given their inputs. The root
cause is one stage earlier and one class earlier than any prior audit
examined: **`MorphologyWireDetector`** (`src/image/morphology_detector.cpp`),
which extracts conductor geometry using only two fixed-orientation,
minimum-length morphological openings (`MORPH_OPEN` with a 25×1 horizontal
kernel and a 1×25 vertical kernel; `include/eke_dx_wire/image/morphology_detector.hpp:12-13`).
This detector **never produces any conductor geometry at all** for the
short and/or non-axis-aligned final segment connecting each of these two
ground symbols to its parent circuit element — even though that ink is
directly confirmed present, continuous, and unambiguous in the source
raster. A standalone control experiment replicating the exact algorithm
confirms: the connecting segment near
`component-candidate-shape-region-6b6ccc2d59afe578` is **completely
erased** by the vertical opening (0 surviving pixels in a region with 134
raw ink pixels), and the segment near
`component-candidate-shape-region-5aa211846bb7891d` includes a diagonal
jog that no horizontal-or-vertical-only structuring element can preserve.
Because no conductor geometry is produced there, no topology node, no
`EndpointCandidate`, and consequently no `TerminalCandidate` is ever
created close enough to either ground symbol — `TerminalLocationDetector`,
`TerminalRecognizer`, `ConductorBoundaryResolver`, and
`EndpointSemanticReconstructor` all correctly report "no evidence" because
they were never given any. This has **zero** impact on Wire count,
topology, or `ElectricalNet` results: each affected Wire is fully
reconstructed and correctly represented, just with one endpoint stranded
at the last point the (real, physically continuous) conductor happened to
survive morphological extraction, rather than at the ground symbol
itself.

## 2. Starting Repository State

- `git rev-parse HEAD` (before audit work): `b31bf035394dda6825925493f81c586148524846`
  — matches AP-DIAG-FIX-006's reported Final SHA exactly.
- `git branch --show-current`: `main`
- `git status --short` (before audit work): empty (clean)
- `git log -1 --oneline`: `b31bf03 AP-DIAG-FIX-006: correct rejected geometry evidence handoff`
- No branch was created, switched, or reset. No `git worktree` was used
  in this audit (no independent historical rebuild was required — the
  investigation traces entirely within the current HEAD's own committed
  behavior).

## 3. Baseline Verification

Full clean Release rebuild + `ctest`, then a fresh canonical unscoped
extraction, before any audit work:

| Metric | Value |
|---|---:|
| Tests | 58/58 passing |
| Assertions | active |
| Compiler warnings | 8 (pre-existing, unchanged) |
| Wire records | 35 |
| Electrical nets | 10 |
| ChassisGround entries | 6 |
| Ground-role nets | 4 |
| Resolved Ground endpoints | 4 |
| Missing Ground endpoints | 2 |
| Validation errors | 0 |
| Validation warnings | 34 |

All match the stated AP-DIAG-FIX-006 baseline exactly.

## 4. The Six Genuine ChassisGround Components

Traced directly through `artifacts/topology/topology.json`'s
`component_candidates` / `component_symbol_recognitions` /
`symbol_primitives` (not by visual inspection):

| Component ID | Bounds (x,y,w,h) | Owned primitives (kind) | Ground endpoint? |
|---|---|---|---|
| `...0edf5ce35037fec3` | 576,586,20,13 | terminal_lead, line, terminal_lead | Yes |
| `...1acbaeb7ac6ac87a` | 621,586,19,13 | terminal_lead, terminal_lead, line | Yes |
| `...a434a925670e9b65` | 924,547,22,13 | terminal_lead, line, terminal_lead | Yes |
| `...791276441805b2e0` | 646,547,22,13 | rectangle, line, terminal_lead | Yes |
| `...5aa211846bb7891d` | 709,547,21,13 | terminal_lead, rectangle, line | **No** |
| `...6b6ccc2d59afe578` | 749,547,20,13 | terminal_lead, terminal_lead, line | **No** |

All 6 are `symbol_kind=chassis_ground`, `status=geometrically_classified`,
`confidence=high` — identical recognition quality. Critically, the
**primitive-kind composition does not separate the groups**:
`...791276441805b2e0` (successful) and `...5aa211846bb7891d` (failed)
share the identical `{terminal_lead, rectangle, line}` pattern; the other
successful trio and `...6b6ccc2d59afe578` (failed) share the identical
`{terminal_lead, terminal_lead, line}` pattern. This rules out primitive
composition/kind as the distinguishing factor before any deeper analysis.

## 5. The Four Successful Ground Endpoints

| Ground endpoint | Component | Coordinates | Nearest node distance |
|---|---|---|---:|
| `endpoint-candidate-bfae64deea64562f` | `...0edf5ce35037fec3` | (585.5,585) | ~3.0px |
| `endpoint-candidate-9d7e7957c56e3777` | `...1acbaeb7ac6ac87a` | (629.5,585) | ~3.2px |
| `endpoint-candidate-682d9fd165bc9fcf` | `...a434a925670e9b65` | (935,542) | ~7.0px |
| `endpoint-candidate-94c3703334d041e1` | `...791276441805b2e0` | (656,546) | ~3.2px |

All four are the terminus of a **long, straight, single-orientation
conductor run** that reaches (or comes within a few pixels of) the ground
symbol's topmost bar directly — confirmed by direct pixel inspection of
`...791276441805b2e0`'s region (§16): an unbroken vertical line from
above y=513 straight down to y=548 with no bend, no gap, no orientation
change.

## 6. The Two Missing Ground Endpoints

| Component | Nearest real endpoint (any kind) | Distance | Its Wire |
|---|---|---:|---|
| `...5aa211846bb7891d` | `endpoint-candidate-159125bfa711f45b` (707.5,536), `geometric` | 17.7px (to top-lead center); 11.1px (to component bbox) | `wire-45da48584643bb03` |
| `...6b6ccc2d59afe578` | `endpoint-candidate-bae93adb5ee5ca59` (736,518), `geometric` | 38.6px (to top-lead center); 31.8px (to component bbox) | `wire-b87293c6bc606b9a` |

Both distances exceed `TerminalLocationDetector`'s `boundary_tolerance`
(8.0px) and `TerminalRecognizer`'s `aligned_boundary_max_distance` (16.0px)
under every reference-point convention tried. **But this is not the root
cause** — see §9-13: no conductor geometry, and therefore no candidate
endpoint, exists any closer, regardless of tolerance value.

## 7. Comparative Evidence Matrix

| Property | 4 successful | `...5aa211846bb7891d` | `...6b6ccc2d59afe578` |
|---|---|---|---|
| Bounding box, confidence, recognition status | high, `geometrically_classified` | identical | identical |
| Owned primitive kinds | 2 patterns, both also present in a failed case | matches a successful pattern | matches a successful pattern |
| `RejectedGeometryEvidence` associated | none for any of the 6 | none | none |
| Nearest topology node to the topmost bar | 3.0-7.0px, already the resolved ground endpoint | 17.7px, a `geometric` endpoint of an unrelated Wire | 38.6px, a `geometric` endpoint of an unrelated Wire |
| Connecting segment shape (per pixel inspection) | single straight run, correct orientation, length far exceeding the morphological kernel | diagonal jog + short (~10px) vertical tail | short (~17-20px) vertical run + right-angle bend |
| Conductor geometry produced for the final connecting segment | yes | **no** | **no** |
| `TerminalCandidate` evidence produced | yes | **none** | **none** |
| Scope-dependent? | n/a | no (identical in scoped/unscoped) | no (identical in scoped/unscoped) |

**The earliest property that separates the two groups is the presence or
absence of conductor geometry for the final connecting segment** — every
downstream property (endpoint existence, terminal-candidate evidence,
boundary resolution, semantic reconstruction) is a deterministic
consequence of that one upstream fact, not an independent divergence.

## 8. Ground Symbol Evidence Pipeline

Tracing the full path for the two failed symbols:

```
raster (real, continuous ink - confirmed, §16)
   |
   v
MorphologyWireDetector::detect()                    [src/image/morphology_detector.cpp]
   -> horizontal/vertical MORPH_OPEN (25px kernels) + minimum_segment_length=12 filter
   -> DIVERGES HERE: no ConductorSegment produced for the final connecting run
   |
   v
ShapeDetector::detect() ground-symbol classification  [src/image/shape_detector.cpp]
   -> succeeds identically for all 6 (the bars themselves are captured fine;
      classification does not depend on the connecting wire)
   |
   v
GeometryOwnershipClassifier / RejectedGeometryClassifier
   -> nothing to classify or reject - no candidate ever existed for this ink
   |
   v
TopologyReconstructor -> no node near the ground bar; the known conductor
   run simply ends wherever morphological extraction happened to stop
   |
   v
EndpointReconstructor -> no EndpointCandidate near the ground bar
   |
   v
TerminalLocationDetector::detect() / TerminalRecognizer::recognize()
   -> correctly find nothing within tolerance (nothing exists there to find)
   |
   v
ConductorBoundaryResolver / EndpointSemanticReconstructor
   -> correctly report "unresolved" (zero evidence was ever supplied)
   |
   v
No Ground endpoint
```

**The two affected symbols diverge from the four successful ones at the
first stage: `MorphologyWireDetector`'s conductor-geometry extraction.**
Every subsequent stage is executing correctly on an already-incomplete
input.

## 9. TerminalLocationDetector Analysis

Inspected `include/eke_dx_wire/topology/terminal_location_detector.hpp`
and `src/topology/terminal_location_detector.cpp` directly (unmodified
by this audit). For both affected endpoints
(`endpoint-candidate-159125bfa711f45b`,
`endpoint-candidate-bae93adb5ee5ca59`):

- `boundary_tolerance` (8.0px): both endpoints' distance to the
  respective component's bounding box (11.1px, 31.8px) exceeds it.
- `attachment_distance()`'s `RejectedGeometryEvidence`-narrowing branch
  (AP-DIAG-FIX-005): inapplicable — neither ground component has any
  associated `RejectedGeometryEvidence` (§14).
- `has_ownership_evidence()` (AP-DIAG-FIX-005): both `ChassisGround`
  components legitimately pass this gate (each owns >=1 owned
  `SymbolPrimitive`), so ownership evidence is **not** the blocker here.
- The candidate is filtered by `require_component_boundary_proximity`
  (distance check) alone, before any ownership question is reached.

**Direct answer to "is the 8px boundary tolerance actually responsible":**
No. Confirmed by construction: **there is no `EndpointCandidate` anywhere
near either ground symbol's boundary at all** — the nearest real endpoint
in each case belongs to a different, unrelated location on the same Wire
(17.7px / 38.6px away, itself the far end of a conductor run that stops
short). Raising or removing `boundary_tolerance` would not create the
missing endpoint; it would only widen how far `TerminalLocationDetector`
searches from an endpoint that fundamentally does not exist at the right
location. Classification against the task's taxonomy:
**(G) missing conductor evidence**, not (J) actual tolerance limitation.

## 10. TerminalRecognizer Analysis

Inspected `src/topology/terminal_recognizer.cpp`. For
`endpoint-candidate-159125bfa711f45b` (component
`...5aa211846bb7891d`, bbox distance 11.1px — within
`aligned_boundary_max_distance` of 16.0px):

- The component owns >=1 `SymbolPrimitive` (post-AP-DIAG-FIX-005 gate) —
  passes.
- `alignment_to_component()` computes the cosine between (a) the
  direction from the endpoint toward the component's nearest boundary
  point (pointing downward/toward the ground bar) and (b) the direction
  of the endpoint's only incident edge, which goes to
  `topology-node-bfa21d362d5cbb87` at (708, 489.5) — **upward**, away
  from the ground bar. This cosine is strongly negative, far below
  `minimum_alignment_cosine` (0.85) — **the alignment check correctly
  fails**, because the only conductor direction this endpoint actually
  has on record points away from the ground symbol. This is not an
  independent defect in the alignment logic; it is the expected,
  logically necessary consequence of the missing final segment (§7-9):
  the endpoint's "forward" continuation (toward the ground bar) was
  never captured, so only its "backward" direction exists to test.
- For `endpoint-candidate-bae93adb5ee5ca59` (component
  `...6b6ccc2d59afe578`), bbox distance is 31.8px, already beyond
  `aligned_boundary_max_distance` (16.0px) — `TerminalRecognizer` never
  reaches the alignment check for this pair at all.

Neither `TerminalLocationDetector` nor `TerminalRecognizer` produced any
`TerminalCandidate` for either endpoint — confirmed directly:
`conductor_boundary_evidence` for both endpoints contains **only** a
self-referential `endpoint_semantic_reconstruction`-kind entry with
`suggested_boundary=unresolved`; no `terminal_candidate`-kind entry
exists for either. Both detectors are working exactly as designed.

## 11. ConductorBoundaryResolver Analysis

No Ground boundary candidate ever reaches `ConductorBoundaryResolver`
for either affected endpoint — confirmed directly from
`conductor_boundary_resolutions`:

```
endpoint-candidate-159125bfa711f45b:
  boundary_kind=geometric, boundary_status=unresolved,
  ground_status=unresolved, evidence_ids=[conductor-boundary-evidence-62127bd4be34edb7]
  (that one evidence entry is itself unresolved/empty)

endpoint-candidate-bae93adb5ee5ca59:
  boundary_kind=geometric, boundary_status=unresolved,
  ground_status=unresolved, evidence_ids=[conductor-boundary-evidence-870579225a2cd090]
  (likewise unresolved/empty)
```

Compared directly against a successful case
(`endpoint-candidate-bfae64deea64562f`,
`...0edf5ce35037fec3`): `boundary_kind=ground`, `boundary_status=resolved`,
`ground_status=resolved`, `boundary_confidence=high`. The difference is
not a resolver decision — the resolver is given nothing to resolve for
the two failing endpoints, and nothing to conflict with either. No
candidate exists; none is rejected; the resolver's own logic is not
exercised for a Ground outcome at all for these two.

## 12. EndpointSemanticReconstructor Analysis

The missing Ground endpoint is lost **before** semantic reconstruction,
not during or after it. `EndpointSemanticReconstructor` (via
`endpoint_semantic_reconstructions`) shows both endpoints resolved to
`endpoint_kind=unresolved`, `status=unresolved`,
`evidence_component_ids=[]` — an entirely empty evidence set. There is no
conflicting evidence to arbitrate and no candidate to demote; the
reconstructor faithfully reports "no evidence" because the previous
stages supplied none. This confirms the loss occurs strictly upstream of
this stage.

## 13. Geometry Ownership / Exclusion Analysis

AP-DIAG-FIX-003 previously (and, per its own committed comment in
`src/image/shape_detector.cpp:756-778`) identified and corrected an
over-broad ground-symbol exclusion mask: the exclusion region used to
extend across the full 14px stem-search corridor
(`ground_stem_search_height`), erasing real approach-wire ink. The fix
narrowed the **exclusion mask** to just the bars' own bounding box plus a
fixed 2px margin (`kGroundExclusionMargin = 2`,
`src/image/shape_detector.cpp:775`) while keeping the wider 14px window
only for the internal *presence check* (confirming a stem exists at all),
not for masking.

**This audit confirms that fix is still working correctly and is not
responsible for the current gap.** For `...5aa211846bb7891d` (top edge
y=547), the exclusion mask extends only to y=545 (547 − 2px margin) — it
does not reach the connecting ink at y=530-545 at all. The same holds for
`...6b6ccc2d59afe578`. This rules out hypothesis **(F) exclusion-mask
truncation** with direct source evidence — it is a different, previously
undiscovered mechanism (§9, §17), not a recurrence of the AP-DIAG-FIX-003
issue.

`GeometryOwnershipClassifier` and scope exclusion were also checked: no
`RejectedGeometryEvidence` exists anywhere near either symbol (§14), and
the identical failure occurs in both scoped and unscoped extraction
(§15) — ruling out scope-based clipping as a contributing factor.

## 14. Rejected Geometry Analysis

AP-DIAG-FIX-006 corrected the `rejected_geometry` evidence handoff into
`TerminalLocationDetector`. Confirmed directly: neither
`component-candidate-shape-region-5aa211846bb7891d` nor
`component-candidate-shape-region-6b6ccc2d59afe578` has **any**
associated `RejectedGeometryEvidence` entry (searched all 10 entries in
`model.rejected_geometry` by coordinate range and by
`associated_object_id` — zero matches for either component). This is
consistent with, and further confirms, the root cause: `RejectedGeometryEvidence`
is only ever created for ink that `GeometryOwnershipClassifier` or
`ConductorEvidenceEvaluator` actually examined and rejected. The
connecting segment for these two ground symbols was never even
*produced* as a `ConductorSegment` candidate by `MorphologyWireDetector`
in the first place, so there was nothing for those classifiers to reject
— rejected-geometry evidence is not relevant to this specific gap, and
AP-DIAG-FIX-006's correction (now working correctly) has no bearing on
it, positive or negative.

## 15. Scoped vs Unscoped Analysis

Both extractions were re-run fresh for this audit. Identical outcome in
both:

| | Unscoped | Scoped |
|---|---|---|
| Genuine ChassisGround | 6 | 6 |
| Missing Ground endpoints | `...5aa211846bb7891d`, `...6b6ccc2d59afe578` | identical |
| Nearest node distance, `...5aa211846bb7891d` | 17.7px | 17.7px (identical node) |
| Nearest node distance, `...6b6ccc2d59afe578` | 38.6px | 38.6px (identical node) |

**The condition is not scope-dependent.** The scope in
`fixtures/trx300/scope_production.json` does not clip or alter geometry
in this region of the diagram; the same two components fail for the same
reason under both conditions.

## 16. Source Raster Verification

Direct pixel-level inspection of `samples/trx300ODG.png` (grayscale,
threshold <128 = ink), via a temporary, uncommitted read-only script (no
production file touched):

**`...5aa211846bb7891d`** (region x:695-735, y:525-560): a filled
junction dot at approximately (707,533), a short **diagonal** run from
the dot down-right to approximately (719,538), then a short **vertical**
run from (719,538) to (719,548) meeting the ground bar's top edge at
y=549. The full connecting path is continuous, unbroken ink — confirmed
present in the source raster.

**`...6b6ccc2d59afe578`** (region x:720-790, y:500-560): a horizontal
run at y≈530-531 (the top of an "L" bend, descending from a component
above), then a continuous **vertical** run from y≈531 to y≈548 at
x≈758, meeting the ground bar's top edge at y=549. Confirmed present and
unbroken.

**`...791276441805b2e0`** (successful, region x:630-680, y:500-552, for
comparison): a single continuous **straight vertical** run from above
y=513 down to y=548 at x≈657, with no bend and no orientation change.

**The physical approach conductor exists in the source image for all
three; it does not disappear in the successful case, and it is present
but geometrically bent/short for both failing cases.** This is an
upstream evidence-extraction limitation, not a source-diagram physical
separation (see §18).

## 17. Control Experiments

A temporary, uncommitted, read-only Python/OpenCV script replicated
`MorphologyWireDetector`'s exact algorithm
(`cv2.adaptiveThreshold(..., ADAPTIVE_THRESH_GAUSSIAN_C, THRESH_BINARY_INV, 31, 7)`
then `cv2.morphologyEx(..., MORPH_OPEN, ...)` with a 25×1 horizontal and
1×25 vertical kernel, matching `MorphologyConfig`'s exact defaults:
`horizontal_kernel_length=25`, `vertical_kernel_length=25`,
`adaptive_block_size=31`, `adaptive_c=7`) against three regions of the
real source image:

| Region | Raw binary ink (px) | Horizontal-opened ink (px) | Vertical-opened ink (px) |
|---|---:|---:|---:|
| `...5aa211846bb7891d` bend region | 148 | 0 | 18 |
| `...6b6ccc2d59afe578` vertical run region | 134 | 0 | **0** |
| `...791276441805b2e0` (successful, reference) | 49 | 0 | 20 |

For `...6b6ccc2d59afe578`, **vertical opening completely erases 100% of
the connecting ink** (134 → 0) — directly, mechanistically confirming why
no `ConductorSegment` is ever produced there: the run is too short
(~17-20px measured, well under the 25px vertical kernel) to survive
erosion.

For `...5aa211846bb7891d`, 18px of ink survives vertical opening (the
short straight tail below the diagonal jog), but this is consistent with
`MorphologyConfig::minimum_segment_length` (12px, applied to a
connected-component's bounding-box major axis by `MorphologyWireDetector::detect()`'s
`extract()` lambda, `src/image/morphology_detector.cpp:63-66`): the true
production run confirms no node was created in this location at all,
meaning the surviving fragment's connected-component measurement did not
clear that filter either the diagonal segment itself (0px survival,
confirmed by the horizontal-opened column too) is invisible to both
kernels regardless, since neither a horizontal nor a vertical
structuring element can preserve diagonal ink.

This experiment fully explains both failures via the **same underlying
mechanism** (a directional, minimum-length morphological line extractor
that cannot preserve short and/or non-axis-aligned ink), while
acknowledging the two symbols differ in the specific geometric detail
(one is erased for being too short and vertical; the other is erased for
being diagonal, with an additionally-too-short vertical remainder).

No temporary instrumentation was added to any committed file for this
experiment — the replication script is standalone Python, run and
discarded, never touching the repository's source tree.

## 18. Root Cause(s)

**Single shared upstream mechanism, two distinct geometric
manifestations.** Both `component-candidate-shape-region-5aa211846bb7891d`
and `component-candidate-shape-region-6b6ccc2d59afe578` fail for the same
class of reason — `MorphologyWireDetector`'s reliance on exactly two
fixed-orientation (horizontal, vertical), fixed-minimum-length (25px
kernel, 12px explicit filter) morphological openings to extract all
conductor geometry — applied to two different specific instances of that
limitation:

- `...5aa211846bb7891d`: the final connecting segment includes a
  **diagonal** jog, which no horizontal-or-vertical-only structuring
  element can preserve, regardless of length.
- `...6b6ccc2d59afe578`: the final connecting segment is a straight
  vertical run, but **shorter than the 25px vertical kernel**, so it is
  entirely erased by opening before any length filter is even reached.

This is **not** a source-diagram physical-separation issue (§16 confirms
continuous ink in both cases), **not** a `boundary_tolerance` limitation
(§9 — no candidate endpoint exists close enough regardless of tolerance),
**not** a `TerminalRecognizer` alignment defect (§10 — its rejection is a
correct, logically necessary consequence of the missing segment, not an
independent error), **not** a `ConductorBoundaryResolver` or
`EndpointSemanticReconstructor` defect (§11-12 — both correctly report
"no evidence" because none was supplied), and **not** a recurrence of the
AP-DIAG-FIX-003 exclusion-mask issue (§13 — confirmed still working
correctly and geometrically uninvolved here).

Per the task's classification: **(G) missing conductor evidence**, with
the earliest defective stage being `MorphologyWireDetector`'s conductor
geometry extraction (`src/image/morphology_detector.cpp`).

## 19. Findings and Severity

| ID | Finding | Severity |
|---|---|---|
| AUDIT-006-001 | `MorphologyWireDetector`'s fixed-orientation, fixed-minimum-length morphological line extraction fails to produce conductor geometry for the short/diagonal final segment connecting `component-candidate-shape-region-5aa211846bb7891d` to its parent circuit element, though the ink is confirmed present and continuous in the source raster | MEDIUM |
| AUDIT-006-002 | The same mechanism fails identically for `component-candidate-shape-region-6b6ccc2d59afe578`, via complete erasure of a too-short vertical run | MEDIUM |
| AUDIT-006-003 | `TerminalLocationDetector`, `TerminalRecognizer`, `ConductorBoundaryResolver`, and `EndpointSemanticReconstructor` are all confirmed to behave correctly given their (incomplete) inputs — no defect found in any of these four stages for this condition | INFORMATIONAL |
| AUDIT-006-004 | AP-DIAG-FIX-003's ground-symbol exclusion-mask narrowing is confirmed still correct and not implicated in this gap | INFORMATIONAL |
| AUDIT-006-005 | The condition is not scope-dependent and does not affect Wire, topology, or ElectricalNet correctness — only the semantic classification of one endpoint on each of two Wires | INFORMATIONAL |

For each MEDIUM finding:

**AUDIT-006-001**
- Component ID: `component-candidate-shape-region-5aa211846bb7891d`
- Source location: `src/image/morphology_detector.cpp` (`MorphologyWireDetector::detect()`, the `v_kernel`/`h_kernel` construction and `extract()` lambda), config defaults in `include/eke_dx_wire/image/morphology_detector.hpp:12-16`
- Earliest defective stage: conductor/line geometry extraction (before `ShapeDetector`'s ground classification, which succeeds independently)
- Evidence: §16 pixel inspection, §17 control experiment
- Downstream effect: one Wire endpoint (`endpoint-candidate-159125bfa711f45b` on `wire-45da48584643bb03`) remains `GeometricConductorEnd` instead of `Ground`; no Wire, topology, or net corruption
- Scope-dependent: no (§15)
- Deterministic: yes (§21)
- Affects physical topology: no (the Wire and its path are fully and correctly represented; only the semantic endpoint classification is affected)
- Affects electrical topology: no (this Wire's `electrical_net_id` is and remains unresolved in both the failing and a hypothetical corrected state, since the ground role would need this endpoint's classification to be established first — no net currently depends on it)
- Affects only semantic endpoint attribution: yes
- Recommended next AP: see §22

**AUDIT-006-002**
- Component ID: `component-candidate-shape-region-6b6ccc2d59afe578`
- Source location: same as above
- Earliest defective stage: same as above
- Evidence: §16, §17 (0px survival — the most direct possible confirmation)
- Downstream effect: one Wire endpoint (`endpoint-candidate-bae93adb5ee5ca59` on `wire-b87293c6bc606b9a`) remains `GeometricConductorEnd` instead of `Ground`; no Wire, topology, or net corruption
- Scope-dependent: no
- Deterministic: yes
- Affects physical topology: no
- Affects electrical topology: no
- Affects only semantic endpoint attribution: yes
- Recommended next AP: see §22

**There is one common root-cause mechanism** (the directional,
minimum-length morphological line extractor) behind both findings, though
each manifests through a distinct specific geometric detail (diagonal vs.
too-short). This audit does not force them into a single finding merely
because both are "missing Ground endpoints" — it establishes the shared
mechanism directly from source and pixel evidence, not from that
superficial similarity.

## 20. Downstream Impact

- **Physical Wire reconstruction**: no impact. `wire-45da48584643bb03`
  and `wire-b87293c6bc606b9a` are both fully and correctly reconstructed,
  35 Wire records total, unchanged from the established baseline.
- **`ElectricalNetResolver` / Ground-role nets**: no impact. Both
  affected wires have unresolved `electrical_net_id` in `wire_semantics`
  — they are not, and were never claimed to be, members of any net; the
  4 existing ground-role nets are unaffected.
- **Wire semantics**: only the two specific endpoints' own classification
  fields are affected (already `geometric`/unresolved, unchanged by this
  audit).
- **Endpoint counts**: unchanged (203 unscoped / 185 scoped, matching the
  established baseline exactly).
- **Topology**: unchanged (686/876 nodes/edges unscoped, 528/641 scoped).
- **Does each missing Ground endpoint correspond to an actual physical
  conductor that otherwise terminates correctly at the ground symbol, or
  is the source diagram itself physically separated?** Confirmed: **the
  former.** Both connections are real, continuous, physically-drawn
  conductors in the source diagram (§16) — this is an extraction
  coverage gap, not a source-diagram separation.

## 21. Determinism

Two unscoped extraction runs produced byte-identical `topology.json`.
Scoped extraction was independently re-verified against the unscoped
result for the same two components, confirming the identical failure
mode. Component recognitions, terminal candidates (or lack thereof),
boundary resolutions, endpoint semantic reconstructions, wires, topology,
and electrical nets are all confirmed deterministic.

## 22. Recommended Next AP

**AP-DIAG-FIX-007 — Ground Symbol Approach-Conductor Detection** (name
tentative): a narrowly-scoped investigation-then-fix AP to extend
conductor geometry extraction to recover short and/or non-axis-aligned
approach segments specifically in the immediate vicinity of an already-
detected `ChassisGround` symbol's topmost bar — for example, a bounded,
local search (not a global parameter change) that looks for a short
connecting run between a ground symbol's confirmed stem-presence check
(already performed by `ShapeDetector`'s existing 14px
`ground_stem_search_height` corridor, §13) and the nearest independently-
detected conductor end, without altering the global
`horizontal_kernel_length`/`vertical_kernel_length`/`minimum_segment_length`
parameters that the rest of the diagram's conductor extraction correctly
depends on. Any such fix must itself go through the same test-first,
before/after whole-diagram regression discipline established by
AP-DIAG-FIX-001 through 006, and must not be a blind tolerance increase
(per this audit's explicit finding that tolerance is not the mechanism).

## 23. Explicit Non-Changes

Per this AP's audit-only mandate, the following were **not** modified:

- `TerminalLocationDetector`, `TerminalRecognizer`.
- `ShapeDetector`, `MorphologyWireDetector` (or any detection threshold,
  kernel size, or filter value).
- Ground classification logic of any kind.
- `ConductorBoundaryResolver`, `EndpointSemanticReconstructor`.
- Wire reconstruction, `ElectricalNetResolver`.
- Any model type, scope logic, or exclusion-mask logic.

No fallback logic was added. No Ground endpoint was manufactured. All
temporary diagnostic work (pixel-level inspection scripts, the
morphological-operation replication experiment) was standalone,
read-only Python run outside the committed source tree and never
touched any tracked file; `git status --short` was confirmed empty
before this document and its JSON artifact were added.

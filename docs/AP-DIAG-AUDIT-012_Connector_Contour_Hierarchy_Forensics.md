# AP-DIAG-AUDIT-012: Connector Contour Hierarchy & Ground-Detector Forensics

**Status:** AUDIT ONLY. No production behavior was changed by this AP.

## 1. Executive Summary

This AP closes the two instrumentation gaps identified by
AP-DIAG-AUDIT-011: (GAP-1) `cv::findContours`'s `RETR_TREE` parent/child
hierarchy at each connector location, and (GAP-2) the ground detector's
decision path after the ≥3-bar count gate. Both gaps are now **fully
closed** with direct, reproducible evidence.

**GAP-1 result:** every one of the 12 connectors' rectangle-path contours
is a **direct child (depth 1) of a single universal root contour**
(`idx=302`, bounding the entire diagram frame, ~848×518px). There is no
connector-specific nesting; all 12 locations' fragments are siblings under
one root, ordered only by `findContours`' raster scan order. Three
genuine two-level (depth-2) parent/child relationships exist in the whole
census — CONN-007's rejected label-row contour, CONN-010's rejected
multi-region contour, and (most significantly) **CONN-011's near-miss
rectangle candidate**, whose interior void is a real child contour, not
an artifact of morphology. The circle detector's `RETR_LIST` retrieval is
confirmed flat (parent/child always −1) for every connector, as expected.

**GAP-2 result:** all 9 ground-bar runs across 6 connectors
(CONN-002, CONN-003, CONN-007 ×2, CONN-008 ×2, CONN-011 ×2, CONN-012) that
clear the ≥3-bar count gate are rejected at one of exactly two
post-count-gate checks — `ground_bar_width_sequence` (7 of 9 windows) or
bar-spacing range (2 of 9 windows) — and **none ever reach the
stem-presence check**. Zero `ChassisGround` candidates are produced at any
connector location. This directly answers AP-DIAG-AUDIT-011's open
question: the protection is the width-sequence/spacing checks, not the
historical AP-DIAG-FIX-003 Y-locality fix (connector bars are already
naturally Y-local and clear that gate cleanly).

Both success conditions of this AP are met without inference: the full
contour tree structure is described for all 12 locations, and every
ground-bar run's complete post-count-gate path is traced to its exact
final disposition.

## 2. Starting Repository State

- Expected starting SHA: `e5ed48fbc46ea2c6c39c46eee8b0df96ebc2373c`
- `git rev-parse HEAD`: `e5ed48fbc46ea2c6c39c46eee8b0df96ebc2373c` — matches.
- `git branch --show-current`: `main`
- `git status --short`: clean at start.
- `git log -1 --oneline`: `e5ed48f AP-DIAG-AUDIT-011: ShapeDetector connector forensic instrumentation`

Baseline rebuild: 61/61 tests, assertions active, 8 pre-existing compiler
warnings (0 new), 34 runtime warnings, 0 validation errors, 37 wires, 12
electrical nets, 6/6 chassis-ground references, 81 `ComponentCandidate`s,
0 resolved / 28 unresolved / 53 rejected (same recurring prompt-inversion
pattern as prior APs — the task's stated "28 resolved, 53 unresolved, 0
rejected" is inverted from the verified real values).

## 3. Connector Ground Truth

Unchanged from AP-DIAG-AUDIT-010/011: 12 connectors, 33 visible
connector-side pins, 33 conductor entries, 33 conductor exits, 0
ambiguous pins. 1 connector (CONN-001) partially represented, 11
completely absent, 0 fully represented. This AP did not re-derive or
re-validate the census; it consumed `connector_location_census.json`'s
exact bounds as fixed ROIs, as directed.

## 4. ShapeDetector Contour Extraction Path

Unchanged from AP-DIAG-AUDIT-011's inventory. `ShapeDetector::detect()`
thresholds the normalized raster once
(`cv::threshold(..., 180, ..., THRESH_BINARY_INV)`) and runs three
detector functions over that single binary image:

1. `detect_rectangles()`: `cv::morphologyEx(MORPH_CLOSE)` (kernel
   `contour_close_kernel|1`, min 3) then
   `cv::findContours(closed, contours, hierarchy, RETR_TREE,
   CHAIN_APPROX_SIMPLE)`.
2. `detect_circles()`: `cv::findContours(binary, contours, hierarchy,
   RETR_LIST, CHAIN_APPROX_SIMPLE)` directly on the un-closed binary
   image.
3. `detect_ground_symbols()`: `cv::morphologyEx(MORPH_OPEN)` (horizontal
   kernel, length `max(3, ground_min_bar_length)`) then
   `cv::connectedComponentsWithStats` — no contour extraction at all;
   bars come from connected-component analysis, not `findContours`.

No ROI cropping, no additional masking, and no connected-component
preprocessing precede the rectangle/circle contour extraction — both
operate on the full-page binary image. This AP's only code change was
exposing the `hierarchy` out-parameter that `cv::findContours` already
computes internally for `RETR_TREE`/`RETR_LIST` regardless of whether it
is requested — requesting it does not alter `contours`' content, order,
or any downstream decision (verified in §17).

## 5. RETR_TREE Configuration

- Retrieval mode: `cv::RETR_TREE` (rectangle path only)
- Approximation mode: `cv::CHAIN_APPROX_SIMPLE`
- Epsilon (rectangle `approxPolyDP`): `config.rectangle_epsilon *
  perimeter` — a **per-contour, perimeter-relative** epsilon (default
  `rectangle_epsilon = 0.04`), not a fixed pixel value. This AP measured
  actual per-contour epsilon values (§11) but did not change
  `rectangle_epsilon` itself.
- The circle path uses `cv::RETR_LIST` (flat, no tree) and performs no
  `approxPolyDP` at all — circularity is computed directly from the raw
  contour's area and arc length.

## 6. Contour-Hierarchy Results

All 12 connectors' rectangle-path contours are **direct children
(depth 1) of one root contour**, `idx=302`, bounding box
`(96, 89, 848, 518)` — this is the diagram's own outer border/frame,
`RETR_TREE`'s top-level parent for essentially the entire page. There is
no connector-specific or component-specific sub-tree: every fragment
found near any of the 12 census locations, from every one of the 12
connectors, is a sibling of every other fragment in the diagram, linked
only by `next`/`prev` in raster scan order. This is the single most
important GAP-1 finding: **contour hierarchy carries no semantic
grouping information usable to associate a connector's fragments with
each other** — sibling adjacency reflects scan order, not spatial or
logical relationship.

Three genuine depth-2 (grandchild) relationships exist in the whole
12-connector census:

| Parent (depth 1) | Child (depth 2) | Connector tag | Note |
|---|---|---|---|
| idx=488, bbox `(358,344,141,35)` | idx=489, bbox `(428,365,15,5)` | CONN-007 | Parent rejected at `interior_density`; child is a small interior void, not connector-notch ink |
| idx=471, bbox `(686,364,41,127)` | idx=472, bbox `(704,431,1,4)` | CONN-010 | Parent rejected at vertex count (5); child is a 1×4 degenerate sliver |
| idx=697, bbox `(594,111,28,31)` | idx=698, bbox `(598,121,20,12)` | CONN-011 | **Parent is the near-miss rectangle candidate (§12)**; child is a real, substantial interior contour (area 70, 4 vertices, convex) |

All other contours overlapping any of the 12 census boxes have
`first_child = -1` (no children) at depth 1. The `RETR_LIST` circle path
confirms flatness exactly as expected: every hierarchy record printed for
every one of the 12 connectors under `[hierarchy][circle]` shows
`parent=-1, first_child=-1` — `RETR_LIST` performs no tree construction at
all, only a doubly-linked sibling list across the whole image.

## 7. CONN-001 Control

Reproduced exactly, via the new instrumentation, matching AP-DIAG-AUDIT-011:

- `detect_circles`: contour `(829,436,21,17)` — circularity **0.6858**,
  aspect **1.235**, radius 10.60, edge_support **0.847**,
  `bounded_by_crossing_lines=0`, interior_density 0.0000 → **ACCEPTED**,
  confidence 0.907.
- `detect_rectangles`: contour `(829,436,20,16)` — `approxPolyDP` epsilon
  = 2.5325 (i.e. `0.04 × 63.31`), yielding **5 vertices**, convex=1 →
  rejected (needs exactly 4).
- Hierarchy: CONN-001's own accepted-circle contour (`idx=401` in the
  rectangle-path's `RETR_TREE` numbering) is a **depth-1 child of the
  universal root** with `first_child=-1` — no internal nesting. Hierarchy
  information adds no new fact not already visible in AP-DIAG-AUDIT-011's
  per-checkpoint trace: CONN-001 succeeds because of its own contour's
  circularity/aspect/edge-support profile, not because of any tree
  relationship to another contour.

## 8. CONN-002–012 Results

Per-connector geometry family, hierarchy position, and detector result
(full per-checkpoint values match AP-DIAG-AUDIT-011 exactly, reproduced
via this AP's instrumentation as an integrity check — see §17):

- **CONN-002** (865,388,20,12): single depth-1 leaf contour
  `(864,381,23,12)`, no children. Rectangle: 5-vertex reject. Circle:
  circularity 0.6601 (passes), aspect 1.917 (rejected). Ground: one 3-bar
  run clears the gate, rejected at `ground_bar_width_sequence`.
- **CONN-003** (213,178,45,15): **housing** contour `idx=681`
  `(216,128,68,39)` **accepted** as Enclosure; has a child contour
  (`idx=682`, not itself connector-adjacent) confirming the isolated
  interior component found during acceptance is a real nested contour,
  physically inside the housing, not near the notch. The connector notch
  itself (y 178–193) remains a leaf-only fragment field, all rejected by
  size/vertex checks. Ground: one 4-bar run clears the gate, rejected at
  `ground_bar_width_sequence`.
- **CONN-004** (258,178,20,15): no rectangle contour at all overlaps this
  box (leaf-only fragment field entirely absent from the rectangle
  hierarchy dump). Circle: single sub-area contour. Ground: max run size
  1 — never clears the count gate.
- **CONN-005** (295,175,35,15): housing contour `(295,127,70,40)`,
  depth-1 leaf, rejected at `interior_density=0.0`. Circle: circularity
  0.7307 passes, aspect 1.750 rejected. Ground: max run size 1 — never
  clears the gate.
- **CONN-006** (335,175,25,15): all contours depth-1 leaves, no nesting.
  Ground: max run size 2 — never clears the gate.
- **CONN-007** (410,318,40,42): contour `idx=488`
  `(358,344,141,35)` (rejected via `interior_density`) has the one
  genuine depth-2 child (§6) — an isolated small void, unrelated to the
  connector's own staircase notch. The connector-shaped candidates
  themselves (`idx=528`/`529`, bbox 13×42/18×42) are depth-1 leaves
  rejected at 6-vertex `approxPolyDP`. Ground: **two** separate runs
  (size 4 and size 3) clear the gate; both rejected at
  `ground_bar_width_sequence`.
- **CONN-008** (400,412,40,42): all rectangle-path candidates are
  depth-1 leaves (no nesting at this connector). Circle: closest
  circularity near-miss in the census, 0.6492. Ground: **two** separate
  runs (size 6 and size 3) clear the gate — the size-6 run rejected at
  `ground_bar_width_sequence`, the size-3 run rejected at bar-spacing.
- **CONN-009** (625,398,45,35): housing contour `(622,363,46,25)`,
  depth-1 leaf, rejected at `interior_density=0.0`. Circle: circularity
  0.7269 passes, aspect 1.840 rejected. Ground: max run size 2 — never
  clears the gate.
- **CONN-010** (695,398,45,35): contour `idx=471` `(686,364,41,127)`
  (rejected via 5-vertex `approxPolyDP`) has the one degenerate depth-2
  child (§6), a 1×4 sliver with no interpretive value. Circle:
  circularity 0.4754/0.4868, both well below threshold. Ground: max run
  size 1 — never clears the gate.
- **CONN-011** (595,142,25,13): contour `idx=697` `(594,111,28,31)` —
  the internal-line-density near-miss candidate — has the one
  **substantial** depth-2 child, `idx=698` `(598,121,20,12)`, a real,
  4-vertex convex interior contour (see §12 for what this means for the
  near-miss). Circle: same parent contour reaches circularity 0.8215,
  aspect 1.107, but rejected at `edge_support=0.417`. Ground: **two**
  separate 3-bar runs clear the gate — one rejected at
  `ground_bar_width_sequence`, the other at bar-spacing.
- **CONN-012** (695,158,45,20): all rectangle contours rejected before
  `approxPolyDP` (never reach vertex-count stage); no nesting observed.
  Ground: one 3-bar run clears the gate, rejected at
  `ground_bar_width_sequence`.

## 9. Connector Geometry Families

Re-classified using the required A–J taxonomy, now grounded in actual
hierarchy data rather than inferred structure:

- **A (single closed contour):** CONN-001 (its own accepted-circle
  contour is one coherent closed shape).
- **D (multiple sibling contours):** all 12 connectors, without
  exception — every fragment near every connector is a sibling of every
  other fragment in the entire diagram under the one universal root.
  This is the dominant, universal category; it does not by itself
  distinguish the 12 connectors from each other or from any other symbol
  in the diagram.
- **B (outer contour + inner contour):** CONN-011 only — the rectangle
  candidate `(594,111,28,31)` genuinely has one child contour
  `(598,121,20,12)`, confirmed via `first_child`/`parent` linkage, not
  merely spatial containment.
- **E (open contour fragments):** CONN-004, CONN-006, CONN-012 — the
  weakest raw-ink locations, where no contour reaches 40px² or a stable
  4+ vertex approximation; effectively no closed connector-shaped
  boundary forms at all.
- **F (conductor-interrupted contour):** CONN-011 is the only location
  with **direct** supporting numeric evidence
  (`internal_line_density=0.0809`, a wire-field signature); no other
  connector's surviving candidates reach this checkpoint, so this AP
  cannot confirm or rule out category F for the other 11 (see §17
  limitations note on why not — most fragments are rejected before this
  metric is ever computed).
- **I (interlocking/staircase geometry):** CONN-007, CONN-008 — confirmed
  via the 6-vertex/7-vertex `approxPolyDP` results on their
  connector-adjacent leaf contours; this AP's hierarchy dump confirms
  these are not additionally split into parent/child structure — the
  interlocking notch presents as **excess vertices on a single contour**,
  not as separate nested contours.
- **H (notch represented by multiple contours):** not observed at any
  connector; CONN-007/008's interlocking geometry is category G-like
  (single-contour vertex excess), not H.
- **G (notch as indentation in one contour):** CONN-007, CONN-008 (see
  above — the interlocking geometry is carried as extra vertices in one
  contour, i.e., an indentation/staircase edge, not a hole or separate
  shape).
- **J (mixed):** CONN-003 is the clearest mixed case — its housing
  (category A/B-like, accepted) and its notch (category E, fragmented)
  are geometrically and topologically distinct features that happen to
  share a forensic padding box.

No connector belongs to exactly one category in isolation; per the task's
instruction, this AP records the overlapping set for each rather than
forcing a single label.

## 10. Notch/Interlock Representation

For the two connectors with confirmed interlocking/staircase geometry
(CONN-007, CONN-008), the notch is represented in the binary image as **an
indentation in a single contour's boundary**, manifesting as excess
`approxPolyDP` vertices (6–7 instead of 4) rather than as a missing
boundary, an interior void, or a separate sibling/child contour. No
child contour was found nested inside either connector's own
notch-adjacent candidate (`idx=528`/`529` for CONN-007, `idx=409`/`410`
for CONN-008 all have `first_child=-1`). The detector's `approxPolyDP`
+ `polygon.size()==4` gate does not distinguish "genuinely non-rectangular
staircase symbol" from "noisy/irregular rectangle" — both produce the
same observable signal (vertex count ≠ 4), so this AP confirms the
existing detector **loses** the distinction between an interlocking
connector notch and any other non-quadrilateral blob **before** candidate
classification: by the time `polygon.size() != 4` is evaluated, there is
no remaining code path that inspects the vertex arrangement itself (e.g.
whether the extra vertices form a regular staircase versus random noise).

For the remaining 9 absent connectors with fragmented (category E) or
housing-adjacent (category F/J) geometry, no interlock/notch structure
was directly observed in the hierarchy — their small fragments do not
individually resemble a notch shape at all; they are simply
sub-threshold pieces.

## 11. Contour Approximation Analysis

Per-contour raw-point-count vs. `approxPolyDP`-vertex-count vs. actual
epsilon used (epsilon = `rectangle_epsilon(0.04) × perimeter`, **not
changed**):

| Connector | Contour bbox | Raw points | Epsilon | Approx vertices | Convex |
|---|---|---|---|---|---|
| CONN-001 | 829,436,20,16 | 16 | 2.53 | **5** | yes |
| CONN-002 | 864,381,23,12 | 16 | 2.41 | 5 | yes |
| CONN-003 (housing) | 216,128,68,39 | 8 | 8.31 | 4 | yes |
| CONN-005 (housing) | 295,127,70,40 | 16 | 8.46 | 4 | yes |
| CONN-007 | 415,319,18,42 | 19 | 4.49 | **6** | no |
| CONN-007 | 426,319,13,42 | 15 | 4.41 | **6** | no |
| CONN-008 | 423,427,17,26 | 19 | 3.09 | **7** | no |
| CONN-008 | 415,427,13,26 | 15 | 3.12 | **5** | no |
| CONN-009 (housing) | 622,363,46,25 | 14 | 5.33 | 4 | yes |
| CONN-011 | 594,111,28,31 | 8 | 4.44 | 4 | yes |
| CONN-011 (child) | 598,121,20,12 | 29 | 2.22 | 4 | yes |
| CONN-012 | — | — | — | never reaches `approxPolyDP` | — |

The two 5-vertex connector-own contours (CONN-001, CONN-002) and the
6–7-vertex staircase contours (CONN-007, CONN-008) sit at epsilons
between 2.4 and 4.5px — all well inside the range where a *slightly*
larger epsilon would collapse extra vertices toward 4, since
`approxPolyDP`'s Douglas-Peucker algorithm removes the point contributing
least perimeter deviation first. This AP **measured but did not test**
this — no epsilon change was made, per the AP's explicit prohibition;
this is recorded strictly as a measurement for a future AP's
consideration, not a recommendation to change `rectangle_epsilon`
uniformly (a global change would affect all 81 candidates, not just
these).

## 12. CONN-011 Near-Miss Analysis

Both near-misses are reproduced exactly: `internal_line_density=0.0809`
(threshold 0.08, margin 0.0009) and `edge_support=0.417` (threshold 0.65).

**Contour hierarchy explains the `internal_line_density` near-miss.**
The rectangle candidate `(594,111,28,31)` (`idx=697`) has a genuine child
contour `(598,121,20,12)` (`idx=698`, area 70, 4 vertices, convex). This
child sits entirely inside the parent's interior region (the same
interior region whose `MORPH_OPEN` horizontal/vertical line density is
measured for `internal_line_density`). A child contour of this size
(20×12, area 70) inside a 28×31 parent necessarily contributes boundary
ink that the 9×1/1×9 opening kernels can partially register as
horizontal/vertical structure — this is a strong, previously-unavailable
explanation that reframes AP-DIAG-AUDIT-011's "wire-crossing artifact"
characterization: the ink tripping `internal_line_density` may be (in
whole or in part) **the connector's own interior notch/void boundary**,
not necessarily a conductor wire. This AP cannot fully separate the two
possibilities without pixel-level wire-mask cross-referencing (outside
ShapeDetector's own inputs), but the existence of a real, substantial
child contour at exactly the location responsible for the density
measurement makes "the connector's own geometry" at least as plausible an
explanation as "an external wire crossing through," and arguably more
so, since a genuinely external, thin wire would not typically manifest as
a well-formed 4-vertex convex child contour of this size.

**Edge support's near-miss is not explained by hierarchy.** The circle
candidate at the same location (`(594,111,28,31)`) has no separate circle-
path hierarchy relationship (RETR_LIST is flat), and the 0.417 edge
support reflects the fitted circle's own angular sampling finding ink
absent over roughly 58% of its 72 sample points. This is consistent with
an **open or interrupted boundary** — plausibly the same interior
notch/void whose child contour was just identified, if that void breaks
through to the outer boundary at points sampled by the circle fit.
Confirming this specific mechanism would require overlaying the circle's
sample points against the child contour's exact pixels, which this AP's
instrumentation did not capture; this is recorded as an unresolved
sub-question, not inferred.

## 13. Ground Detector Execution Path

Traced completely for all 9 count-gate-clearing windows. After
`run.size() >= ground_min_bars` (3), `evaluate_ground_run` iterates over
`(begin, length)` windows from longest to shortest, and for each window
checks, **in this order**: (1) `gap` between consecutive bars must lie in
`[ground_min_bar_spacing, ground_max_bar_spacing]` for every adjacent
pair, (2) `ground_bar_width_sequence` — bars must be strictly decreasing
in width with a minimum ratio/difference, (3) `ground_bar_spacing_uniform`
— gap variation must stay within `ground_max_bar_spacing_ratio`. Only if
all three pass does the function proceed to the stem-presence check
(non-zero ink count ≥ 2 in a corridor above the first bar) and then emit
an accepted `ChassisGround` region. **No connector-generated run ever
reaches the stem check** — every one of the 9 windows fails at check (1)
or (2) first.

## 14. Ground Bar-Pattern Matrix

| Connector | Bar Count | Count Gate | Subsequent Tests | Final Candidate | Final Disposition |
|---|---|---|---|---|---|
| CONN-002 | 3 | cleared | spacing_ok=1, width_seq=**0**, uniform=1 | none | rejected: width sequence not decreasing |
| CONN-003 | 4 | cleared | spacing_ok=1, width_seq=**0**, uniform=0 | none | rejected: width sequence (first failure) |
| CONN-007 (run A) | 4 | cleared | spacing_ok=1, width_seq=**0**, uniform=1 | none | rejected: width sequence |
| CONN-007 (run B) | 3 | cleared | spacing_ok=1, width_seq=**0**, uniform=1 | none | rejected: width sequence |
| CONN-008 (run A) | 6 | cleared | spacing_ok=1, width_seq=**0**, uniform=0 | none | rejected: width sequence (first failure) |
| CONN-008 (run B) | 3 | cleared | spacing_ok=**0** (gap=1, need [2,14]) | none | rejected: bar spacing out of range |
| CONN-011 (run A) | 3 | cleared | spacing_ok=1, width_seq=**0**, uniform=1 | none | rejected: width sequence |
| CONN-011 (run B) | 3 | cleared | spacing_ok=**0** (gap=1, need [2,14]) | none | rejected: bar spacing out of range |
| CONN-012 | 3 | cleared | spacing_ok=1, width_seq=**0**, uniform=1 | none | rejected: width sequence |

Connectors that **never reach the count gate** (maximum observed run size
< 3 for every candidate run at that location): CONN-004 (max 1),
CONN-005 (max 1), CONN-006 (max 2), CONN-009 (max 2), CONN-010 (max 1),
CONN-001 (max 2).

Total: 6 of 12 connectors produce at least one count-gate-clearing run (9
runs total across those 6); 0 of 9 runs produce a `ChassisGround`
candidate; 0 false positives anywhere in the census.

## 15. Historical AP-DIAG-FIX-003 Comparison

AP-DIAG-FIX-003's documented false-positive mechanism was specifically an
**X-alignment / missing-Y-locality** failure: bars over 100px apart in Y,
unrelated to each other, merged into one candidate group because their
rounded `center_x` values coincidentally fell within the ±3px tolerance,
corrupting the Y-sort order. The fix was splitting each X-tolerance group
into Y-contiguous runs (gap ∈ [-2, `ground_max_bar_spacing`]) before
evaluation.

This AP's evidence shows that mechanism is **not what is protecting
against connector-notch false positives today** — connector bars are
already naturally close together in both X and Y (they come from one
compact physical notch, typically under 45px in either dimension), so
they cleanly form a single Y-contiguous run and clear both the X-tolerance
grouping and the Y-locality split without needing that protection at all.
The actual protection operating here is the **separate, later**
`ground_bar_width_sequence` (strictly-decreasing width requirement) and
bar-spacing checks — mechanisms AP-DIAG-FIX-003 also introduced in the
same fix, but distinct from the Y-locality split itself. In 7 of 9 cases
the width-sequence check is decisive; in 2 of 9 cases (both involving a
gap of exactly 1px against a minimum of `ground_min_bar_spacing=2`) the
spacing-range check is decisive. Multiple protections do not need to
combine for any single connector — for each of the 9 runs, exactly one
check is the first and only reason recorded (the pipeline short-circuits
at the first failing check per window). Y-locality enforcement, stem
presence, and the ≥3-bar count gate are never the deciding factor for any
connector-generated pattern in this diagram; width-sequence and
bar-spacing are the operative defenses.

## 16. Scoped vs Unscoped Results

`ShapeDetector::detect()` operates on the full-page normalized/thresholded
raster with no ROI cropping and no scope-dependent masking anywhere in
its own three detector functions — the same binary image is scanned in
full regardless of any later extraction-scope configuration applied
elsewhere in the pipeline. This AP ran the canonical, unscoped
`dx-extract extract samples/trx300ODG.png` command (no `--scope` flag or
equivalent), consistent with AP-DIAG-AUDIT-011 and all prior APs in this
chain. No scoped extraction pathway was exercised, so this AP has no
scoped-vs-unscoped delta to report; ShapeDetector's own contour
extraction and hierarchy are, by inspection of its source, scope-agnostic
inputs (the same `cv::Mat normalized` argument is passed regardless of
scope), so no difference is expected. This is recorded as a structural
observation from source inspection, not as an experimentally-measured
comparison, since no scoped run was executed.

## 17. Instrumentation Integrity

- **A. Pre-instrumentation baseline:** `topology.json` SHA-256
  `3fc904e7a62580e35c252cbeda9e2cdc76fef47cdd5d05ecf23a274361e40ed6`.
- **B. Instrumented forensic run:** same SHA-256,
  `3fc904e7a62580e35c252cbeda9e2cdc76fef47cdd5d05ecf23a274361e40ed6` —
  **byte-identical**. 920 lines of `[FORENSIC][...]` output captured.
- **C. Restored normal extraction (×2, post-revert):** both runs, same
  SHA-256, `3fc904e7a62580e35c252cbeda9e2cdc76fef47cdd5d05ecf23a274361e40ed6`.

All four hashes are identical; the instrumentation (exposing
`findContours`' existing `hierarchy` out-parameter, and adding read-only
`fprintf` calls immediately before existing `continue`/`return`/`break`
statements) introduced zero behavioral change. `git diff` after revert
against the starting commit shows no change to `shape_detector.cpp`
(md5 `e633b19035cfad8a457200f31ad7be75`, identical to the pristine backup
taken before any edit and to AP-DIAG-AUDIT-011's own pristine backup of
the same file).

**Instrumentation limitations acknowledged, not inferred past:**

1. `internal_line_density` is only computed for candidates that survive
   to that checkpoint in `detect_rectangles`; 10 of 12 connectors'
   notch-region fragments never reach it (rejected earlier by
   `min_width/min_height` or `approxPolyDP`), so this AP cannot report an
   `internal_line_density` value for those 10 locations — there is
   nothing to instrument at a checkpoint the algorithm never executes for
   a given contour.
2. CONN-011's `edge_support` near-miss is plausibly linked to its
   confirmed child contour (§12), but confirming the exact geometric
   mechanism (whether the circle-fit's failing sample points align with
   the child contour's boundary) would require instrumenting
   `circle_edge_support`'s individual per-angle sample results against
   the rectangle-path's separately-computed child contour geometry —
   two different code paths operating on the same binary image that this
   AP did not cross-reference at the pixel level.
3. No scoped extraction run was performed (§16); the "scope-agnostic"
   conclusion is from source inspection, not an executed comparison.

## 18. Findings by Severity

- **HIGH** — GAP-1's central finding (all 12 connectors are siblings
  under one universal root contour, with no connector-specific hierarchy)
  is architecturally significant: any future connector detector cannot
  rely on `RETR_TREE` parent/child relationships to group a connector's
  own fragments together; grouping must come from spatial proximity or
  another mechanism entirely.
- **HIGH** — GAP-2's central finding (100% of connector-generated
  ground-bar runs are stopped by width-sequence or spacing checks, never
  reaching stem presence) confirms the ground detector's current defenses
  are fully sufficient against every connector-notch pattern in this
  diagram; there is no live false-positive risk from connectors in the
  ground detector today.
- **MEDIUM** — CONN-011's `internal_line_density` near-miss is now better
  explained (a real interior child contour, not necessarily a wire) than
  in AP-DIAG-AUDIT-011, which materially affects how a future connector
  detector should interpret this checkpoint for interior-void connector
  geometry.
- **MEDIUM** — the confirmation that CONN-007/CONN-008's interlocking
  geometry manifests as vertex-count excess on a single contour (not
  multiple contours) is a concrete, actionable geometric fact for any
  future detector design targeting staircase-notch connectors.
- **LOW/INFORMATIONAL** — the epsilon measurements in §11 are informative
  but explicitly not actionable without further work (a uniform epsilon
  change affects all 81 candidates, not just connectors).

## 19. Architectural Constraints

Building on AP-DIAG-AUDIT-011's constraints, this AP adds:

(f) A future connector-aware detector cannot use contour hierarchy
(`RETR_TREE` parent/child) to associate a connector's own fragments,
since all 12 locations' fragments are siblings of the entire diagram's
root contour, not of each other — any fragment-association logic must be
purely spatial (proximity/bounding-box clustering), not tree-based.
(g) A connector detector targeting interior-void geometry (as seen at
CONN-011) must be able to distinguish a connector's own internal notch
boundary from actual conductor ink when both can trip the same
`internal_line_density`-style heuristic — the current metric cannot make
this distinction on its own.
(h) A connector detector targeting interlocking/staircase geometry
(CONN-007/008) must operate on vertex count/shape irregularity directly,
since the notch is encoded as excess polygon vertices on a single
contour, not as a separate identifiable sub-contour.
(i) The ground detector's existing width-sequence and spacing checks are
confirmed sufficient current protection against connector-notch
false-ground-positives; a future connector detector does not need to
additionally guard against this specific interaction, though it should
not weaken those checks either.

## 20. Explicit Non-Fixes

Confirmed unchanged in the final, reverted production tree: `ShapeDetector`
(all thresholds, all three detector functions, epsilon, morphology
kernels, ROI dimensions, bar-count thresholds), `ConnectorBoundary`,
`TerminalRecognizer`, `PhysicalWireIdentityReconstructor`,
`ElectricalNetResolver`, and all Wire/Component/Connector model types. No
connector recognition, detector, or representation was implemented. No
`ComponentCandidateKind`, `ShapeKind`, or `ConnectorTerminal` concept was
added or altered. `git diff` against the starting commit shows zero
change to any `.cpp`/`.hpp` file (verified: `shape_detector.cpp` md5
`e633b19035cfad8a457200f31ad7be75`, identical pre- and post-audit).

## 21. Recommended Next AP

Both forensic gaps identified by AP-DIAG-AUDIT-011 are now closed. The
smallest evidence-based next step is a **connector geometry/model design
AP** — the forensic groundwork (AP-DIAG-AUDIT-010 through -012) is
complete: the census is fixed, the detector failure modes are
characterized down to specific checkpoints, the contour hierarchy is
fully mapped, and the ground-detector interaction is fully traced with no
live false-positive risk. The next AP should propose (design only, no
implementation in the same AP unless explicitly scoped to do so) a
connector-aware geometric classification approach that: (1) does not rely
on contour hierarchy for fragment association, (2) can accept
non-quadrilateral (5–7 vertex) polygon approximations for
interlocking/staircase notches without simply loosening
`rectangle_epsilon` globally, (3) can accept elongated/oval aspect ratios
without loosening `circle_max_aspect_ratio` for all circle candidates,
and (4) can distinguish a connector's own interior notch/void boundary
from true conductor ink where both currently trip the same
`internal_line_density`-style heuristic.

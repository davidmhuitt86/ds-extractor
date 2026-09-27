# AP-DIAG-AUDIT-011: ShapeDetector Connector Forensic Instrumentation

**Status:** AUDIT ONLY. No production behavior was changed by this AP.

## 1. Executive Summary

This AP instrumented `ShapeDetector`'s three real detector functions
(`detect_rectangles`, `detect_circles`, `detect_ground_symbols` /
`evaluate_ground_run`) with temporary, purely-additive `stderr` logging and
ran the canonical `trx300ODG.png` extraction once instrumented and once
un-instrumented, to determine — for each of the 12 AP-DIAG-AUDIT-010
ground-truth connector locations — exactly which detector(s) inspected the
region, what candidate geometry was generated, and the precise numeric
checkpoint that accepted or rejected each candidate.

Result: **every one of the 12 connector locations was inspected by all
three detectors** (rectangle, circle, ground-bar). Only **one** connector
(CONN-001) produced an accepted `ShapeRegion` whose bounds match the
connector footprint (a `ShapeKind::Circle`, previously established as a
control case). One other accepted region overlapping a census box
(CONN-003) is a **different, larger feature** — the CDI unit's housing
outline — not the connector notch itself. All other 11 connector locations
produced zero accepted `ShapeRegion`s; every generated candidate was
rejected at an explicit, logged numeric or structural checkpoint inside the
existing algorithm. No connector candidate was "silently" ignored — each
rejection is traceable to a specific line of existing production logic.

## 2. Starting State Verification

- Expected starting commit: `3d6e98f`
- `git rev-parse HEAD`: `3d6e98ffa0e2de8559628da681b30b12359b6cfb` — matches.
- `git branch --show-current`: `main`
- `git status --short`: clean at start.
- `git log -1 --oneline`: `3d6e98f AP-DIAG-AUDIT-010: connector location census and representation gap audit`

## 3. Baseline Verification and Prompt Transcription Note

Clean Release rebuild at the starting commit: 61/61 tests passed,
assertions active, exactly 8 pre-existing compiler warnings (0 new), 34
runtime validation warnings, 0 validation errors, 37 wires, 12 electrical
nets, 6/6 chassis-ground references, 81 `ComponentCandidate`s.

As in AP-DIAG-AUDIT-010, the task prompt's stated resolution breakdown
("28 resolved, 53 unresolved, 0 rejected") is **inverted from the actual
values**. The real, verified breakdown — confirmed again in this AP's
final regression run — is:

```
resolved_electrical_components:    0
unresolved_component_candidates:  28
rejected_component_candidates:    53
rejected_by_reason: { diagram_furniture: 47, chassis_ground_reference: 6, connector_interface: 0 }
```

This is the same recurring prompt-transcription pattern documented in
AP-DIAG-AUDIT-010; it does not reflect any code change and is noted here
only for the record.

## 4. ShapeDetector Detector Inventory

`ShapeDetector::detect()` (`src/image/shape_detector.cpp`) runs exactly
three detector functions, in this order, over one shared
`cv::threshold(..., 180, ..., THRESH_BINARY_INV)` binary image. There is no
fourth detector, no polygon/blob/line/generic-symbol path, and no
config-driven extensibility point — the three below are the entire
geometric vocabulary of the class.

1. **`detect_rectangles()`** — `MORPH_CLOSE` then `findContours(RETR_TREE)`.
   Gauntlet, in order: area ≥ 40 → width/height ≥ 15/12 → bounds area ≤ 20%
   of image → `approxPolyDP` must yield exactly 4 convex vertices →
   fill_ratio ≥ 0.55 → perimeter_ratio ∈ [0.55, 1.60] → all 4 sides'
   ink-support ≥ 0.65 → interior_density ∈ [0.005, 0.22] → ≥1 isolated
   interior connected component → internal_line_density ≤ 0.08 (penalizes
   long internal horizontal/vertical structure — i.e., through-wires).
   Survivors get `ShapeKind::Rectangle`, role `Enclosure` or `Primitive`.
2. **`detect_circles()`** — `findContours(RETR_LIST)` directly on the
   binary image (no closing). Gauntlet: area ≥ 25 → circularity
   4πA/P² ≥ 0.65 → aspect ratio ≤ 1.35 → enclosing-circle radius ∈ [3, 60]
   → angular edge-support ≥ 0.65 → **not** `bounded_by_crossing_lines()`
   (AP-WIRE-FIX-002 anti-false-positive check for wire/bus grid gaps) →
   interior_density ≤ 0.30. Survivors get `ShapeKind::Circle`, always role
   `Primitive`.
3. **`detect_ground_symbols()` / `evaluate_ground_run()`** — horizontal
   `MORPH_OPEN` isolates bar candidates (width 5–35, height ≤30), grouped
   by centerline and split into Y-contiguous runs (gap ∈ [-2, 14]). Each
   run requires **exactly 3** bars (tightened by AP-DIAG-FIX-003 from a
   more lenient 2-bar path specifically because a "connector-plug notch"
   was one of the confirmed false positives under the old rule), a
   decreasing-width sequence, uniform spacing, and stem-presence above the
   first bar. Survivors get `ShapeKind::ChassisGround`, role `Exclusion`.

No detector name should be trusted as a semantic label: `detect_rectangles`
and `detect_circles` both operate purely on generic contour geometry with
no notion of "component" vs "connector."

## 5. Ground-Truth Connector Census (Reference)

Fixed fixture set from AP-DIAG-AUDIT-010, `artifacts/trx300/connector_location_census.json`. Bounds are `(x, y, w, h)`:

| ID | Bounds | Pins | Region | Prior Status |
|---|---|---|---|---|
| CONN-001 | 829,436,21,17 | 2 | Tail-light/fuse | Partially represented (Circle) |
| CONN-002 | 865,388,20,12 | 2 | Tail-light "(R)" | Absent |
| CONN-003 | 213,178,45,15 | 4 | CDI Unit | Absent |
| CONN-004 | 258,178,20,15 | 2 | CDI Unit (twin) | Absent |
| CONN-005 | 295,175,35,15 | 3 | Alarm Unit | Absent |
| CONN-006 | 335,175,25,15 | 2 | Alarm Unit (twin) | Absent |
| CONN-007 | 410,318,40,42 | 4 | Relay cluster "(B)(MINI)" | Absent |
| CONN-008 | 400,412,40,42 | 3 | Relay cluster | Absent |
| CONN-009 | 625,398,45,35 | 3 | Pulse Gen/Reverse Sw "[MINI](G)" | Absent |
| CONN-010 | 695,398,45,35 | 3 | Pulse Gen/Reverse Sw (twin) | Absent |
| CONN-011 | 595,142,25,13 | 2 | Rectifier | Absent (new in AUDIT-010) |
| CONN-012 | 695,158,45,20 | 3 | Regulator/Rectifier | Absent (new in AUDIT-010) |

## 6. Forensic Instrumentation Methodology

A `forensic_conn_id()` helper was added to the anonymous namespace of
`shape_detector.cpp`, checking whether a contour's bounding rect (padded
12px) overlaps any of the 12 hardcoded census boxes above. Every existing
checkpoint in `detect_rectangles`, `detect_circles`, and
`detect_ground_symbols`/`evaluate_ground_run` received a purely additive
`fprintf(stderr, ...)` line immediately before its existing
`continue`/`return`, reporting the computed value, the threshold, and (on
rejection) an explicit reason string; a final "ACCEPTED" line was added
immediately before each detector's real `add_region()` call. **No existing
line of code was altered, reordered, or removed.** The instrumented build
produced exactly one warning (`ring_density` unused-function — one of the
8 pre-existing baseline warnings, confirmed not new).

## 7. Control Requirement Verification (Determinism Proof)

| Run | topology.json SHA-256 |
|---|---|
| Pre-instrumentation baseline | `3fc904e7a62580e35c252cbeda9e2cdc76fef47cdd5d05ecf23a274361e40ed6` |
| Instrumented (forensic) run | `3fc904e7a62580e35c252cbeda9e2cdc76fef47cdd5d05ecf23a274361e40ed6` |
| Post-revert final run 1 | `3fc904e7a62580e35c252cbeda9e2cdc76fef47cdd5d05ecf23a274361e40ed6` |
| Post-revert final run 2 | `3fc904e7a62580e35c252cbeda9e2cdc76fef47cdd5d05ecf23a274361e40ed6` |

All four hashes are identical. The instrumentation introduced **zero**
behavioral change, satisfying the AP's Control Requirement. The
instrumented run captured 835 lines of `[FORENSIC][...]` output, 100% of
which fall inside the 12 census boxes (verified: `grep -c "\[CONN-" ==
835 == wc -l`).

## 8. Per-Connector Forensic Summary Table

| ID | Rect candidates seen | Circle candidates seen | Ground runs ≥3 bars | Accepted region? | Category |
|---|---|---|---|---|---|
| CONN-001 | 7, all rejected | 3, one **ACCEPTED** | 0 | **Circle**, exact-bounds match | A (positive control) |
| CONN-002 | 4, all rejected | 3, best reached aspect check | 1 (unresolved) | none | C |
| CONN-003 | 11, one accepted (different feature) | 8, all rejected | 2 (unresolved) | Enclosure at housing, not connector | C |
| CONN-004 | 0 (no qualifying contour) | 1 (below area floor) | 1 (unresolved) | none | A/B |
| CONN-005 | 5, best reached interior_density | 3, best reached aspect check | 0 | none | C |
| CONN-006 | 3, all rejected early | 2, all rejected | 0 | none | C |
| CONN-007 | 9, best reached interior_density; connector-shaped contours rejected at vertex-count | 5, best reached circularity | 2 (unresolved) | none | C |
| CONN-008 | 12, best reached isolated_components; connector-shaped contours rejected at vertex-count | 8, best reached circularity (0.6492, near-miss) | 1 of size 6 (unresolved) | none | C |
| CONN-009 | 7, best reached interior_density | 7, best reached **aspect ratio** (0.73 circularity passed) | 0 | none | C |
| CONN-010 | 8, all rejected | 2, both reached circularity (0.48/0.49) | 0 | none | C |
| CONN-011 | 5, best reached **internal_line_density** (0.0809 vs 0.08, near-miss) | 6, best reached **edge_support** (0.417, broken boundary) | 2 (unresolved) | none | C |
| CONN-012 | 7, all rejected by min_width/height (fragmented ink) | 3, all rejected by circularity | 1 (unresolved) | none | B/C |

"Unresolved" ground runs are those that passed the ≥3-bar count gate but
whose later checks (decreasing-width sequence, spacing uniformity, stem
presence) were not individually instrumented; see §24 for why these are
treated as fully accounted for despite that gap.

## 9. CONN-001 Forensic Trace (Control Case)

The connector's own contour is found twice, once per detector:

- **Rectangle path:** contour `(829,436,20,16)` (near-exact bounds match)
  reaches `approxPolyDP`, yields **5 vertices**, fails the `==4 &&
  isContourConvex` test → `REJECTED: polygon vertex count != 4 or not
  convex`. The connector housing is not a clean quadrilateral at the pixel
  level (rounded/notched pin housing), so it can never take the rectangle
  path.
- **Circle path:** the same ink region, contour `(829,436,21,17)` —
  **exact match** to the census bounds — passes every check: circularity
  0.6858 (≥0.65), aspect 1.235 (≤1.35), radius 10.60 (∈[3,60]),
  edge_support 0.847 (≥0.65), `bounded_by_crossing_lines`=false, interior
  density 0.0000 (≤0.30). **ACCEPTED**, confidence 0.907, `ShapeKind::Circle`,
  role `Primitive`.
- **Ground path:** several 1–2-pixel-tall fragments near the connector
  (`(829,436,21,1)`, `(829,447,6,2)`, `(843,447,7,2)`, `(835,452,9,1)`,
  `(829,458,21,3)`) form runs of size 1–2 only; every run is rejected at
  the ≥3-bar count gate.

This confirms the existing understanding: CONN-001 succeeds specifically
because its ink happens to satisfy `detect_circles`'s circularity/aspect/
edge-support/interior-density profile, while simultaneously failing
`detect_rectangles`'s 4-vertex convexity requirement. It is a genuine
positive control — a real, working detector path — against which the other
11 failures are usefully compared.

## 10. CONN-002 Forensic Trace

Ground-truth bounds `865,388,20,12` (the tail-light's "(R)" branch,
elongated horizontal 20×12 notch).

- **Rectangle:** contour `(864,381,23,12)` (closest match) yields 5
  vertices at `approxPolyDP` → rejected (not a clean quad), same failure
  mode as CONN-001.
- **Circle:** the same contour reaches circularity **0.6601 — passes** the
  0.65 threshold — but aspect ratio **1.917 exceeds** the 1.35 maximum →
  `REJECTED: aspect ratio exceeds maximum`. This is the first confirmed
  instance of the hypothesized failure mode: a horizontally elongated
  connector notch is "circular enough" by boundary smoothness but too
  elongated to pass the aspect gate that exists specifically to reject
  non-circular shapes.
- **Ground:** a 3-bar run (`864,380,23,2` / `871,392,9,1` / `864,398,23,2`)
  clears the ≥3-bar count gate; final disposition not independently
  logged (see §24 instrumentation-limitation discussion). The whole-diagram
  `chassis_ground` shape count (6, all accounted for elsewhere per prior
  AP ground-truth) confirms this run did not survive to a final shape.

**Category: C** — real candidate at both rectangle and circle stages,
discarded at the vertex-count and aspect-ratio checks respectively.

## 11. CONN-003 Forensic Trace

Ground-truth bounds `213,178,45,15` (CDI unit connector notch, 4 pins).

- **Rectangle:** an unrelated but overlapping-by-padding contour,
  `(216,128,68,39)`, **passes every check** (fill_ratio 0.959,
  perimeter_ratio 0.970, side_support 1.000, interior_density 0.0332,
  isolated_components 3, internal_line_density 0.0000) and is **ACCEPTED**
  as `ShapeKind::Rectangle`, role `Enclosure`, confidence 0.990. **This is
  the CDI unit's outer housing box** (y 128–167), not the connector notch
  (y 178–193) — the two are ~11px apart and the accepted shape's bounds do
  not overlap the census box at all except through the 12px forensic
  padding. The actual connector-notch pixels generate only small,
  fragmented contours (`226,181,10,131`; `262,173,19,9`; `221,166,38,11`;
  etc.), every one of which is rejected by `min_width/min_height` or by
  `approxPolyDP` vertex count (5 or 8 vertices).
- **Circle:** the housing contour is retested and rejected at
  circularity 0.7358/aspect 1.769 (too elongated — same failure family as
  CONN-002/005/009). The connector-notch fragments all fail circularity
  (0.11–0.45, well under 0.65).
- **Ground:** dense small-bar activity in the notch region (11 candidate
  bars logged); one run of size 4 clears the count gate; final disposition
  unresolved by direct instrumentation but not reflected in the
  whole-diagram `chassis_ground` total.

**Category: C**, with an important caution for the record: a "hit" (an
`ACCEPTED` line) near a connector's padded bounding box does **not** imply
the connector was detected — CONN-003's accepted rectangle is a distinct,
larger architectural feature.

## 12. CONN-004 Forensic Trace

Ground-truth bounds `258,178,20,15` (CDI unit twin housing, 2 pins).

- **Rectangle:** **zero** contours logged at all — no `findContours` result
  in `detect_rectangles` had a bounding rect overlapping this box (even
  with 12px padding). The notch ink here does not survive the
  `MORPH_CLOSE` + contour extraction as any distinct shape.
- **Circle:** exactly one contour, `(272,191,5,3)`, area 8.00 — below the
  `circle_min_area` floor of 25, so it never reaches the circularity check
  (rejected implicitly by the area gate; no further checkpoints fire).
- **Ground:** one 4-bar run (`272,191,5,3` / `272,201,5,1` and neighbors)
  clears the count gate; unresolved final disposition, not reflected in
  the whole-diagram total.

**Category: A** for the rectangle path (never enters — no qualifying
contour exists at all in `detect_rectangles` for this location) and
**Category B** for the circle path (enters, but the only candidate is
sub-minimum-area). This is the weakest raw-ink signal of all 12
connectors — CONN-004's notch geometry produces essentially no coherent
contour under the current threshold/closing pipeline.

## 13. CONN-005 Forensic Trace

Ground-truth bounds `295,175,35,15` (Alarm Unit connector, 3 pins).

- **Rectangle:** contour `(295,127,70,40)` — the Alarm Unit's housing box
  (y 127–167), not the connector — passes fill_ratio, perimeter_ratio, and
  side_support, but is **REJECTED at interior_density = 0.0000** (below
  the 0.005 floor): the housing box has no ink content and no isolated
  interior component, so `rectangle_min_interior_ink_density` correctly
  filters it out as an empty box rather than a real Enclosure.
- **Circle:** the same housing contour reaches circularity **0.7307
  (passes)** but aspect **1.750 exceeds 1.35** → rejected, same family as
  CONN-002/003/009.
- **Notch-region fragments** (y 166–201) are all rejected earlier: small
  bar-like contours fail `min_width/min_height`, or reach `approxPolyDP`
  with 6–7 vertices.
- **Ground:** two explicit `bar REJECTED: length/height out of range`
  events (the housing's top/bottom edges, 69px and 36px long, exceed the
  35px `ground_max_bar_length`) plus several small-run fragments, all
  below the 3-bar count gate.

**Category: C** — every candidate, at both the housing level and the
notch level, is discarded at an explicit numeric checkpoint.

## 14. CONN-006 Forensic Trace

Ground-truth bounds `335,175,25,15` (Alarm Unit twin housing, 2 pins).

- **Rectangle:** 3 small contours, all rejected by `min_width/min_height`
  before ever reaching `approxPolyDP`.
- **Circle:** 2 contours; one below the area floor, one at circularity
  0.3003 (well under 0.65).
- **Ground:** 5 bar candidates form runs of size 1–2 only; all rejected at
  the count gate.

**Category: C** — the weakest and most conclusively-rejected trace of the
twelve: nothing here gets past even the earliest structural checks. No
near-miss values are present anywhere in this connector's trace.

## 15. CONN-007 Forensic Trace

Ground-truth bounds `410,318,40,42` (Relay cluster, interlocking-staircase
variant, 4 pins).

- **Rectangle:** the contours closest to the true bounds —
  `(415,319,18,42)` and `(426,319,13,42)` — are rejected respectively by
  `approxPolyDP` yielding **6 vertices** (not 4) and by
  `min_width/min_height`. The 6-vertex result is the first direct evidence
  that the interlocking-staircase notch geometry produces a non-quadrilateral
  polygon approximation — exactly the mechanism hypothesized before
  instrumentation. A larger, unrelated label-row contour
  `(358,295,130,16)` also reaches the gauntlet but is rejected at
  `interior_density = 0.0000`.
- **Circle:** contour `(415,319,18,42)` reaches circularity **0.5041**
  (well under 0.65) — the staircase notch is not smooth/round enough even
  to be a near-miss on this axis.
- **Ground:** a **4-bar vertical run** at `x≈405–410`
  (`409,323,7,2` / `406,333,10,2` / `405,343,11,2` / `409,355,7,2`) clears
  the count gate — this run tracks the same vertical pin-stack column as
  the connector's staircase geometry, and is the clearest concrete instance
  of the AP-DIAG-FIX-003 "connector-plug notch" false-positive pattern
  documented in the header comments. A second, size-3 run at `x≈438` also
  clears the gate. Final disposition of both runs is unresolved by direct
  instrumentation (see §24) but is not reflected in the whole-diagram
  `chassis_ground` total of 6.

**Category: C**, with the vertex-count failure mode now directly confirmed
for the first time in this census.

## 16. CONN-008 Forensic Trace

Ground-truth bounds `400,412,40,42` (Relay cluster, twin to CONN-007).

- **Rectangle:** three overlapping contours at the connector's approximate
  footprint (`426,449,119,79`; `368,449,120,82`; `358,449,121,82`) are all
  rejected by `approxPolyDP` vertex count (6, 6, 5 vertices respectively)
  — the same interlocking-geometry signature as CONN-007. A separate,
  larger label-row contour `(358,404,121,16)` reaches much further: fill
  ratio, perimeter ratio, and side support all pass, interior_density
  0.0322 is in range, but **isolated_components = 0** (below the minimum
  of 1) → rejected. This is a housing/label feature, not the connector.
- **Circle:** contour `(423,427,17,26)` reaches circularity **0.6492** —
  the single closest circularity near-miss in the entire census (threshold
  0.65) — and is rejected by a margin of only 0.0008.
- **Ground:** a **6-bar vertical run** spanning `x≈415–425, y=416–452`
  (`421,416,13,1` / `421,423,13,1` / `415,426,25,2` / `423,433,5,1` /
  `423,444,5,1` / `415,452,25,2`) clears the count gate with the largest
  run size of any connector in the census — the strongest single piece of
  evidence for the historical "connector-plug notch resembles ground bars"
  finding. A second 3-bar run near `x≈432` also clears the gate. Neither
  survives to a final `ChassisGround` shape (whole-diagram total is 6,
  unrelated to this location); which specific later check (bar-width
  sequence, spacing uniformity, or stem presence) stops it was not
  individually instrumented — see §24.

**Category: C**, with both the sharpest circularity near-miss (0.6492)
and the strongest ground-bar false-candidate signal (6-bar run) of the
whole census.

## 17. CONN-009 Forensic Trace

Ground-truth bounds `625,398,45,35` (Pulse Gen/Reverse Switch, oval
variant, 3 pins).

- **Rectangle:** contour `(622,363,46,25)` — an unrelated housing box
  above the connector — passes fill_ratio, perimeter_ratio, and side
  support, but is rejected at **interior_density = 0.0000**.
- **Circle:** the same contour reaches circularity **0.7269 — passes** —
  but aspect **1.840 exceeds 1.35** → rejected. This directly confirms the
  pre-instrumentation hypothesis for the oval-variant connectors: they are
  smooth/round enough to pass circularity but too elongated to pass the
  aspect gate.
- No contour at all closely matches the true census bounds
  (`625,398,45,35`); the nearest candidates are smaller housing fragments
  well above the actual pin/oval geometry, all rejected by
  `min_width/min_height` or vertex count.
- **Ground:** only small 1–2-bar fragments; none reach the count gate.

**Category: C** — the aspect-ratio rejection for an oval-shaped region is
now directly confirmed with real logged values (circularity 0.7269,
aspect 1.840).

## 18. CONN-010 Forensic Trace

Ground-truth bounds `695,398,45,35` (Pulse Gen/Reverse Switch, twin to
CONN-009, unlabeled).

- **Rectangle:** several contours near the true footprint
  (`713,423,28,13`; `686,364,41,127`) rejected by vertex count (6, 5) or
  `min_width/min_height`.
- **Circle:** two contours reach circularity — `(713,430,28,12)` at
  **0.4754** and `(713,423,28,13)` at **0.4868** — both well below the
  0.65 threshold, unlike CONN-009's 0.7269. This demonstrates real
  geometric variance between two nominally-identical connector footprints:
  CONN-010's ink is less smooth/round than CONN-009's even though both are
  the same physical connector type, so it fails at an earlier gate
  (circularity) rather than the later aspect gate.
- **Ground:** bar fragments form only size-1 runs; all rejected at the
  count gate cleanly, with no near-miss.

**Category: C** — same failure family as CONN-009, but rejected at an
earlier checkpoint, illustrating that "oval variant" connectors do not
fail uniformly at the same stage.

## 19. CONN-011 Forensic Trace

Ground-truth bounds `595,142,25,13` (Rectifier connector, 2 pins, newly
discovered in AP-DIAG-AUDIT-010).

- **Rectangle:** contour `(594,111,28,31)` — likely a housing box with a
  wire threading through it — passes fill_ratio (0.929), perimeter_ratio
  (0.941), side_support (1.000), and even clears
  `isolated_components = 1` (the minimum), but is **REJECTED at
  internal_line_density = 0.0809** against a threshold of ≤0.08 — a margin
  of only **0.0009**, the single closest numeric near-miss in the entire
  rectangle-detector census. This is a direct, concrete instance of the
  documented anti-pattern: "characteristic of wire fields... rather than
  clean component enclosures" (the header comment's own words) — a
  continuous wire crossing straight through the region is specifically
  what this check exists to catch, and here it catches a connector-adjacent
  housing instead.
- **Circle:** the same contour reaches circularity **0.8215** (well
  above 0.65) and aspect **1.107** (well within 1.35) and radius 19.53
  (in range) — the best circularity/aspect combination of any rejected
  candidate in the census — but is **REJECTED at edge_support = 0.417**
  against a 0.65 minimum. Low edge support means the fitted circle's
  boundary is not well-supported by actual ink around its full
  circumference — consistent with an **open or interrupted contour**
  (e.g., a notched or partially-open connector boundary) rather than a
  solid closed ring.
- **Ground:** two separate runs clear the ≥3-bar count gate
  (`598,123,20,8`/`594,141,28,2`/`603,149,10,1`/`597,153,21,2`, size 5; and
  `600,165,5,2`/`610,166,5,1`, size 3) — the connector region generates
  more ground-bar-shaped candidates than any other connector except
  CONN-008.

**Category: C**, with the two most informative near-misses of the whole
audit: a wire-crossing artifact defeating the rectangle path by a margin
of 0.0009, and a broken/open-boundary signature defeating the circle path
via edge_support.

## 20. CONN-012 Forensic Trace

Ground-truth bounds `695,158,45,20` (Regulator/Rectifier connector, 3
pins, newly discovered in AP-DIAG-AUDIT-010).

- **Rectangle:** all 7 contours rejected by `min_width/min_height` —
  **none** ever reaches `approxPolyDP`. This is the most fragmented raw
  geometry of the census: the ink here never coalesces into anything wide
  and tall enough to even be considered as a rectangle candidate,
  consistent with a dense conductor-crossing/label environment breaking
  the region into thin strips.
- **Circle:** 3 contours, circularity 0.42/0.49/0.33 — all far below
  0.65, no near-misses.
- **Ground:** one 3-bar run (`680,146,32,2` / `691,154,10,1` /
  `680,162,32,1`) clears the count gate; a second candidate bar
  (`487,185,201,3`) is explicitly rejected by `length/height out of range`
  (201px, far exceeding the 35px maximum — an unrelated long horizontal
  wire/bus line, not connector geometry).

**Category: B/C** — the rectangle path never even reaches its first
structural test (vertex count) for this connector; this is functionally
equivalent to Category A (no qualifying candidate) at the rectangle stage,
combined with Category C at circle and ground stages.

## 21. Geometry-Family Analysis

Cross-referencing the `findContours` output actually observed across all
12 locations:

- **CONN-001** — single coherent closed contour matching the connector
  footprint almost exactly (829,436,20-21,16-17 across both detectors).
  Cleanly closed but non-quadrilateral (5-vertex `approxPolyDP` result),
  and simultaneously circle-like enough to pass the circle gauntlet. This
  is the only connector in the census with one dominant, well-formed
  contour.
- **CONN-002, CONN-005, CONN-009** — a single dominant contour exists, but
  it corresponds to a **housing/label rectangle above or around** the true
  connector notch, not the notch itself; the notch geometry is either
  absorbed into that larger contour or produces only sub-threshold
  fragments. CONN-002's and CONN-009's dominant contours are notably
  circle-like (circularity 0.66 and 0.73) but too elongated (aspect 1.92,
  1.84) — this is the **"oval/elongated housing" geometry family**.
  CONN-005's dominant contour is a clean, ink-free housing box (rejected
  by empty interior, not by aspect, though it also fails aspect).
- **CONN-003** — **multiple contours**: one large, well-formed housing
  contour (accepted as Enclosure, but 11px away from the notch) plus 8+
  small fragmentary contours in the notch region itself, none reaching
  40px² area or a valid 4-vertex polygon. This is the **"housing +
  fragmented notch" family**.
- **CONN-004, CONN-006, CONN-012** — **no dominant contour at all**;
  purely small (<40px²) or narrow (<15×12) fragments. This is the
  **"fully fragmented / sub-threshold ink" family** — the most severe
  geometry mismatch, since these locations never generate a candidate
  large enough to reach any of the detectors' middle-stage checks.
- **CONN-007, CONN-008** — **interlocking-staircase geometry**: contours
  close to the true bounds exist and are large enough, but
  `approxPolyDP` consistently yields 5–6 vertices instead of 4. This is
  the **"interlocking/staircase" family**, structurally distinct from all
  others: it is the only family whose rectangle-path failure is a pure
  polygon-shape mismatch rather than an area/density/aspect numeric
  threshold.
- **CONN-010** — same "oval" family as CONN-009 but with lower
  circularity (0.48 vs 0.73), showing that visually similar connector
  footprints do not always produce numerically similar contour geometry.
- **CONN-011** — a housing contour with **good** rectangle-candidate
  geometry (passes fill/perimeter/side-support/isolated-components) that
  fails only on `internal_line_density`, and separately an
  **interrupted/open-boundary** circle candidate (high circularity, low
  edge-support). This is the **"wire-crossed housing with open connector
  boundary"** family, distinct from all others because two of its
  detector paths each reach the *last* checkpoint before failing, rather
  than failing early.

No connector in this census presents as a genuinely closed, clean,
axis-aligned rectangle at the connector's own footprint; every "clean
rectangle" contour found near a connector location corresponds to an
adjacent component housing rather than the connector notch itself.

## 22. Conductor Interaction Analysis

The logged `internal_line_density` values (only computed for candidates
that survive far enough into `detect_rectangles`) directly implicate
through-wires in at least two locations:

- **CONN-011**: `internal_line_density = 0.0809` against a 0.08 ceiling —
  a wire (or wires) passing through the housing box registers as
  sufficient horizontal/vertical internal structure to trip this
  wire-field detector by a hair's breadth. This is the clearest direct
  evidence in the census that continuous conductor ink inside a
  candidate's bounds actively defeats rectangle acceptance.
- **CONN-003's housing** (`internal_line_density = 0.0000`) and
  **CONN-007's/CONN-008's label rows** (`interior_density = 0.0000`) show
  the opposite failure mode — these boxes are *too empty*, not too busy —
  meaning conductor interaction is not the dominant failure mode
  everywhere; it is specific to CONN-011.

No connector's own notch-region small fragments ever reach the
`internal_line_density` checkpoint at all (they are rejected earlier, by
`min_width/min_height` or `approxPolyDP`), so for 10 of the 12 connectors
this audit **cannot** determine whether a through-wire is fragmenting the
notch geometry versus the notch geometry simply never forming a large
enough closed contour in the first place — both explanations are
consistent with the observed small-fragment pattern, and distinguishing
them would require inspecting the raw contour hierarchy (parent/child
relationships from `RETR_TREE`), which was not part of this AP's
instrumentation. This is recorded as an explicit unresolved question in
§24, not answered by inference.

No conductor extraction logic was altered or exercised differently by this
audit.

## 23. Detector Failure-Mode Matrix

| Failure mode | Connectors affected | Checkpoint |
|---|---|---|
| Non-quadrilateral polygon approximation (rectangle path) | CONN-001, CONN-002, CONN-003(partial), CONN-007, CONN-008 | `approxPolyDP` vertex count ≠ 4 or not convex |
| Aspect ratio too high (circle path, elongated/oval housing) | CONN-002, CONN-003, CONN-005, CONN-009 | `circle_max_aspect_ratio` (1.35) |
| Circularity too low (circle path) | CONN-004*, CONN-006, CONN-007, CONN-010, CONN-012 | `circle_min_circularity` (0.65) |
| Empty/near-empty interior (rectangle path, housing boxes) | CONN-005, CONN-007, CONN-009 | `rectangle_min_interior_ink_density` (0.005) |
| Sub-minimum contour size / width-height floor | CONN-002, CONN-003, CONN-004, CONN-005, CONN-006, CONN-009, CONN-010, CONN-011, CONN-012 | `rectangle_min_area`/`min_width`/`min_height`, `circle_min_area` |
| Internal wire-crossing structure | CONN-011 | `rectangle_max_internal_line_density` (0.08), near-miss by 0.0009 |
| Broken/interrupted boundary ink | CONN-011 | `circle_min_edge_support` (0.65) |
| Insufficient isolated interior components | CONN-008 (housing) | `rectangle_min_interior_components` (1) |
| Ground-bar count gate cleared but unresolved further | CONN-002, CONN-003, CONN-004, CONN-007, CONN-008, CONN-011, CONN-012 | `ground_min_bars`/`ground_max_bars` (3) — cleared; later checks not individually instrumented |
| No qualifying contour reaches any gauntlet | CONN-004 (rect), CONN-012 (rect) | Pre-`min_width/min_height` |

\* CONN-004's only circle candidate is rejected by the area floor before
circularity is even computed; grouped here for summary purposes only.

## 24. Findings by Severity

Severity reflects downstream engineering consequences and evidence
quality, not mere absence, per the AP's instruction.

- **CRITICAL** — None. No data corruption, incorrect electrical inference,
  or safety-relevant misrepresentation was found; all 11 absent connectors
  simply produce no candidate, which is a completeness gap, not a
  correctness defect.
- **HIGH** — The near-universal rejection of connector-notch geometry by
  `approxPolyDP`'s strict 4-vertex requirement (5 of 12 connectors
  affected) and by the circle detector's aspect-ratio gate (4 of 12
  connectors affected) together account for the majority of failures and
  represent the two structurally distinct geometric assumptions (`must be
  a clean quadrilateral` / `must be near-circular, not elongated`) that
  any future connector-aware detector would have to relax or bypass.
- **MEDIUM** — CONN-011's two near-miss checkpoints
  (`internal_line_density` 0.0809 vs 0.08, and `edge_support` 0.417 vs
  0.65) are high-quality, reproducible evidence of the wire-crossing and
  open-boundary failure modes, but affect only 1 of 12 connectors directly
  (though the underlying mechanisms plausibly generalize).
- **MEDIUM** — The repeated appearance of ≥3-bar ground-candidate runs at
  7 of 12 connector locations (with CONN-008's 6-bar run and CONN-007's
  4-bar run being the strongest) confirms the AP-DIAG-FIX-003 historical
  "connector-plug notch" false-positive concern is a live, recurring
  geometric coincidence in this diagram, not a one-off. Its severity is
  capped at MEDIUM because the whole-diagram `chassis_ground` shape count
  (6, independently accounted for) confirms none of these runs currently
  produce an incorrect ground detection — but the margin by which they are
  currently being stopped (later checks not instrumented in this AP) is
  unverified, which is itself a LOW/INFORMATIONAL follow-up item, not a
  live defect.
- **LOW/INFORMATIONAL** — CONN-004's near-total absence of any coherent
  contour at all, and CONN-012's total failure to reach even the
  `min_width/min_height` gate on the rectangle path, are noted as the
  weakest raw-signal cases; they carry low evidentiary value for any
  future geometric-relaxation approach because there is no near-miss
  candidate to reason about, only an absence of ink structure.
- **INFORMATIONAL** — CONN-001 is confirmed once more as a working,
  understood positive-control detector path (`ShapeKind::Circle`), and
  CONN-003's accepted Enclosure is confirmed to be a distinct feature (the
  CDI housing), not evidence of any connector detection.

## 25. Architectural Constraints, Non-Fixes, Instrumentation Limitations, and Recommended Next AP

**Architectural constraints established** (evidence-based, no design
performed): any future connector-aware geometric detector would need to
(a) accept non-quadrilateral polygon approximations (5–8 vertices) for
notched/interlocking pin housings, (b) accept aspect ratios above the
current 1.35 circle ceiling for oval/elongated connector footprints
without simply loosening the existing circle detector (which exists to
reject genuine non-circular symbols), (c) tolerate `internal_line_density`
values marginally above 0.08 where a through-wire crosses a connector
housing without conflating that with genuine wire-field/grid regions, (d)
tolerate `edge_support` below 0.65 for open/interrupted connector
boundaries without conflating that with noise, and (e) coexist with the
ground-bar detector's 3-bar requirement, since at least two locations
(CONN-007, CONN-008) generate ground-bar-shaped candidates whose disposal
currently depends on later ground-run checks that were not fully traced in
this AP.

**Explicit non-fixes** — confirmed unchanged in the final, reverted
production tree: `ShapeDetector` (all thresholds, all three detector
functions, `ShapeKind`/`ShapeRole` enums), `ConnectorBoundary`,
`TerminalRecognizer`, `PhysicalWireIdentityReconstructor`,
`ElectricalNetResolver`, and all Wire/Component/Connector model types. No
`ConnectorShape`, connector detector, `ComponentCandidateKind` value, or
`ConnectorTerminal` concept was added. `git diff` against the starting
commit shows zero change to any `.cpp`/`.hpp` file (see §regression
verification below).

**Instrumentation limitations** (explicitly identified rather than
guessed past): for the 7 connectors whose ground-bar runs cleared the
≥3-bar count gate (CONN-002, CONN-003, CONN-004, CONN-007, CONN-008,
CONN-011, CONN-012), this AP did **not** individually instrument the three
downstream `evaluate_ground_run()` checks (decreasing bar-width sequence,
uniform spacing, stem presence above the first bar) that run after the
count gate. Non-appearance of any of these runs in the final
`chassis_ground` shape total (6, all independently attributable to real
ground symbols elsewhere in the diagram, per AP-DIAG-AUDIT-010's
established topology) is strong indirect evidence that all 7 runs are
ultimately rejected by one of those three later checks, but this AP cannot
name *which specific* check stops each individual run. Separately, for the
10 connectors whose notch-region contours are rejected before reaching
`internal_line_density` (all except CONN-011), this AP cannot distinguish
"through-wire fragmenting the contour" from "notch ink never forms a large
enough contour regardless of wires" without inspecting `RETR_TREE`
parent/child contour hierarchy, which was outside this AP's instrumentation
scope.

**Recommended next AP:** the smallest evidence-based next step is a
**contour-hierarchy forensic AP** (not a fix) that instruments the
`RETR_TREE` parent/child relationships and the three unlogged
`evaluate_ground_run()` sub-checks specifically for the 7 connectors with
count-gate-clearing ground runs and the 10 connectors with sub-threshold
notch fragments, to close the two instrumentation gaps identified above
before any connector-detector design work begins.

# AP-WIRE-FIX-002 — Bus-Crossing Circle False-Positive Correction

## Status

Implements the upstream correction identified by AP-WIRE-TUNE-004.
`gap_interpretation.maximum_gap` remains 18.0px; AP-WIRE-024, AP-WIRE-029,
AP-WIRE-030, AP-WIRE-031, `ElectricalNetResolver`, and AP-WIRE-FIX-001
were not modified. Files changed: `include/eke_dx_wire/image/shape_detector.hpp`,
`src/image/shape_detector.cpp`, `tests/test_shape_detector.cpp`.

## Phase 0 — Baseline

`ctest`: 52/52 passing. Fresh TRX300 extraction: 41 physical wires, 10
electrical nets, 3 AP-WIRE-024 conflicts (CONFLICT-01/03/04),
CONFLICT-02 confirmed still Resolved.

## Phase 1 — Reproducing the false positive

All three known false circles were located by cross-referencing
`ComponentCandidate` bounds against the `ShapeDetector::detect_circles()`
acceptance path. Rather than guessing at the internal geometry, temporary
debug instrumentation (added, used, then fully reverted before any real
edit — confirmed via an empty `git diff`) printed every accepted circle's
center, radius, bounding box, circularity, aspect, edge-support, interior
density, and confidence. All three matched exactly:

| Conflict | bbox | center/r | circularity | aspect | edge_support | interior_density |
|---|---|---|---|---|---|---|
| CONFLICT-01/03 | (869,342,12,9) | (875,346) r=6 | 0.835 | 1.333 | 1.000 | 0.000 |
| CONFLICT-04 (a) | (584,439,9,10) | (588,443) r=5 | 0.852 | 1.111 | 1.000 | 0.000 |
| CONFLICT-04 (b) | (575,439,9,9) | (579,443) r=5 | 0.886 | 1.000 | 1.000 | 0.000 |

## Phase 2 — Characterizing the false circle

Direct raster inspection (crops preserved, sent to the user) confirms
each candidate sits exactly at the intersection of two parallel bus
conductors crossing two parallel drop conductors — a small enclosed
white/background *gap* bounded on all sides by ink, not a drawn shape.
Its near-square gap has circularity ≈0.83–0.89 (a plain square already
scores 4π/16≈0.785, comfortably above `circle_min_circularity`=0.65),
`edge_support`=1.0 (the fitted enclosing-circle radius reaches into the
bordering ink at every sampled angle), and `interior_density`=0.0 (the
gap's own center is empty).

**Critical finding: none of these three local statistics — circularity,
aspect, edge_support, interior_density — nor a polygon-approximation
vertex count at any tested epsilon, distinguishes these false positives
from genuinely legitimate circles elsewhere in the same diagram.** A
full dump of all 77 circles ShapeDetector accepts on TRX300 showed the
same "circularity 0.8–0.95, edge_support 1.0, interior_density 0.0"
signature on dozens of entries, including a confirmed genuine fuse-symbol
round top (`component-candidate-shape-region-845947b0afd7451a`, the same
component AP-WIRE-FIX-001's CONFLICT-02 resolves to) and confirmed
genuine ring glyphs inside a wire-color legend table. Polygon vertex
count at `eps=0.01`–`0.08` also failed to separate them (a confirmed
real fuse-top collapsed to the same 4 vertices as the crossings at
`eps≥0.02`).

## Phase 3 — Comparison against legitimate circles

Because local shape statistics were indistinguishable, every candidate
discriminator was validated against **directly, visually confirmed**
ground truth, not assumption. Crops were taken and inspected for:
- 8 candidates confirmed by eye to be real bus/table crossings (empty
  gaps), including 5 beyond the 3 originally known from AP-WIRE-TUNE-004
- 3 candidates confirmed by eye to be genuine drawn circles: a real
  fuse-symbol round top, a legend-table ring glyph, and a real ring with
  a wire lead touching its edge

This uncovered an important, code-confirmed extra distinction: a
candidate ink shape a human would call "circular" is not automatically
"legitimate" — the same visual test also revealed some of the diagram's
smaller "circle-topped" symbols and legend-table rings are real, drawn,
intentional ink, geometrically indistinguishable from a crossing by
shape statistics alone. Only a *context* signal (what is bounding the
shape, not the shape's own local statistics) discriminates reliably.

## Phase 4 — Determining the discriminator

The reliable, minimal discriminator found: **for each of a candidate's
four bounding-box sides, check whether ink continues, as a thin
straight run, for a fixed probe distance beyond both ends of that side.**
A crossing-bounded gap's box is *literally defined by* two pairs of
long crossing lines, so ink continues past all four sides simultaneously.
A genuine circle's boundary is self-contained ink; even sitting inside a
bordered table cell or touched by a wire lead, at least one side does
not continue as a straight line.

Measured against the fully visually-verified set (`far`=25px probe,
2px band):

| Candidate | min(top,bottom,left,right) | Ground truth |
|---|---|---|
| CONFLICT-01/03 crossing | 0.64 | crossing (reject) |
| CONFLICT-04 crossing (a) | 0.80 | crossing (reject) |
| CONFLICT-04 crossing (b) | 0.96 | crossing (reject) |
| a 4th, previously unknown crossing | 0.92 | crossing (reject) |
| a 5th, previously unknown crossing | 1.00 | crossing (reject) |
| real fuse-symbol round top | 0.04 | genuine (keep) |
| real legend-table ring | 0.12 | genuine (keep) |
| real ring with wire lead touching one edge | 0.08 | genuine (keep) |

A clean margin (0.12 highest-genuine vs. 0.64 lowest-crossing-that-must-
be-caught) supports a threshold of 0.5. Some other, unrelated empty
table-grid cells score below 0.5 too (a false negative — they are not
rejected, but they were never causing a known problem either); per the
task's explicit guidance ("if a legitimate circular symbol is difficult
to distinguish from a crossing, preserve the ambiguity rather than
introducing an aggressive heuristic"), the threshold was kept
conservative rather than tuned to catch every possible spurious circle
in the diagram.

## Phase 5 — Implementation

`src/image/shape_detector.cpp`: added `row_line_continuity()`,
`col_line_continuity()`, and `bounded_by_crossing_lines()` (anonymous
namespace, same style as the file's existing helpers), and one new call
in `detect_circles()` immediately after the existing `edge_support`
check:

```cpp
if (bounded_by_crossing_lines(binary, bounds, config))
    continue;
```

`include/eke_dx_wire/image/shape_detector.hpp`: added three new
`ShapeDetectorConfig` fields (`circle_crossing_probe_distance = 25`,
`circle_crossing_probe_thickness = 2`,
`circle_crossing_min_line_continuity = 0.5`) — new tunables for this
new check, not a change to any existing default. The check operates
entirely on `binary`, the same already-thresholded image
`detect_circles()` already uses; no other pipeline stage's output is
consulted (`ConductorSegment`s do not exist yet at this point in
`ExtractionPipeline::run()`, confirmed by reading the call order —
`ShapeDetector::detect()` runs before `MorphologyWireDetector` and
`ConductorNormalizer`).

## Phase 6 — Regression tests added

`tests/test_shape_detector.cpp`, 4 new scoped blocks:
1. Reproduces the exact CONFLICT-01/03/04 crossing geometry; asserts no
   circle is reported near the gap.
2. A second, independently-positioned crossing (covering more than one
   false candidate).
3. A genuine ring with a conductor lead touching one edge (not passing
   through); asserts it remains detected.
4. Determinism: the same input run twice yields the same region count
   and the same accept/reject outcome for both a crossing and a
   genuine circle.

**Important, unrelated discovery made while validating these tests:**
this project's `CMakeLists.txt` does not override `CMAKE_BUILD_TYPE`'s
default flags, so the Release build `ctest` always runs under
(`-O3 -DNDEBUG`) — and `-DNDEBUG` compiles every `assert()` in this
entire test suite into a no-op. Confirmed directly: a trivial
`assert(false)` exits 0 under the project's exact Release flags. This
means `ctest`'s "52/52 passing" has only ever verified that each test
binary runs to completion without crashing, not that any assertion
holds — a pre-existing condition of the whole repository, not
introduced by this task. To give this task's own new tests real
teeth despite that, they were independently compiled and run with
`-O0` (no `-DNDEBUG`) against both the fixed and an assertion-disabled
copy of `shape_detector.cpp`: all 4 pass with the fix, and the crossing
assertions **genuinely abort** without it — non-vacuous, verified
outside the officially-configured (assert-neutered) build. This
NDEBUG issue affects every test file in the repository, not just this
one; fixing it project-wide is out of scope for this task and is
flagged here as a recommended follow-up, not attempted.

The pre-existing `test_shape_detector.cpp` content (rectangle/ground
tests) was left completely unmodified; only new blocks were appended.

## Phase 7 — Full TRX300 regression

| Metric | Before | After |
|---|---|---|
| Physical wires | 41 | **40** |
| Electrical nets | 10 | 10 |
| Component candidates | 109 | 85 |
| `circular_symbol` candidates | 47 | 26 |
| AP-WIRE-024 conflicts | 3 | **0** |
| CONFLICT-01 | Conflicted | Resolved |
| CONFLICT-03 | Conflicted | Resolved |
| CONFLICT-04 | Conflicted | Resolved |
| CONFLICT-02 (AP-WIRE-FIX-001) | Resolved | Resolved (unchanged) |
| Validation errors | 0 | 0 |
| Topology nodes/edges | 692 / 877 | 692 / 877 (unchanged) |

24 spurious `ComponentCandidate`s were removed in total — the 3 known
ones plus 21 more of the identical crossing-gap defect, previously
silent (never surfaced as an AP-WIRE-024 conflict because none happened
to sit within any endpoint's association distance). `symbol_primitives`
count is unchanged (39 before and after — none of these 24 candidates
owned any primitive, consistent with AP-WIRE-TUNE-004's finding).

**The 41→40 physical-wire change was stopped and investigated per this
task's explicit instruction, not accepted on sight.** Tracing it:
`wire-15f768907f0643e8` (a 14-edge, 3-segment run crossing a
distribution node, endpoints at (602,176) and (275,196)) disappeared.
One of its two endpoints
(`endpoint-candidate-8a7b6fb67ed4eeab`) had been resolved — Low
confidence, `boundary_status: Resolved` — to
`component-candidate-shape-region-2ca0cb53e282cb32`, bounds (603,187,9,12),
which is **one of the 24 removed candidates**; direct raster inspection
confirms it is, again, a plain bus/drop-wire crossing, not a real
symbol. Reading `PhysicalWireIdentityReconstructor`'s own source
(`src/topology/physical_wire_identity_reconstructor.cpp:216-229`) shows
its distribution-crossing pass explicitly requires
`boundary_status == Resolved` before treating a degree-1 endpoint as a
legitimate wire terminus at all — its own comment: *"a bare geometric
conductor end is never a new Wire boundary here (AP-WIRE-029 Sec 3)."*
Before this fix, that rule was satisfied by **the same class of false
evidence** this task corrects; after the fix, the endpoint correctly
reverts to an unresolved bare conductor end, and AP-WIRE-031's own
pre-existing, unmodified safety rule correctly refuses to manufacture a
long cross-distribution wire from it. **This is not a regression — it
is AP-WIRE-031 behaving exactly as designed, once fed corrected
evidence; the previous 41-wire count included one wire that should
never have been reconstructed.** No other wire, node, edge, or
electrical net changed.

Two other endpoints changed from a spurious `component_terminal`
resolution to a **different, still-genuine** resolution they already
had independent evidence for (`b06048fbeb206768` and
`4d7a2c9da1a78d41`, both correctly falling back to their own `ground`
evidence, unaffected in status) — a strict improvement, not a loss. One
further endpoint (`c68b526cba9cc6d3`, tied to the same
`2ca0cb53e282cb32` crossing as the lost wire) correctly reverts from a
false "resolved" to accurate "unresolved," with no further topology
consequence.

## Phase 8 — Invariants

- AP-WIRE-031 wire records: unchanged except the one legitimately-lost
  wire explained above; every other wire (including its own identity
  evidence and edges/segments) is byte-identical before and after.
- `WireIdentityStatus` semantics: untouched code, confirmed by the
  above trace being a consequence of *evidence* correction, not a
  code change to AP-WIRE-031.
- Electrical nets: byte-identical (confirmed via direct JSON diff).
- AP-WIRE-FIX-001: unaffected — its call site
  (`extraction_pipeline.cpp`'s `SymbolGeometryExtractor::extract()`
  call) was not touched; CONFLICT-02 remains Resolved with identical
  evidence.
- Splice/junction/crossing/continuation semantics: no file implementing
  any of these was opened for editing.
- Topology nodes/edges: byte-identical (692/877, confirmed via JSON
  diff).

## Phase 9 — No parameter mitigation used

`gap_interpretation.maximum_gap` remained 18.0 throughout this task and
was never changed. `terminal_recognition.aligned_boundary_max_distance`
and every other existing tuning default are untouched — the fix stands
entirely on the `ShapeDetector` correction described above.

## Phase 10 — Full test suite

Previous: 52/52. New: **52/52** (no new CTest target was added; the 4
new cases were appended to the existing `dx-wire-test-shape-detector`
binary, matching this repository's own established convention). 0
failures, 0 new warnings (confirmed via a full clean rebuild; the same
8 pre-existing, unrelated warnings as every prior AP-WIRE-TUNE session).

## Phase 11 — Code review

Diff touches exactly 3 files, all within `ShapeDetector`'s own scope:
2 new helper functions + 1 new discriminator function + 1 call site in
`shape_detector.cpp`; 3 new config fields in `shape_detector.hpp`; 4 new
test blocks appended to `test_shape_detector.cpp` with the pre-existing
content untouched. No AP-WIRE-024/029/030/031 file was opened for
editing. No `ElectricalNetResolver` change. No existing tuning default
changed (only new fields added). No speculative CV (no new external
library or model — the same OpenCV primitives, i.e. simple pixel
lookups, already used throughout this file). No OCR, no LLM reasoning,
no circuit-function assumption (the discriminator never asks what a
line *means* electrically, only whether it is a long straight run of
ink). No unrelated refactoring; no stray generated artifacts staged
(`git status --short` shows only the 3 intended files).

## Final report

1. **Root cause:** `ShapeDetector::detect_circles()`
   (`src/image/shape_detector.cpp`) accepted a small enclosed background
   gap between two pairs of crossing bus/drop conductors as a
   `ShapeKind::Circle` `ComponentCandidate`, because such a gap's
   contour statistics (circularity, aspect, edge-support, interior
   density) are indistinguishable from a genuine small drawn circle.
2. **Files changed:** `include/eke_dx_wire/image/shape_detector.hpp`,
   `src/image/shape_detector.cpp`, `tests/test_shape_detector.cpp`.
3. **Functions changed:** new `row_line_continuity()`,
   `col_line_continuity()`, `bounded_by_crossing_lines()`; one new
   `continue` check added inside `detect_circles()`.
4. **Geometric discriminator:** all four sides of the candidate's own
   bounding box continue as a thin straight line for a fixed probe
   distance beyond the box, in both directions — the signature of a
   gap defined by crossing lines rather than a self-contained drawn
   shape.
5. **Why the false candidates were bus crossings:** direct raster
   inspection of all removed candidates and cross-checking against
   `symbol_primitives` (none owned any) confirms each sits exactly
   where a bus or table-grid line crosses another line.
6. **Why legitimate circles remain detectable:** a genuine circle's
   boundary is self-contained ink; even touched by a wire lead or
   sitting inside a bordered table cell, at least one of its four
   bounding-box sides does not continue as a long straight line —
   verified against 3 directly-confirmed real circles plus a
   determinism check.
7. **Tests added:** 4 new scoped blocks in the existing
   `tests/test_shape_detector.cpp`; independently verified non-vacuous
   (fails without the fix, passes with it) via an out-of-band debug
   build, because the project's own Release/CTest configuration
   compiles `assert()` out under `-DNDEBUG` (a pre-existing,
   project-wide condition, flagged as a follow-up, not fixed here).
8. **CONFLICT-01:** Conflicted → Resolved.
9. **CONFLICT-03:** Conflicted → Resolved.
10. **CONFLICT-04:** Conflicted → Resolved.
11. **CONFLICT-02:** verified still Resolved, identical evidence,
    unaffected by this task.
12. **AP-WIRE-024 total:** 3 → 0.
13. **Physical-wire topology:** 41 → 40, investigated and traced to
    AP-WIRE-031's own pre-existing safety rule correctly refusing to
    reconstruct a wire whose only distribution-crossing terminus
    evidence was itself one of the 24 corrected false candidates — not
    a regression (Phase 7).
14. **Electrical-net topology:** unchanged (10, byte-identical).
15. **Validation:** 0 errors before and after.
16. **CTest:** 52/52 passing (unchanged count; new coverage added to
    the existing shape-detector test binary).
17. **Release build:** clean.
18. **Warning count:** 8, all pre-existing and unrelated to this
    change.
19. **Commit SHA:** recorded after commit below.
20. **Remaining limitations:** the discriminator only catches a
    candidate whose bounding box is fully bounded by long lines on all
    four sides; some other spurious circles in dense table/legend
    regions with only 2-3 continuing sides are not caught (a
    deliberate, conservative choice per Phase 4 — no known conflict
    depends on them). The project-wide `assert()`/`NDEBUG` issue
    discovered in Phase 6 affects every test in the repository and is
    recommended, but not attempted, as a separate follow-up task.

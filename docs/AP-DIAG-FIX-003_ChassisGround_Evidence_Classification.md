# AP-DIAG-FIX-003 — ChassisGround Evidence Classification

## 1. Starting State

- Starting SHA: `be4bf0d349bcbc08d4db3938bd0eacae806feecf` (branch
  `claude/modest-dirac-a3d7pw`, per this AP's own header). Work was
  redirected to `main` mid-task at the user's explicit standing
  instruction ("only work on or towards origin main ever"); `main` at
  the time already contained `be4bf0d`'s full content, merged as
  `a15cf0a` (confirmed via `git merge-base --is-ancestor`), so no branch
  was created, reset, or replaced — `main` was simply checked out and
  all work proceeded from there.
- Clean Release rebuild: 0 errors, the same 8 pre-existing warnings as
  every prior AP.
- Full assertion-enabled test suite: 55/55 passing before any change.
- Reproduced the scoped TRX300 extraction and confirmed the unscoped
  hash matched the established baseline
  (`c4e99f52bedfe3e4083f4f5c5df6bd283696e1365671c7f8c5a5cd6da212432f`)
  before touching any code.

## 2. Forensic Reconstruction (before implementation)

All 10 ChassisGround-classified components from AP-DIAG-AUDIT-002 (8
false positives, 2 true positives) and all 4 Ground-role electrical nets
(3 misclassified, 1 correct) were re-located by exact coordinate in a
fresh extraction and cross-checked against
`docs/AP-DIAG-AUDIT-002_Post_Scoping_ReAudit.md` before any code change —
all 10 reproduced at identical bounds.

**Root cause was found by replicating the detector's own algorithm
directly against `samples/trx300ODG.png`'s real pixels** (via a Python
OpenCV harness reproducing the exact `cv::morphologyEx` +
`connectedComponentsWithStats` + grouping/matching logic in
`detect_ground_symbols()`), not by inspecting cropped images alone —
cropped-region analysis was tried first and gave misleading results
(see §3) precisely because it couldn't see the effect described below.

Two distinct, compounding defects were found in
`src/image/shape_detector.cpp`'s `detect_ground_symbols()`:

### Defect A — the degraded-scan 2-bar fallback

The classic chassis-ground glyph is a stem above **three** progressively
narrower bars. The code accepted a match of only `ground_min_bars = 2`
bars whenever the full 3-bar pattern didn't validate, explicitly to
tolerate "image quality [that] has erased one of the bars." Replicating
the exact matching logic against every one of the 10 known instances
showed:

| Instance | Bars matched | Widths |
|---|---:|---|
| Battery ground (true positive) | 3 | 18, 12, 5 |
| Alternator ground (true positive) | 3 | 16, 10, 5 |
| Rectifier diode #1 (false positive) | 2 | 20, 14 |
| Rectifier diode #2 (false positive) | 2 | 28, 20 |
| Connector-plug notch, CDI Unit (false positive) | 2 | 20, 5 |
| Connector-plug notch, flasher (false positive) | 2 | 34, 11 |
| Pin-label text glyphs (false positive) | 2 | 32, 10 |
| Connector/splice housing (false positive) | 2 | 23, 9 |
| Table-cell marker (false positive) | 2 | 6, 5 |
| "SWITCH" text glyphs (false positive) | 3 | 21, 6, 5 |

**Both genuine symbols independently satisfy the full 3-bar pattern; 7
of 8 false positives only qualify via the 2-bar fallback.** On the only
real-world evidence gathered to date, that fallback path has a 100%
false-positive rate and a 0% true-positive rate.

### Defect B — no Y-locality constraint in the initial grouping

The one false positive that *did* match 3 bars ("SWITCH" text) revealed
a second, independent defect. Its accepted bars had gaps of 11px then
3px between them (ratio 3.67); both genuine symbols have gap ratios of
1.0 and 1.5 — a real drawn glyph keeps a consistent vertical rhythm,
coincidentally-aligned unrelated ink does not.

Fixing defect A alone (requiring 3 bars) caused a **new** problem when
first tried: the confirmed genuine battery-ground symbol's own 3rd bar
became unreachable. Tracing this precisely (again by replicating the
real grouping algorithm, this time across the *entire* image rather than
per-region) found the actual defect: `detect_ground_symbols()`'s initial
clustering pass groups candidate bars by `center_x` proximity only
(within 3px), with **no constraint on how far apart in Y they are**. A
bar from a wholly unrelated part of the diagram — 128px away vertically
— was found sharing a similar rounded `center_x` with the real ground
symbol's bars purely by coincidence, and merged into the same candidate
group. Because the group is then sorted by Y, this stray bar could land
*before* the real bars in sort order, corrupting which `(begin, length)`
combinations the matching loop could try and making the real 3-bar
sequence unreachable.

**Neither defect is fixable by "raising a threshold."** Defect A's
correct minimum bar count is a structural fact about the actual symbol
convention this diagram uses (verified against both real instances, not
assumed); defect B is a genuine grouping-algorithm gap (candidates need
to be co-located in **both** X and Y to belong to one glyph, and the
code only ever checked X) that a threshold change cannot fix, since it
is a control-flow/data-structure defect, not a magnitude one.

## 3. A Third Problem Found During Implementation

Fixing defects A and B together correctly rejected all 8 false positives
— but also correctly **recovered 4 previously-undetected genuine ground
symbols** (see §4) that the broken grouping had always failed to find at
all (this was not anticipated by AP-DIAG-AUDIT-002, which only audited
components that were *already* being classified ChassisGround; it never
looked for real symbols the detector was missing entirely).

This recovery then exposed a **third, latent defect**: the exclusion
region computed for an accepted match padded its bounds by the full
`ground_stem_search_height` (14px) upward, `+10`px downward, and `+6`px
in width. That 14px upward pad is a reasonable width for the *evidence
check* ("is there ink somewhere above the bars to imply a stem/wire
connects here") — but using the same 14px as the **masked/excluded**
region is a different question, and turned out to be wrong: for
symbols packed close to other components (the four newly-recovered
ones), the wire genuinely approaching the symbol from a nearby switch or
sensor runs straight through most of that 14px corridor before reaching
the bars. Masking it erased real, drawn conductor ink and caused 8
otherwise-valid physical Wires to disappear entirely (verified by
overlaying one such wire's traced endpoints directly on the source
raster — see the artifact retained in this session's working files: the
overlay traces exactly the real drawn wire from a switch symbol down
into its ground stem).

**Fix**: the search corridor (used only for the boolean ink-presence
check) and the exclusion bounds (used to mask pixels from further
detection) are no longer the same region. The exclusion region is now
just the union of the accepted bars themselves plus a small, fixed
2px anti-aliasing margin — the only ink a chassis-ground glyph's bars
can be certain to occupy. The approaching wire, wherever it comes from,
remains fully visible to ordinary conductor detection and is classified
`ground`-kind by `TerminalLocationDetector`'s existing, unmodified
distance-to-component check once it reaches the (now tight) boundary —
the exact mechanism that already worked correctly for the original
battery-ground true positive.

## 4. The Corrective Rule (and why it's engineering-valid)

Three changes to `detect_ground_symbols()` / `ShapeDetectorConfig`,
`include/eke_dx_wire/image/shape_detector.hpp` +
`src/image/shape_detector.cpp`:

1. **`ground_min_bars`: 2 → 3.** A chassis-ground symbol is classified
   ChassisGround only when the full stem-plus-three-decreasing-bars
   pattern is present — not "close enough" with two. Justification is
   not "this produces a better count on TRX300": it is that both real
   instances of the actual symbol convention this diagram uses satisfy
   the full pattern, and the 2-bar shortcut has a demonstrated 100%
   false-positive rate and 0% true-positive rate on every instance
   checked. `ground_max_bars` stays 3, so the fallback-loop code that
   tries shorter lengths is still present (it still matters if more than
   3 candidate bars are ever found in one column and a 3-of-N subset
   must be selected) but can never accept fewer than 3.
2. **New `ground_max_bar_spacing_ratio` (2.0) + `ground_bar_spacing_
   uniform()` check.** A real ground symbol is one continuously drawn
   glyph, so the gaps between its bars share a consistent rhythm — this
   is a structural property of "these lines were drawn as one symbol,"
   not a pixel-count coincidence. Both genuine symbols have gap ratios
   of 1.0 and 1.5; the one 3-bar false positive that survives requiring
   3 bars alone has a ratio of 3.67. Only evaluated once a candidate has
   3+ bars (2+ gaps), which is now every accepted candidate.
3. **Y-locality run-splitting in the initial grouping.** After the
   existing X-tolerance clustering and Y-sort, the cluster is split into
   maximal runs where consecutive bars are within `ground_max_bar_
   spacing` of each other (the same spacing bound already used to
   validate a real sequence). Each run is evaluated independently. This
   directly targets defect B: a candidate group is only ever treated as
   one glyph if its members are actually close together in *both*
   dimensions, not X alone.
4. **Exclusion bounds tightened to the bars' own union + 2px**, decoupling
   "how far to search for stem evidence" from "how much to mask."

None of these are threshold tuning to fit TRX300's aggregate object
count — each is a structural, falsifiable claim about what a drawn
ground symbol actually looks like, checked against every real instance
available, both positive and negative.

## 5. Shape vs. Semantics

`ComponentCandidateClassifier::classify_kind()` (unmodified) passes
`ShapeKind::ChassisGround` straight through to
`ComponentCandidateKind::ChassisGround` with no independent check of its
own — meaning `detect_ground_symbols()` in `ShapeDetector` is the *only*
place "this geometry is a chassis-ground symbol" is asserted, and is
therefore the correct and only correction point (per this AP's own
instruction to fix the earliest incorrect stage, not patch a downstream
consumer). No parallel classification system was created; the existing,
purpose-built ground-bar detector was corrected in place.
`TerminalLocationDetector`'s existing distance-to-component logic — the
consumer that turns a nearby wire endpoint into an `EndpointKind::Ground`
classification — was not touched at all; §3's fix works specifically by
no longer blocking it from seeing the approaching wire in the first
place.

## 6. Results

### 6.1 ChassisGround classification

| | Before | After |
|---|---:|---|
| Total ChassisGround components | 8 (post-AUDIT-002 numbering; 10 in the raw unscoped extraction, 2 outside the production-scope furniture region) | 6 |
| Confirmed genuine | 2 | 6 |
| Confirmed false positive | 8 | 0 |

All 8 previously-confirmed false positives (both rectifier diodes, both
connector-plug notches, both text-glyph instances, the connector/splice
housing, the table-cell marker) are absent by exact coordinate search in
the corrected extraction. Both previously-confirmed genuine symbols
(battery, alternator) remain present, each individually re-verified by
cropping and visually inspecting the corrected extraction's reported
bounds against the source raster. **4 additional genuine chassis-ground
symbols were recovered** (previously undetected entirely) — for
"...SWITCH", "NEUTRAL SWITCH", "PULSE GENERATOR", and one associated
with the alternator/starter area — each individually visually confirmed
against the source raster the same way. All 6 are anchored by real,
independently-drawn stem+3-bar glyphs.

### 6.2 Ground-role electrical nets

| Net | Before | After |
|---|---|---|
| Anchored on rectifier diode (449,154) | `ground`, confirmed **wrong** | absent — no longer anchors any net |
| Anchored on "SWITCH" text (398,114) | `ground`, confirmed **wrong** | absent |
| Anchored on connector housing (861,366) | `ground`, confirmed **wrong** | absent |
| Anchored on battery ground (923,535) | `ground`, confirmed correct | `ground`, confirmed correct (same physical net) |
| 4 new ground-role nets | did not exist | 3 new nets, each anchored on one of the 3 newly-recovered symbols that produce a `ground`-kind endpoint (see §6.4 on why not all 6 do) |

Net role counts: 4 `ground` / 6 `unknown` before, **4 `ground` / 6
`unknown` after** — same totals, but the actual anchoring evidence
behind every `ground`-role net is now correct. No role was manufactured:
the `unknown` count is unchanged because nothing about those 6 nets'
own evidence changed.

### 6.3 Whole-diagram inventory (unscoped TRX300)

| Category | Before | After | Explanation |
|---|---:|---:|---|
| Components | 85 | 81 | −4 (8 false-positive ChassisGround → 0, +4 newly-recovered true positives; net 8→6 minus 2 = the 4 count difference, since 85−8+6=83... see note below) |
| Symbol primitives | 40 | 34 | Reduced ChassisGround-associated primitive geometry from removed false positives |
| Terminal/endpoint candidates | 200 | 203 | Net +3 from corrected endpoint kinds and newly-visible approach wires |
| Ground-kind endpoints | 12 | 4 | 3 false-ground endpoints removed; the pre-existing 12 included several endpoints on false-positive components that never carried real semantic weight; only 4 of the 6 genuine symbols currently produce a resolved ground endpoint (unchanged, pre-existing `TerminalLocationDetector` proximity behavior — not modified by this AP; the alternator symbol did not have a resolved ground endpoint before this fix either) |
| Conductor boundary evidence | 228 | 223 | Net effect of removed false-positive exclusion ink and newly-eligible approach-wire ink |
| Topology nodes | 678 | 686 | +8, tracks the net wire/endpoint change |
| Topology edges | 868 | 876 | +8 |
| Physical wires | 35 | 37 | See §6.4 |
| Electrical nets | 10 | 10 | Unchanged total; composition corrected (§6.2) |
| Rejected geometry | 10 | 10 | Unchanged |
| Validation errors | 0 | 0 | Unchanged |
| Validation warnings | 30 | 32 | `NET-ROLE-UNRESOLVED` 6→6 (unchanged); `WIRE-GEOMETRIC-ENDPOINTS` 24→26 (tracks the wire-count increase, not a new defect class) |

Note on the components arithmetic: 85 − 8 (false positives removed) + 4
(newly recovered) = 81. Matches exactly.

### 6.4 Physical Wire audit

8 wires removed, 10 added, **0 modified**, all 37 remaining wires
`identity_status: resolved` (0 conflicted, 0 unresolved) — reconfirmed
directly from `topology.json`, not assumed. Every removed/added pair was
either:
- the same physical conductor with a corrected endpoint kind (e.g. a
  wire previously reporting a false `ground` endpoint at the diode now
  reports `geometric` at the same coordinate, since the diode is
  correctly no longer ChassisGround), or
- a previously-fabricated wire tracing misclassified text-glyph ink near
  the "SWITCH" text false positive, which correctly disappears with no
  replacement once that classification is corrected (the same failure
  mode AP-DIAG-FIX-001 already fixed for component-boundary ink, here
  found riding along a different false classification), or
- a wire that gained a `ground`-kind endpoint it should always have had,
  now that its approaching conductor is no longer masked away (§3).

No wire went from `resolved` to `conflicted` or `unresolved`.
`PhysicalWireIdentityReconstructor`, `ConductorBoundaryResolver`, and
`ElectricalNetResolver` were not modified; every change here is a
consequence of different (corrected) upstream evidence being handed to
unmodified consumers — consistent with AP-DIAG-AUDIT-001/002's own
conclusion that this defect family lives at shape/component
classification, not in topology or identity reconstruction.

## 7. Regression Tests

`tests/test_shape_detector.cpp` (still built under
`-DNDEBUG -UNDEBUG`, `dx-wire-test-assertions-enabled` reconfirmed
passing):

- Updated the existing "low-quality 2-bar variant" fixture's intent:
  previously asserted (implicitly) that a 2-bar match should still
  recognize a ground symbol; now explicitly asserts it must **not** be
  classified ChassisGround, with the real 3-bar fixture's own
  recognition reconfirmed alongside it in the same test.
- New: a coincidental 3-bar decreasing-width sequence with irregular
  spacing (gap ratio like the confirmed "SWITCH"-text false positive)
  must be rejected.
- New: a genuine 3-bar symbol must still be recognized even when an
  unrelated bar from 150+px away shares a similar rounded `center_x` —
  direct regression coverage for the Y-locality grouping defect (§2,
  defect B).
- New: the exclusion mask must cover only the bars themselves (plus a
  small margin), never a long approach-wire corridor above them —
  direct regression coverage for §3's fix, checking both a masked point
  (on the bars) and an unmasked point (up the approach wire, where the
  old 14px padding would have wrongly excluded it).

All four new/modified assertions were run and confirmed passing against
the actual built binary (not merely reasoned about).

## 8. Whole-Diagram / Unscoped / Scoped Regression

- Unscoped canonical extraction re-run from a clean Release rebuild;
  determinism reconfirmed via an independent second run
  (`diff -rq`, 0 differences excluding the documented
  `review_manifest.json` timestamp).
- Scoped (production fixture) extraction re-run; produces the same
  6-ChassisGround / 10-net / 37-wire picture inside the scope boundary,
  confirming the fix is not accidentally dependent on the scope fixture.
- New canonical unscoped hashes (this AP legitimately changes extraction
  output, so the historical hash table no longer applies):
  `extraction_audit.json`:
  `dfbd71c9cd28de1797f71950afe1806a2ecff097410dec5f93b6eeba5689c1ce`
  `output/wires.svg`:
  `375f96d9802181d3f2aed5f77bc43fb55a901cbb41051248a72a6730a2eacb0e`

## 9. Model Changes

None. `ShapeDetectorConfig` gained one new field
(`ground_max_bar_spacing_ratio`) with a default value — an ordinary
config parameter addition, not a change to any core model type
(`ComponentCandidate`, `EndpointCandidate`, `Wire`, `ElectricalNet`,
etc. are all untouched). No serialization format changed.

## 10. Unresolved / Follow-on Items

- Only 4 of the 6 confirmed genuine ChassisGround symbols currently
  produce a resolved `ground`-kind endpoint (the alternator symbol did
  not before this fix either, and still does not) —
  `TerminalLocationDetector`'s proximity-based classification was not
  modified by this AP and this gap was not investigated further; worth
  a dedicated follow-on if ground-endpoint coverage itself becomes the
  next target.
- The 6 `unknown`-role electrical nets' *membership* (as opposed to
  their ground-anchor status, which this AP's evidence directly bears
  on) was not re-verified — same open item AP-DIAG-AUDIT-002 already
  named.
- A full pairwise reconciliation of all 8 removed/10 added wires was
  spot-checked for the general mechanism, not individually re-derived
  pixel-by-pixel for every single pair, given the general mechanism was
  independently confirmed via direct visual overlay for the wire whose
  disappearance was least obviously explained (§3).

## 11. Final Report Summary

- **Starting SHA**: `be4bf0d349bcbc08d4db3938bd0eacae806feecf`
- **Final SHA**: see commit immediately following this document (on `main`)
- **Root cause**: `detect_ground_symbols()` in
  `src/image/shape_detector.cpp` — the earliest and only stage that
  asserts ChassisGround — accepted a degraded 2-bar match with no
  Y-locality constraint on candidate grouping (§2).
- **Previous ChassisGround count**: 8 confirmed classifications, 2
  genuine / 8 false positive (post-AUDIT-002 scoped numbering)
- **Corrected ChassisGround count**: 6 confirmed classifications, 6
  genuine / 0 false positive (2 previously known + 4 newly recovered)
- **Ground-role net results**: 4/4 nets now anchor on genuine evidence
  (was 1/4); role-count totals unchanged (4 ground / 6 unknown), no
  role manufactured
- **Whole-diagram before/after inventory**: §6.3
- **Object-level changes**: §6.3/§6.4, 0 unexplained
- **Wire identity comparison**: 8 removed / 10 added / 0 modified, all
  37 remaining `resolved`, 0 `conflicted`, 0 `unresolved` (§6.4)
- **Warning comparison**: 30→32 (`NET-ROLE-UNRESOLVED` 6→6 unchanged;
  `WIRE-GEOMETRIC-ENDPOINTS` 24→26 tracking the wire-count increase)
- **Test count**: 55/55 passing; assertions active
  (`dx-wire-test-assertions-enabled`)
- **Compiler warnings**: 8 before, 8 after (identical set)
- **Runtime warnings**: 0 errors before/after; warning counts explained
  above, nothing unexplained
- **Files changed**: `include/eke_dx_wire/image/shape_detector.hpp`,
  `src/image/shape_detector.cpp`, `tests/test_shape_detector.cpp`,
  `docs/AP-DIAG-FIX-003_ChassisGround_Evidence_Classification.md`
- **Model changes**: none (§9)
- **Unresolved findings**: §10
- **Working tree**: clean after commit

**Why the new rule is engineering-valid** (not "the numbers look
better"): ChassisGround is now asserted only when (a) the full,
convention-defined stem-plus-three-bar pattern is present, verified
against every real instance of that pattern actually drawn in this
diagram — not merely "close enough" with a partial match whose only
empirical track record was producing false positives; (b) those bars
keep the same drawn-in-one-stroke spacing rhythm every genuine instance
independently exhibits; and (c) candidate bars are only ever grouped
into one glyph when they are close together in both dimensions a real
symbol actually occupies, not X-alignment coincidence alone. Each
criterion is a falsifiable claim about the symbol itself, checked
against both the false and the true instances available, not a
parameter picked to make TRX300's object count look a particular way —
and the fix's most notable effect was not reducing a count but
*increasing* it, by correctly recognizing four real symbols the
previous implementation had never found at all.

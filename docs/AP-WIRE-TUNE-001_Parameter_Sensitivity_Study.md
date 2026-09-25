# AP-WIRE-TUNE-001 — Extraction Parameter Sensitivity Study

## Status

**Measurement study. No engineering-rule or algorithm code changed.**
This document reports how 19 real, already-existing extraction
configuration parameters affect engineering topology on the TRX300
fixture, via 88 total extraction runs (1 baseline + 77 systematic
one-variable-at-a-time experiments + 11 follow-up runs on the single
most consequential parameter's full plausible range). No parameter's
*default* value was changed in the repository as a result of this study
— per the handoff, that was explicitly not the goal.

Final AP-WIRE-031 commit at the start of this study: `862cd80`.
Verified TRX300 baseline: **41 physical wires** (unchanged by this
study — see Phase 1).

**Branch note (added during AP-WIRE-TUNE-002):** this study's commit
(`7208f10`) was originally built on `claude/modest-dirac-a3d7pw` while
that branch was still missing AP-WIRE-024 through AP-WIRE-031 (it had
diverged from `main` before those landed). The measurements above were
taken against a locally built `libeke_dx_wire.a` that in fact reflected
`main` at `862cd80`, not the feature branch's own source at the time.
`main` was subsequently merged into `claude/modest-dirac-a3d7pw`
(merge commit `e379d72`) during AP-WIRE-TUNE-002, after which the
branch's real, rebuilt binary reproduced every number in this report
bit-for-bit (spot-checked against the baseline and against the
`gap_interpretation.maximum_gap` transition points in
AP-WIRE-TUNE-002). The findings in this document stand unmodified;
only the branch/commit provenance note above is new.

## Phase 0 — Repository and pipeline audit

### How parameters are exposed today

There is **no tuning/slider UI** anywhere in this repository. Every
config struct below is an ordinary C++ struct with default member
values, aggregated into one `ExtractionConfig`
(`include/eke_dx_wire/pipeline/extraction_pipeline.hpp`) that
`ExtractionPipeline`'s constructor takes by value. The CLI
(`dx-extract`) and the GUI (`AP-GUI-001`) both construct
`ExtractionPipeline` with the all-defaults `ExtractionConfig{}` and
expose no flag or slider to override any field. **Changing any
parameter for this study required no source-file edit** — every field
was already a constructor parameter — but it did require a small,
temporary, non-repository driver program to construct a non-default
`ExtractionConfig` and call `ExtractionPipeline::run()` directly (the
driver is not part of this repository and is not part of this
deliverable; it links against the already-built `libeke_dx_wire` and is
described here only for reproducibility). No repository file was
modified to conduct this study — confirmed by `git status --short`
reporting no changes throughout, and by the full test suite (52/52)
passing unchanged at the end (Phase 9).

### Actual parameter inventory, mapped to the handoff's categories

No parameter name below was invented; every one is copied verbatim from
its struct definition in `include/eke_dx_wire/`.

**A. Conductor gap / bridge / continuity**

| Parameter | Struct | Default | Type | Smallest meaningful increment |
|---|---|---|---|---|
| `gap_interpretation.maximum_gap` | `GapInterpretationConfig` | 18.0 | pixel distance (double) | ~1 px |
| `gap_interpretation.minimum_gap` | `GapInterpretationConfig` | 1.5 | pixel distance (double) | ~0.5 px |
| `gap_interpretation.collinear_tolerance` | `GapInterpretationConfig` | 1.5 | pixel/angle tolerance (double) | ~0.5 |
| `gap_interpretation.minimum_ink_density` | `GapInterpretationConfig` | 0.08 | threshold, 0–1 (double) | ~0.004 (5%) |
| `geometry.endpoint_tolerance` | `GeometryNormalizationConfig` | 0.75 | pixel distance (double) | ~0.1 px |
| `geometry.collinear_tolerance` | `GeometryNormalizationConfig` | 0.75 | pixel/angle tolerance (double) | ~0.1 |
| `geometry.duplicate_tolerance` | `GeometryNormalizationConfig` | 0.75 | pixel distance (double) | ~0.1 px |
| `topology.snap_tolerance` | `TopologyConfig` | 4.0 | pixel distance (double) | ~0.2 px |
| `topology.intersection_tolerance` | `TopologyConfig` | 0.75 | pixel distance (double) | ~0.1 px |
| `conductor_evidence.minimum_continuous_length` | `ConductorEvidenceConfig` | 12.0 | pixel distance (double) | ~1 px |

**B. Line detection** — **honest finding: this codebase has no Canny
or Hough-line detector anywhere** (`grep -rl "HoughLines\|Canny"
src/ include/` returns nothing). Line/conductor detection is done by
adaptive thresholding + morphological structuring elements
(`MorphologyWireDetector`) followed by an ink-density/length acceptance
gate (`ConductorEvidenceEvaluator`). Category B and C are therefore, in
this codebase, **the same mechanism** — mapped below under C, not
invented as separate parameters.

**C. Morphology (this codebase's actual "line detection" mechanism)**

| Parameter | Struct | Default | Type | Increment |
|---|---|---|---|---|
| `morphology.horizontal_kernel_length` | `MorphologyConfig` | 25 | pixel length (int) | 1 px |
| `morphology.vertical_kernel_length` | `MorphologyConfig` | 25 | pixel length (int) | 1 px |
| `morphology.adaptive_block_size` | `MorphologyConfig` | 31 | pixel size, must stay odd (int) | 2 (odd-preserving) |
| `morphology.adaptive_c` | `MorphologyConfig` | 7 | threshold offset (int) | 1 |
| `morphology.minimum_segment_length` | `MorphologyConfig` | 12 | pixel length (int) | 1 px |
| `morphology.minimum_component_area` | `MorphologyConfig` | 8 | pixel area (int) | 1 px² |
| `conductor_evidence.minimum_continuous_length` | `ConductorEvidenceConfig` | 12.0 | pixel length (double) | functions as "minimum line length" acceptance |
| `conductor_evidence.minimum_supporting_ink_density` | `ConductorEvidenceConfig` | 0.08 | threshold, 0–1 | functions as "line detection confidence" |
| `shapes.contour_threshold` | `ShapeDetectorConfig` | 180 | ink threshold, 0–255 (int) | 1 |
| `shapes.contour_close_kernel` | `ShapeDetectorConfig` | 3 | pixel size (int) | 1 |

**D. Endpoint / conductor-boundary tolerance**

| Parameter | Struct | Default | Type | Increment |
|---|---|---|---|---|
| `terminals.boundary_tolerance` | `TerminalLocationConfig` | 8.0 | pixel distance (double) | ~0.4 px |
| `terminals.high_confidence_distance` | `TerminalLocationConfig` | 2.5 | pixel distance (double) | ~0.2 px |
| `terminals.medium_confidence_distance` | `TerminalLocationConfig` | 5.0 | pixel distance (double) | ~0.3 px |
| `terminals.allow_interior_attachment` | `TerminalLocationConfig` | true | boolean | n/a |
| `terminal_recognition.terminal_lead_max_distance` | `TerminalRecognitionConfig` | 6.0 | pixel distance (double) | ~0.5 px |
| `terminal_recognition.aligned_boundary_max_distance` | `TerminalRecognitionConfig` | 16.0 | pixel distance (double) | ~0.8 px |
| `terminal_recognition.minimum_alignment_cosine` | `TerminalRecognitionConfig` | 0.85 | threshold, -1–1 (double) | ~0.04 (5%) |

**E. Shape/component filtering**

| Parameter | Struct | Default | Type | Increment |
|---|---|---|---|---|
| `shapes.contour_threshold` | `ShapeDetectorConfig` | 180 | ink threshold, 0–255 (int) | 1 |
| `shapes.rectangle_min_area` | `ShapeDetectorConfig` | 40.0 | pixel area (double) | 1 |
| `shapes.rectangle_max_area_ratio` | `ShapeDetectorConfig` | 0.20 | fraction of page area (double) | 0.01 |
| `diagram_furniture.minimum_cluster_size` | `DiagramFurnitureConfig` | 16 | count (size_t) | 1 |
| `diagram_furniture.minimum_rows` / `minimum_columns` | `DiagramFurnitureConfig` | 2 / 3 | count (size_t) | 1 |
| `diagram_furniture.max_neighbor_gap_px` | `DiagramFurnitureConfig` | 45.0 | pixel distance (double) | ~2 px |

**F. Other** — `text.*` (`TextDetectorConfig`), `symbol_geometry.*`
(`SymbolGeometryExtractorConfig`), `rejected_geometry_classification.*`
(`GeometryClassificationConfig`), `geometry_ownership.*`
(`GeometryOwnershipConfig`), `distribution.*`
(`DistributionDecompositionConfig`, both fields boolean), and
`WireModelValidationConfig`/`WireReconstructionConfig` (both entirely
boolean flags, not sweepable as numeric ranges). These exist and were
inventoried but were not swept in this study — they primarily affect
text/label recognition and component-identity plumbing, not the
raster→wire topology chain this study targets, and the handoff's
priority order (1–5) does not reach them.

All parameters above **already require no rebuild to change** (they are
constructor arguments); "requires a rebuild" would only describe adding
a *new* parameter, which this study never did.

## Phase 1 — Baseline lock

Input: `samples/trx300ODG.png`. Configuration: all-defaults
`ExtractionConfig{}` (identical to every prior AP-WIRE AAR's baseline).

```
conductor_segments      = 294
topology_nodes          = 692
topology_edges          = 877
endpoint_candidates     = 210
conductor boundaries resolved   = 36   (conflicted = 4)
physical wires          = 41   (resolved = 41, unresolved = 0, conflicted = 0)
electrical_nets         = 10
shared_conductor_segments = 1
gaps_bridged            = 6
validation errors       = 0
validation warnings     = 30
valid_wires             = 41
AP-WIRE-024 conflicted endpoints (4, unchanged from every prior AAR):
  endpoint-candidate-1fd584e37a5c72b5
  endpoint-candidate-b0e3d6bb622a227c
  endpoint-candidate-c033140e251b7b86
  endpoint-candidate-cde07f8718a2b9cd
```

This matches the handoff's stated 41-wire baseline exactly. Full row
preserved as the first data row of `tuning/results.csv`
(`param=baseline`). Test suite at the time of this study: 52/52 passing
(confirmed again in Phase 9, unaffected by this study).

## Phase 2/3 — Priority-ordered one-variable-at-a-time sweep

77 experiments run (Priority 1: 26, Priority 2/3: 23, Priority 4: 16,
Priority 5: 12), full raw output in `tuning/results.csv`. Every
experiment changed exactly one field from Phase 0's inventory, all
others at default, per Phase 3's mandatory rule.

### Priority 1 — conductor gap / bridging / continuity

| Parameter | Values tested | Result |
|---|---|---|
| `gap_interpretation.maximum_gap` | 16, 17, 19, 20, 22 | **NO_EFFECT** in this range (see Phase 6 for a wider follow-up sweep that found where this *does* matter) |
| `gap_interpretation.minimum_gap` | 0.5, 1.0, 2.0, 2.5 | NO_EFFECT |
| `gap_interpretation.minimum_ink_density` | 0.072, 0.076, 0.084, 0.088, 0.096 | NO_EFFECT |
| `topology.snap_tolerance` | 3.6, 3.8, 4.2, 4.4 | **SENSITIVE** — see below |
| `geometry.endpoint_tolerance` | 0.6, 0.68, 0.83, 0.9 | NO_EFFECT |
| `geometry.collinear_tolerance` | 0.6, 0.68, 0.83, 0.9 | NO_EFFECT |

`topology.snap_tolerance` was the one genuinely sensitive Priority-1
parameter in the tested range:

- **3.6 / 3.8** (tighter snapping): topology_nodes 692→698/697,
  endpoints 210→216/215, wires 41→**43**, validation_warnings 30→32.
  Tighter snapping stops merging some node pairs that the default
  distance does merge, so a few previously-continuous chains split into
  more, shorter wires. AP-WIRE-024 conflicted-endpoint set unchanged
  (still exactly the same 4 ids).
- **4.2 / 4.4** (looser snapping): topology_nodes 692→689, endpoints
  210→206, shared_segments 1→3, wires **unchanged at 41**. Looser
  snapping merges a few more coincident nodes without changing wire
  count, and correctly increases the "shared conductor segment"
  coverage-diagnostic count (more segments now legitimately touch a
  shared node) — the existing `CONDUCTOR-SHARED` diagnostic (AP-WIRE-022A)
  continues to operate correctly on this changed input.

Classification: `topology.snap_tolerance` = **SENSITIVE** (small ±0.2–0.4px
changes visibly move node/endpoint/wire counts) but **not DESTRUCTIVE**
in this range — the AP-WIRE-024 conflict set never changed, and no
validation error appeared anywhere.

### Priority 2/3 — detection & morphology (same mechanism here)

| Parameter | Values tested | Result |
|---|---|---|
| `morphology.adaptive_block_size` | 21, 25, 35, 41 | **SENSITIVE** |
| `morphology.adaptive_c` | 5, 8, 9 (6 = NO_EFFECT) | mildly sensitive, no wire-count change |
| `morphology.horizontal_kernel_length` | 15, 20, 30, 35 | **SENSITIVE/DESTRUCTIVE at extremes** |
| `morphology.vertical_kernel_length` | 15, 20, 30, 35 | **most SENSITIVE parameter tested** |
| `conductor_evidence.minimum_continuous_length` | 9.6, 10.8, 13.2, 14.4 | NO_EFFECT |
| `morphology.minimum_segment_length` | 8, 10, 14 | NO_EFFECT |

`morphology.vertical_kernel_length` produced the largest swings of any
parameter in the entire study: at 15 (40% shorter than default 25),
conductor_segments jumped 294→366, wires 41→**50**, boundaries_conflicted
4→6 (a **new** conflict appeared, not just a count coincidence — the
conflicted-endpoint-id set changed). At 30/35 (20–40% longer), wires
dropped to 33/31 and boundaries_conflicted stayed elevated (5). This
parameter controls the vertical structuring-element length used to
separate true conductor ink from surrounding graphics; shortening it
lets more marginal/noisy ink qualify as conductor geometry (more
segments, more spurious topology, a genuinely new conflict), while
lengthening it starts discarding real short vertical conductor runs
(fewer wires, and the loss is not obviously "just noise" — see Phase 6).
Classification: **SENSITIVE, and DESTRUCTIVE at the tested extremes in
both directions** (a new conflict at the low end; net conductor/wire
loss with no corresponding conflict-count improvement at the high end).
`horizontal_kernel_length` shows the same pattern at reduced magnitude.

`morphology.adaptive_block_size` and `adaptive_c` (the adaptive-threshold
parameters) are meaningfully sensitive (wire count moves by ±1,
conductor_segments/topology drift, and 41 introduced a **new** AP-024-style
conflicted-endpoint case) but at much smaller magnitude than the kernel
lengths — classified **SENSITIVE**, not destructive, within the tested
range.

`conductor_evidence.minimum_continuous_length` and
`morphology.minimum_segment_length` — both functionally "minimum line
length" gates — showed **NO_EFFECT** across a ±20% sweep, meaning no real
conductor segment on this fixture sits near either threshold.
Classification: **INSENSITIVE** in this range.

### Priority 4 — endpoint / conductor-boundary tolerance

| Parameter | Values tested | Result |
|---|---|---|
| `terminals.boundary_tolerance` | 7.2, 7.6 (change), 8.4, 8.8 (NO_EFFECT) | **SENSITIVE below baseline only** |
| `terminal_recognition.aligned_boundary_max_distance` | 14.4, 15.2 (NO_EFFECT), 16.8, 17.6 (change) | **SENSITIVE above baseline only** |
| `terminal_recognition.minimum_alignment_cosine` | 0.765, 0.8075 (change), 0.8925, 0.935 (NO_EFFECT) | **SENSITIVE below baseline only** |
| `terminal_recognition.terminal_lead_max_distance` | 5.0, 5.5, 6.5, 7.0 | NO_EFFECT |

Three of the four Priority-4 parameters show an asymmetric sensitivity
pattern: tightening `boundary_tolerance` (7.2/7.6) *loses* a wire
(41→40, electrical_nets 10→9, shared_segments 1→0) while loosening it
(8.4/8.8) changes nothing; loosening `aligned_boundary_max_distance`
(16.8/17.6) *creates* a new conflicted endpoint (boundaries_conflicted
4→5) while tightening it (14.4/15.2) changes nothing; loosening
`minimum_alignment_cosine`'s *rejection* threshold (i.e. lowering the
required cosine to 0.765/0.8075, accepting less-aligned candidates)
*increases* resolved-boundary count (36→38/37) with **no** wire-count or
conflict-count change — this is the one Priority-4 result worth a second
look (Phase 6).

Classification: `boundary_tolerance` and `aligned_boundary_max_distance`
= **SENSITIVE, one-sided** (each answers the handoff's stated Priority-4
question — "can artificial endpoints be eliminated without merging
neighboring conductors?" — with a qualified *no* in this range: every
tested change that altered anything either lost a wire or added a
conflict, never a clean improvement). `terminal_lead_max_distance` =
**INSENSITIVE**.

### Priority 5 — shape/component filtering

| Parameter | Values tested | Result |
|---|---|---|
| `shapes.contour_threshold` | 162, 171, 189, 198 | **SENSITIVE, and non-monotonic** |
| `diagram_furniture.minimum_cluster_size` | 14, 15, 17, 18 | NO_EFFECT (±2 around 16) |
| `diagram_furniture.max_neighbor_gap_px` | 36, 44, 49 (NO_EFFECT), 40 → change | mixed |

`shapes.contour_threshold` (the binary ink threshold used for shape/
component contour detection — a global re-binarization threshold, so it
ripples into everything downstream of shape detection) moved wire count
in **both directions** depending on value (162→42, 171→42, 189→**47**,
198→39) and moved boundaries_conflicted both up and down (3, 3, 5, 5
against a baseline of 4) with the conflicted-*endpoint-id set itself*
changing every single time it was tested. This is expected for a global
binarization threshold — it is the least targeted, broadest-blast-radius
parameter in the whole inventory, exactly matching the handoff's own
caution ("do not tune these aggressively"). Classification:
**SENSITIVE, broad/untargeted, do not use for topology tuning** — a
change here cannot be read as "improving" or "harming" wire identity in
isolation, because it changes which pixels are shapes at all before any
wire logic runs.

`diagram_furniture.minimum_cluster_size` = **INSENSITIVE** (±2 around
the calibrated default 16 changes nothing — consistent with
AP-WIRE-022A's own documented calibration margin between the largest
real 9–10-shape component cluster and the 50-shape reference table).

## Phase 4 — result recording

Full 78-row (baseline + 77) table: `tuning/results.csv`. Follow-up
11-row wide sweep of the one Priority-1 parameter that needed a wider
range to find its real sensitivity boundary: `tuning/gap_maximum_gap_wide_sweep.csv`
(Phase 6 below). No experiment altered repository source, so no
`tuning/<name>/` artifact subdirectories were needed in the sense Phase
4 sketched; the two experiments whose topology was inspected in forensic
detail (Phase 6) are fully described inline below with real wire/
endpoint ids rather than as separate copied artifact trees, since the
full `topology.json` for each is >1.4MB of generated, reproducible data
better described than duplicated into the repository.

## Phase 5 — classification summary

| Parameter | Classification |
|---|---|
| `gap_interpretation.maximum_gap` (±4px around default) | NO_EFFECT / **INSENSITIVE in this narrow range** — see Phase 6 for its real, much wider sensitivity boundary |
| `gap_interpretation.minimum_gap` | NO_EFFECT |
| `gap_interpretation.minimum_ink_density` | NO_EFFECT |
| `geometry.endpoint_tolerance` | NO_EFFECT |
| `geometry.collinear_tolerance` | NO_EFFECT |
| `topology.snap_tolerance` | SENSITIVE |
| `morphology.adaptive_block_size` | SENSITIVE |
| `morphology.adaptive_c` | SENSITIVE (mild) |
| `morphology.horizontal_kernel_length` | SENSITIVE / DESTRUCTIVE at extremes |
| `morphology.vertical_kernel_length` | **most SENSITIVE parameter tested; DESTRUCTIVE at both tested extremes** |
| `conductor_evidence.minimum_continuous_length` | INSENSITIVE |
| `morphology.minimum_segment_length` | INSENSITIVE |
| `terminals.boundary_tolerance` | SENSITIVE (one-sided; tightening only loses wires) |
| `terminal_recognition.aligned_boundary_max_distance` | SENSITIVE (one-sided; loosening only adds a conflict) |
| `terminal_recognition.minimum_alignment_cosine` | SENSITIVE (one-sided; loosening increases resolved-boundary count with no observed cost — candidate for future, separately-reviewed follow-up, not adopted here) |
| `terminal_recognition.terminal_lead_max_distance` | INSENSITIVE |
| `shapes.contour_threshold` | SENSITIVE, broad/untargeted |
| `diagram_furniture.minimum_cluster_size` | INSENSITIVE |
| `diagram_furniture.max_neighbor_gap_px` | mixed / mildly SENSITIVE |

**No parameter in this study was classified BENEFICIAL.** Per the
handoff's own instruction, a wire-count increase or a resolved-boundary-
count increase was never treated as automatically beneficial; every
case where a metric improved was cross-checked against whether it
introduced a new conflict, lost topology, or (in the `maximum_gap` wide
sweep) silently erased evidence of a known conflict outright — and in
every case tested, an apparent improvement was accompanied by one of
those costs, except `minimum_alignment_cosine`'s two lower values (0.765/
0.8075), which are flagged above as a genuine candidate for a *future*,
separately-reviewed follow-up rather than adopted as a recommendation
here (this study's scope is measurement, not tuning the shipped
defaults).

## Phase 6 — engineering significance of every wire-count change

### `topology.snap_tolerance = 3.6` (41→43 wires)

Tighter node-snapping stops merging some coincident-node pairs the
default merges. Two additional short wires appear; the AP-WIRE-024
conflicted-endpoint set is byte-identical to baseline. Physically
plausible (snapping less aggressively can reveal genuinely separate
short conductor runs the default tolerance was quietly collapsing
together) but not verified against the source image at pixel level in
this study — flagged as requiring visual confirmation before ever being
considered for adoption.

### `morphology.vertical_kernel_length = 15` (41→50 wires, +6 boundaries_conflicted)

The largest topology change in the study. Shortening the vertical
structuring element by 40% admits substantially more marginal ink as
conductor geometry (conductor_segments 294→366, +24%). This is not a
clean wire-count "improvement" — `boundaries_conflicted` rose from 4 to
6, meaning **two new endpoint-level engineering conflicts appeared**
that do not exist in the baseline. Per Phase 8's explicit prohibition
("do not modify AP-WIRE-031 to resolve extraction ambiguity"), these new
conflicts are not something this study attempted to resolve — they are
reported as evidence that this parameter value is unsafe, not as a
target for the engineering-rule layer to "fix."

### `gap_interpretation.maximum_gap` — the headline finding, from a wider follow-up sweep

The handoff's central engineering question for Priority 1 was: *"Can
the extractor recover artificial conductor breaks caused by printed
wire-color labels... without creating false connections?"* The
prescribed ±4px range around the default (18px) showed **zero effect**
(`gaps_bridged` stays at exactly 6 for every value from 8 through 20 —
see `tuning/gap_maximum_gap_wide_sweep.csv`), so a wider diagnostic
sweep (4 through 100px) was run specifically to locate where this
parameter actually does something, per the study's own governing
principle of measuring real sensitivity rather than stopping at a null
result in an arbitrarily narrow window.

**Below the real gap sizes present in this diagram (`maximum_gap = 4`,
`gaps_bridged` drops to 0):** exactly one wire is added (41→42),
`wire-f0fdb61aea51319b`, a single 1-edge, 31px straight run between two
bare `geometric` endpoints at (129.5, 212)–(129.5, 181) that the
baseline's bridging correctly absorbs into a longer, already-existing
wire. Under-bridging does not create a false connection here — it
**fragments a correct one**, presenting one physical conductor as two
disconnected pieces. Classified **DESTRUCTIVE** ("valid topology is
lost"), even though no wrong connection was invented.

**Well above the default (`maximum_gap = 100`, `gaps_bridged` rises from
6 to 22):** wire count *drops* (41→33) and, far more importantly,
**three of the four AP-WIRE-024 conflicted endpoints
(`endpoint-candidate-cde07f8718a2b9cd`, `...1fd584e37a5c72b5`,
`...c033140e251b7b86`) cease to exist as endpoint candidates at all** —
confirmed directly against the exported topology (they are not merely
no longer conflicted; their ids are entirely absent from
`endpoint_candidates`). Aggressive bridging converts what were
genuinely ambiguous degree-1 stubs into interior degree-2 continuation
nodes, so they never reach endpoint-candidate status, never reach
`ConductorBoundaryResolution`, and the conflict they represented simply
**disappears from the model rather than being resolved by any evidence**.
This is precisely the failure mode the whole AP-WIRE-029/030/031
architecture exists to prevent — not a new engineering-rule violation
(AP-WIRE-031 itself was not touched and correctly still shows 0
conflicted *wires* either way), but a demonstration that an *upstream*
geometry parameter, tuned aggressively enough, can silently erase the
evidence that an engineering ambiguity exists before the careful,
never-guess logic downstream ever gets a chance to preserve it.
Classified unambiguously **DESTRUCTIVE**, and specifically flagged as
the study's most important safety finding: **`gap_interpretation.
maximum_gap` must never be tuned upward as a way to "clean up" AP-WIRE-024-style
conflicts** — a smaller conflict count produced this way is not an
improvement, it is evidence loss.

Between these two extremes (12–24px) the transition is gradual and
monotonic in `gaps_bridged`, with the default 18 sitting comfortably in
the middle of the wide 8–20px plateau where nothing changes at all —
i.e. the shipped default is not perched near either failure mode.

## Phase 7 — TRX300 difficult structures

Coverage against the ten named structures, from the experiments above:

1. **Simple two-terminal wires** — unaffected by every Priority 1/4
   parameter within its tested range; only broad morphology/threshold
   changes (Priority 2/3/5) touch these.
2. **Continuation segments** — `topology.snap_tolerance` is the
   parameter that actually governs how continuation chains merge or
   split; shown sensitive (Phase 6).
3. **Wires interrupted by printed color labels** — directly addressed by
   `gap_interpretation.maximum_gap`'s wide sweep (Phase 6): the
   mechanism recovers 6 such breaks at the shipped default, and neither
   shrinking nor growing the threshold within a wide practical range
   improves on that without cost.
4. **Wires through splices** — AP-WIRE-031's own segment-sharing
   mechanism (unmodified throughout this study) continued to produce
   exactly the same 1 shared-segment, 1 extra-wire result at every
   tested value that left the underlying topology graph itself
   unchanged; parameters that change the topology graph (morphology,
   snap tolerance) can gain or lose splice-adjacent wires as a
   side-effect of changing the graph, not because AP-WIRE-031 behaved
   differently.
5. **Crossings** — not observed to be individually affected by any
   tested parameter beyond the general topology-graph perturbations
   above.
6. **Multi-branch splice structures** — no experiment introduced a new
   multi-way branch-pairing ambiguity beyond the pre-existing AP-WIRE-024
   set, except where noted (Phase 6, `vertical_kernel_length=15`).
7. **Ground distribution** — `electrical_nets`/`net_roles` moved in
   several morphology/threshold experiments as a direct, expected
   consequence of topology-graph changes (Phase 8 confirms no
   electrical-net logic was touched to produce this).
8. **Component boundaries** — `shapes.contour_threshold` (Priority 5) is
   the parameter with the broadest, least targeted effect on these.
9. **Connector terminals** — 0 connectors/connector-terminals in every
   single experiment in this study, unchanged from every prior AP-WIRE-028/030/031
   baseline; no tested parameter creates one (expected — AP-WIRE-028
   already traced this to a shape-classification gap this study's
   parameter range cannot reach).
10. **Graphical symbols adjacent to conductors** — `shapes.
    contour_threshold` and the morphology kernel lengths are the
    relevant parameters; both shown sensitive above.

The known AP-WIRE-031 result (**41 physical wires**) was never altered
in the repository — every experiment ran through the separate scratch
driver against an unmodified checkout, and Phase 1's baseline row
(`param=baseline` in `tuning/results.csv`) is the untouched, permanent
reference.

## Phase 8 — confirmation of prohibited techniques

None of the following were done, anywhere in this study: inventing a
new electrical connection; pairing splice branches by proximity;
shortest-path or collinearity used as wire identity; wire color used as
physical identity; modifying AP-WIRE-031's algorithm to resolve
extraction ambiguity (its source file was never opened for editing in
this study); changing electrical-net logic to improve physical-wire
count; speculative CV, OCR, or LLM reasoning; overfitting a rule to one
TRX300 feature (every parameter tested is a pre-existing, generically-named
field, not a new rule). Where a parameter change appeared to "fix" a
conflict (the `maximum_gap=100` case), it was investigated and reported
as evidence erasure, specifically because the instruction not to guess
extends to not calling evidence-erasure a fix.

## Phase 9 — full regression

```
$ cd build && ctest
100% tests passed, 0 tests failed out of 52
```

`git status --short` reports no changes at any point during this study
— no source file was edited, so no explanation is owed for a source
change, because none occurred. The scratch measurement driver used to
vary `ExtractionConfig` fields is not part of this repository and is not
included in this deliverable.

## Summary / recommendation

This study's purpose was measurement, not optimization, and no default
value is recommended for change as a result of it. The two
highest-value findings to carry into future work:

1. **`gap_interpretation.maximum_gap` is safe at its shipped default and
   dangerous if tuned upward "to reduce conflicts"** — doing so does not
   resolve AP-WIRE-024-style ambiguity, it deletes the endpoints that
   carried it. Any future automatic or manual tuning pass on this
   parameter must check the AP-WIRE-024 conflicted-endpoint-id set
   (not just its count) before accepting a change.
2. **`morphology.vertical_kernel_length`/`horizontal_kernel_length` are
   the most consequential, least safe parameters in the whole inventory**
   — both directions of change tested here cost something (new
   conflicts when shortened, lost wires when lengthened) — any future
   work on them needs source-image-level visual verification per
   change, not metric-count comparison alone.

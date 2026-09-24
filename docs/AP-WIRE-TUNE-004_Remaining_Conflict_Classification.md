# AP-WIRE-TUNE-004 — Remaining AP-WIRE-024 Conflict Classification

## Status

**Forensic classification only. No source file was modified.**
This task classifies the three AP-WIRE-024 conflicts that survive
AP-WIRE-FIX-001 (CONFLICT-01, CONFLICT-03, CONFLICT-04) and determines,
for each, whether it is a software defect, a parameter-sensitive
extraction issue, or a legitimate unresolved condition. `git status
--short` is clean throughout; `gap_interpretation.maximum_gap` was left
at 18.0px; AP-WIRE-024/029/030/031 and AP-WIRE-FIX-001 were not touched.

## Phase 0 — Baseline

```
Branch: claude/modest-dirac-a3d7pw
HEAD:   a45da38 (AP-WIRE-FIX-001)
```

Fresh Release build (no changes to compile — tree already current);
`ctest`: **52/52 passing**.

Fresh TRX300 extraction (`samples/trx300ODG.png`, all-default
`ExtractionConfig`, i.e. `gap_interpretation.maximum_gap = 18.0`):

```
physical wires    = 41
electrical nets   = 10
AP-WIRE-024 conflicts = 3
conflicted_endpoint_ids = endpoint-candidate-1fd584e37a5c72b5 (CONFLICT-01),
                          endpoint-candidate-c033140e251b7b86 (CONFLICT-03),
                          endpoint-candidate-cde07f8718a2b9cd (CONFLICT-04)
```

`endpoint-candidate-b0e3d6bb622a227c` (CONFLICT-02) is **absent** from
the conflicted set, confirmed still Resolved by AP-WIRE-FIX-001 (its
`conductor-boundary-resolution-3119f42d457ef725` shows `boundary_status:
resolved`, `component_id: component-candidate-shape-region-845947b0afd7451a`,
`confidence: high`) — this run used a fresh rebuild of the scratch
driver against the current library, not any file left over from a
previous session.

## Phase 1 — Frozen conflict identities

| | CONFLICT-01 | CONFLICT-03 | CONFLICT-04 |
|---|---|---|---|
| Endpoint | `endpoint-candidate-1fd584e37a5c72b5` | `endpoint-candidate-c033140e251b7b86` | `endpoint-candidate-cde07f8718a2b9cd` |
| Node | `topology-node-8f6a7c0b370305c5` @ (869, 365) | `topology-node-b5a86df129808446` @ (881.5, 365) | `topology-node-f23492300e26f06a` @ (584, 458) |
| Boundary resolution | `conductor-boundary-resolution-4ce4d99138dab9ea` | `conductor-boundary-resolution-a4f2f28d4146195c` | `conductor-boundary-resolution-5478ba59ad5b519c` |
| Incident edge | `topology-edge-069517e7a7b6d577` | `topology-edge-baa4e5a80b46d6d8` | `topology-edge-1b5f353ffe8d3644` |
| Conductor segment | `normalized-conductor-segment-b4ad1ee582fe2a64` | `normalized-conductor-segment-bae03ff8a8a054f1` | `normalized-conductor-segment-8e4e3038b93cfa75` |
| "Ground"/splice evidence component (High confidence) | `component-candidate-shape-region-432be0a200811b76` | `component-candidate-shape-region-432be0a200811b76` (**same** component as CONFLICT-01) | `component-candidate-shape-region-6a7001c24fe4c252` |
| Competing "component_terminal" evidence component(s) (Low confidence) | `component-candidate-shape-region-4f1e5de1ef1830bd` | `component-candidate-shape-region-4f1e5de1ef1830bd` (**same** as CONFLICT-01) | `component-candidate-shape-region-320c9114d1207736` **and** `component-candidate-shape-region-64afaa2a14134a2a` |
| Wire | `wire-…` (start/end unaffected by any conflict; unchanged across this whole task) | (each is one degree-1 endpoint of a distinct straight wire; none touch each other's topology) | |

No electrical net, connector, or terminal identifier is involved in any
of the three — all three stop at component-association, exactly like
CONFLICT-02.

## Phase 2 — Reconstructing every candidate

All three conflicts have **the same internal shape**, differing only in
location:

- **Candidate "ground"** (High confidence, `TerminalLocationDetector`,
  `terminals.*` config): the endpoint sits at distance ≈0–1px from a
  real, distinctly-drawn splice/ground housing (a notched box for
  CONFLICT-01/03, a notched oval for CONFLICT-04). `component_status:
  Resolved`, `ground_status: Resolved`.
- **Candidate "component_terminal"** (Low confidence, `TerminalRecognizer`,
  boundary-alignment path — **not** the TerminalLead-primitive path
  AP-WIRE-FIX-001 corrected): the endpoint sits 9–14px from a *second*,
  much smaller (9×9 to 12×9px) `ComponentCandidate` of
  `ComponentSymbolKind::CircularSymbol`, confidence **High** at the
  shape-candidate level itself. Confirmed via `symbol_primitives`: none
  of these three small components own any `SymbolPrimitive` at all —
  they are pure `ComponentCandidate` shape regions with no internal
  geometry, ruling out any TerminalLead-based path.

`ConductorBoundaryResolver`'s multi-category rule
(`src/topology/conductor_boundary_resolver.cpp:278-322`) flags an
endpoint `Conflicted` the instant **two different boundary categories**
(here: `ComponentTerminal` and `Ground`) both carry any evidence at all,
with no confidence-based tie-break — the same "never pick one"
principle already confirmed for CONFLICT-02's within-category case,
now shown to also apply *across* categories.

## Phase 3 — Earliest divergence stage

**Shape / component-candidate detection** — earlier than CONFLICT-02's
defect stage. Specifically `ShapeDetector::detect_circles()`
(`src/image/shape_detector.cpp:397-495`), which runs during initial
raster shape extraction, well before `SymbolGeometryExtractor`,
`TerminalRecognizer`, `ConductorBoundaryResolver`, or AP-WIRE-024
conflict detection ever execute.

The three small "component_terminal" components
(`4f1e5de1ef1830bd`, `320c9114d1207736`, `64afaa2a14134a2a`) are visually
confirmed, by direct raster inspection, to be **wire-crossing
intersections** — points where a horizontal bus conductor crosses one
or more vertical drop wires — not drawn symbols of any kind. Their
geometry (9–12px bounding boxes, roughly square, `circular_symbol`
confidence "High") passes `detect_circles()`'s contour-based
circularity/edge-support/interior-density gate purely because a
crossing's ink pattern is, at this scale, compact and roughly circular
enough. One of the three (`4f1e5de1ef1830bd`, 12×9px) has an aspect
ratio of 1.33 against a `circle_max_aspect_ratio` limit of 1.35 —
passing by a margin of 0.02, i.e. barely.

Notably, `detect_circles()`'s own source comment already anticipates
exactly this risk: *"HoughCircles is intentionally not used here. On
wiring diagrams it readily interprets wire intersections, connector
holes, text glyphs, and other repeated geometry as circles.
Closed-contour geometry gives us stronger evidence..."* — the
contour-based approach was chosen specifically to avoid this failure
mode, but this diagram's crossings are dense/compact enough to still
pass its circularity, edge-support, and interior-density thresholds.

## Phase 4 — Comparison with CONFLICT-02

**Not the same defect**, though the same general character (an
extraction stage manufacturing evidence for something that isn't really
there). Differences, confirmed directly:

| | CONFLICT-02 (fixed) | CONFLICT-01/03/04 (this task) |
|---|---|---|
| Stage | `SymbolGeometryExtractor::extract()` (internal-primitive scan of an *already-accepted* component's own ROI) | `ShapeDetector::detect_circles()` (the *earlier* stage that decides whether a shape region becomes a `ComponentCandidate` at all) |
| Mechanism | Two real, adjacent components' boxes overlap; one's own exit-stroke ink gets rediscovered as the other's internal primitive | A wire-bus crossing's ink is itself misclassified as a whole new (spurious) small circular component |
| Owns a `SymbolPrimitive`? | Yes (the spurious primitive AP-WIRE-FIX-001 now excludes) | No — these are bare `ComponentCandidate`s with zero internal primitives |
| Addressed by AP-WIRE-FIX-001? | Yes | **No** — confirmed empirically (byte-identical `conductor_boundary_resolutions` for all three, before and after the fix) and structurally (the defect is upstream of `SymbolGeometryExtractor` entirely) |

AP-WIRE-FIX-001 does not, and was never positioned to, address this
class of failure.

## Phase 5 — Gap-suppression mechanism (re-confirmed, not re-derived)

Re-verified against the current (post-fix) build; identical to
AP-WIRE-TUNE-002's findings, unaffected by AP-WIRE-FIX-001 (which never
touches `GapInterpreter`):

| Conflict | Suppression threshold | Mechanism |
|---|---|---|
| CONFLICT-01 | `maximum_gap` (32.00, 32.05] | Node `topology-node-8f6a7c0b370305c5` bridges to a node ≈32px away and is retyped `ConductorEnd → Continuation` before `EndpointReconstructor` runs, so it never becomes an `EndpointCandidate` again |
| CONFLICT-03 | (51.0, 51.2] | Same mechanism, ≈51px bridge |
| CONFLICT-04 | (99.0, 99.1] | Same mechanism, ≈99px bridge |

This remains **evidence suppression, not resolution** — the node simply
stops qualifying for candidacy; nothing about the "ground vs.
component_terminal" evidence conflict identified in Phases 2–4 is ever
evaluated or corrected by gap bridging. `gap_interpretation.maximum_gap`
was left at 18.0 throughout this task, per the frozen invariant.

## Phase 6 — Existing-parameter local tests

| Parameter | Values tested | Effect |
|---|---|---|
| `terminal_recognition.aligned_boundary_max_distance` (default 16.0, ±0.8) | 15.2, 16.0, 16.8 | No change to CONFLICT-01/03/04 (16.8 only affects an unrelated endpoint elsewhere) |
| `terminal_recognition.minimum_alignment_cosine` (default 0.85) | 0.81 → 0.97 | **No change whatsoever**, even at very strict 0.97 — the spurious candidates' alignment cosine is essentially 1.0 (near-perfect), so this parameter cannot discriminate them |
| `terminal_recognition.terminal_lead_max_distance` (default 6.0, ±0.5) | 5.5, 6.0, 6.5 | No change (confirms boundary-alignment path, not lead-primitive path) |
| `terminals.boundary_tolerance` (default 8.0, ±0.4) | 7.6, 8.0, 8.4 | No change to these three (7.6 causes an unrelated wire loss elsewhere, already known from TUNE-003) |

Since the ±1-increment neighborhood produced no change, a wider,
still-targeted probe of `aligned_boundary_max_distance` was run (the
spurious candidates sit at real distances of 14px for CONFLICT-01/03
and 9–10px for CONFLICT-04):

| `aligned_boundary_max_distance` | boundaries_conflicted | wires | Conflicts remaining |
|---|---|---|---|
| 16.0 (default) | 3 | 41 | 01, 03, 04 |
| 13.0 | 1 | 41 | 04 only |
| 11.5 – 13.0 | 1 | **41 (unchanged)** | 04 only |
| 11.0 | 1 | **40** | 04 only, but a wire is lost |
| 10.0 – 9.0 | 1 | 40 | 04 only |
| 8.0 | **0** | 40 | none — but two wires' worth of collateral damage already present |

**CONFLICT-01 and CONFLICT-03 both disappear cleanly at
11.5 ≤ `aligned_boundary_max_distance` ≤ 13**, with **zero** wire-count
change and, critically, a full before/after model diff shows only the
two conflicts' own resolutions change — **except** one additional,
previously-unrelated endpoint (`endpoint-candidate-7b1232ec85583772` at
(880.5, 416), a Low-confidence but non-conflicted `component_terminal`
match) loses its resolution entirely (`Resolved(Low) → Unresolved`).
This is a real, if small, side effect even in the "safe" window — the
same blunt distance cutoff that stops the spurious evidence for
CONFLICT-01/03 also stops a different, seemingly-legitimate weak match
elsewhere.

**CONFLICT-04 cannot be cleanly separated this way** — its two spurious
candidates sit at 9–10px, inside the range where tightening the
threshold *also* starts destroying a genuine wire's terminal evidence
(wire count 41→40 already at 11.0, well above where CONFLICT-04's own
spurious evidence disappears at ≤8.0).

## Phase 7 — Positive-evidence test

For all three: **the answer is "the competing evidence disappeared,"
not "new physical continuity was established."** No new evidence ever
appears; the same High-confidence ground/splice evidence that was
always present is what remains after the weak evidence is starved out.

This is **not** the same as TUNE-002's gap-suppression (which erases an
already-formed `EndpointCandidate`/node from existence). Here, the
`EndpointCandidate` and its strong ground evidence are untouched at
every tested value — only whether the *weak, spurious* second piece of
evidence gets generated at all changes, and it changes only because of
where this specific diagram's crossings happen to sit in distance, not
because of any judgment about which evidence is trustworthy. Per the
task's own definitions, this is closest to **parameter-mediated
avoidance of an upstream defect's symptom** — not an independent
resolution mechanism, and (per the side effect on
`endpoint-candidate-7b1232ec85583772`) not even a clean one. It is
explicitly **not recommended** as a fix; the defect's actual locus is
Phase 3's `ShapeDetector::detect_circles()`.

## Phase 8 — Source-diagram evidence

Direct raster inspection (crops preserved, see Phase 10) confirms, for
every one of the three endpoints, that **the diagram itself is not
ambiguous**:

- CONFLICT-01/03: two wires labeled "P/W" and "R" (wire-color
  convention, visible in the raster; not OCR-recognized in this run)
  drop from a bus and terminate cleanly at a single, distinctly drawn
  notched-box splice/ground symbol. The only other "candidate" nearby
  is the visual crossing of two bus lines over the same two wires,
  ~14px above — plainly not a second component in the drawing.
- CONFLICT-04: three wires (labeled "G", "R/B", "LG/W") terminate at a
  single, distinctly drawn notched-oval splice symbol with an internal
  reference label. The two competing "candidates" are, again, visibly
  just the crossing of bus lines over these wires, 9–10px above.

Because the true terminus is unambiguous once the crossing-derived
evidence is disregarded, this rules out **SOURCE-DIAGRAM AMBIGUITY** —
the diagram supplies a single, clear, explicit termination point per
wire; the ambiguity is manufactured entirely by software, not present
in the source.

## Phase 9 — Classification

All three: **UPSTREAM EXTRACTION DEFECT**.

`ShapeDetector::detect_circles()` (`src/image/shape_detector.cpp:397-495`)
classifies a wire-bus crossing's compact, roughly-circular ink pattern
as a genuine `ComponentCandidate` (`ComponentSymbolKind::CircularSymbol`,
confidence High), despite the function's own header comment explicitly
naming "wire intersections" as a known risk the contour-based approach
was chosen to avoid. `TerminalRecognizer`'s boundary-alignment path then
legitimately, mechanically associates this spurious component with a
nearby real wire endpoint (Low confidence, correctly reflecting the
distance), and `ConductorBoundaryResolver` correctly, mechanically
reports `Conflicted` once both a component and a ground category carry
any evidence — exactly as AP-WIRE-030's decision matrix specifies. Every
stage from `TerminalRecognizer` onward is behaving correctly on flawed
input, precisely the same downstream-is-innocent pattern established
for CONFLICT-02 in AP-WIRE-TUNE-003 — just one stage further upstream.

## Phase 10 — No fixes; artifacts preserved

No file was modified. Raster crops preserved for this task (not
committed to the repository, consistent with prior AP-WIRE-TUNE
sessions' convention of keeping generated raster/JSON artifacts as
working files, offered to the user directly):

- `/tmp/tune004/crop_conflict01_03.png` — wide view of the CONFLICT-01/03
  region (bus crossings, both drop wires, the notched splice box)
- `/tmp/tune004/crop_ground_component_01_03.png` — tight crop of the true
  (correct) splice/ground terminus
- `/tmp/tune004/crop_small_component_01_03.png` — tight crop of the
  spurious crossing misclassified as a component
- `/tmp/tune004/crop_conflict04_wide.png`, `crop_conflict04_tight.png`,
  `crop_conflict04_terminus.png` — the equivalent three views for
  CONFLICT-04
- `/tmp/tune004/baseline.json` — full topology dump used for this task's
  analysis

Proposed correction (**not implemented**, per this task's charter):
`ShapeDetector::detect_circles()` should exclude a circle-candidate
whose location coincides with an intersection of two already-detected
line-like contours/segments (conceptually the same "check against
already-known conductor geometry before accepting new evidence"
principle AP-WIRE-FIX-001 already applied one stage later, in
`SymbolGeometryExtractor`) — e.g. reject a circle candidate whose
bounding box is crossed by two roughly-perpendicular line contours each
spanning well beyond the candidate's own box, rather than relying solely
on circularity/edge-support/interior-density of the local blob.

## Phase 11 — Cross-conflict matrix

| Conflict | Earliest Stage | Classification | Gap Suppression Threshold | Evidence Type |
|---|---|---|---|---|
| CONFLICT-01 | `ShapeDetector::detect_circles()` | UPSTREAM EXTRACTION DEFECT | (32.00, 32.05] px | Spurious `circular_symbol` (crossing) vs. real ground/splice |
| CONFLICT-03 | `ShapeDetector::detect_circles()` | UPSTREAM EXTRACTION DEFECT | (51.0, 51.2] px | Spurious `circular_symbol` (crossing, **same component id as CONFLICT-01**) vs. real ground/splice (**same component id as CONFLICT-01**) |
| CONFLICT-04 | `ShapeDetector::detect_circles()` | UPSTREAM EXTRACTION DEFECT | (99.0, 99.1] px | Two spurious `circular_symbol`s (two crossings) vs. real ground/splice |

**All three share one upstream cause.** This is stronger than mere
correlation: CONFLICT-01 and CONFLICT-03 share the *exact same pair* of
component ids (`432be0a200811b76` ground, `4f1e5de1ef1830bd` spurious
crossing) — they are two different wires dropping through the *same*
bus crossing into the *same* splice box. CONFLICT-04 is geometrically
independent (a different bus crossing, a different splice) but fails
via the identical mechanism in `ShapeDetector::detect_circles()`. Gap
suppression thresholds differ only because they reflect unrelated,
coincidental real distances between each endpoint and *whatever* other
node the widened gap eventually bridges to — not evidence of a shared
gap-specific cause.

## Phase 12 — Priority for next fix

Only one defect locus was found (`ShapeDetector::detect_circles()`), so
there is one fix to prioritize, not several to rank:

- **Defect scope:** narrow — a single function, gated by four existing
  numeric thresholds (`circle_min_circularity`, `circle_max_aspect_ratio`,
  `circle_min_edge_support`, `circle_max_interior_ink_density`); the
  proposed correction adds one new check, no new subsystem.
- **Reproducibility:** fully deterministic and already reproduced three
  times on one fixture (CONFLICT-01, 03, 04); the same crossing-vs-symbol
  ambiguity is a generic risk for any diagram with a bus crossing
  multiple drop wires, not specific to TRX300.
- **Affected pipeline stage:** `src/image/shape_detector.cpp`
  (`detect_circles()`), i.e. the very first stage where raster ink
  becomes a `ComponentCandidate` — fixing it here prevents the spurious
  evidence from ever propagating into `TerminalRecognizer` or
  `ConductorBoundaryResolver`, the same "fix at the source, not the
  symptom" principle AP-WIRE-FIX-001 already established.
- **Number of affected objects on TRX300:** 3 spurious `ComponentCandidate`s,
  3 downstream `ConductorBoundaryResolution` conflicts, 0 wires, 0
  electrical nets — a small, well-contained blast radius, smaller even
  than CONFLICT-02's single-primitive fix in terms of conflicts resolved
  per line of investigation.
- **Regression-test feasibility:** high — `ShapeDetector` already has an
  existing unit-test file (`tests/test_shape_detector.cpp`) that
  synthesizes raster ink with `cv::line`/`cv::circle` exactly like
  `test_symbol_geometry_extractor.cpp` did for AP-WIRE-FIX-001; a
  synthetic crossing (two perpendicular lines) alongside a synthetic
  genuine circle is a direct, deterministic reproduction, in the same
  style as AP-WIRE-FIX-001's regression test.

**Recommended next fix task: correct `ShapeDetector::detect_circles()`**
to exclude circle-candidates that coincide with a line-on-line crossing
already accounted for by other detected line contours, following the
same "exclude ink already explained by other evidence" architecture as
AP-WIRE-FIX-001, one stage earlier in the pipeline.

## Phase 13 — Regression

```
$ cd build && ctest
100% tests passed, 0 tests failed out of 52
```

`git status --short`: clean. No source file was modified anywhere in
this task; `gap_interpretation.maximum_gap` remained 18.0px throughout.

## Final report

1. **HEAD:** `a45da38`.
2. **Branch:** `claude/modest-dirac-a3d7pw`.
3. **Baseline:** 41 wires, 10 electrical nets, 3 AP-WIRE-024 conflicts,
   52/52 CTest.
4. **CONFLICT-02:** confirmed still Resolved (High confidence,
   component `845947b0afd7451a`), reproduced on a fresh rebuild.
5–7. **Exact identities:** Phase 1 table above.
8. **Candidate evidence:** Phase 2 — one High-confidence ground/splice
   candidate (real) plus one or two Low-confidence `component_terminal`
   candidates (spurious, from `ShapeDetector`) per conflict.
9. **Earliest divergence stage:** `ShapeDetector::detect_circles()` for
   all three — shape/component-candidate detection, earlier than
   CONFLICT-02's `SymbolGeometryExtractor` stage.
10. **Relationship to CONFLICT-02:** different defect, different
    (earlier) stage, not addressed by AP-WIRE-FIX-001 (Phase 4).
11. **Gap suppression:** identical node-retyping mechanism as
    TUNE-002, at ≈32px/51px/99px respectively — unaffected by
    AP-WIRE-FIX-001, unrelated to the true root cause (Phase 5).
12. **Parameter sensitivity:** `aligned_boundary_max_distance` provides
    a partial, imprecise, side-effect-bearing mitigation for
    CONFLICT-01/03 only; does not reach CONFLICT-04 cleanly; not
    recommended (Phase 6/7).
13. **Source-diagram evidence:** each wire has one clear, single,
    correctly-drawn terminus; the diagram itself is not ambiguous
    (Phase 8).
14. **Final classification:** **UPSTREAM EXTRACTION DEFECT** for all
    three (Phase 9).
15. **Cross-conflict comparison:** all three share one upstream cause;
    CONFLICT-01 and CONFLICT-03 additionally share the exact same pair
    of component ids (Phase 11).
16. **Confirmed software defect:** `ShapeDetector::detect_circles()`,
    `src/image/shape_detector.cpp:397-495` — misclassifies wire-bus
    crossings as `circular_symbol` `ComponentCandidate`s.
17. **Proposed next fix (not implemented):** exclude a circle-candidate
    coinciding with a line-on-line crossing already evidenced by other
    detected line contours (Phase 12).
18. **Regression:** 52/52 CTest passing.
19. **Build:** clean; no rebuild required (tree unchanged since
    AP-WIRE-FIX-001).
20. **Artifacts generated:** this report only (committed); raster crops
    and topology dump under `/tmp/tune004/` (working artifacts, offered
    to the user, not committed).

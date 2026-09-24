# AP-WIRE-TUNE-002 — Gap Parameter Forensic Characterization

## 0. Status and a required correction to how this study was conducted

**Measurement study. No engineering-rule or algorithm code modified by
hand.** This document forensically characterizes exactly one parameter,
`gap_interpretation.maximum_gap`, on the TRX300 fixture: where it helps,
where it becomes dangerous, and the precise mechanism in both cases.

**A material correction, surfaced during this study's own Phase 0:**
`claude/modest-dirac-a3d7pw` had diverged from `main` *before*
AP-WIRE-024 through AP-WIRE-031 landed there. When this task began, the
branch's actual checked-out source had no `Wire::identity_status`, no
`WireModel::conductor_boundary_resolutions`, and no AP-WIRE-024 conflict
model at all — `ctest` on a genuine rebuild came back **42/42**, not
52/52. AP-WIRE-TUNE-001's committed report had, in fact, been produced
against a stale locally-built library left over from when the branch
mix-up (documented in that session) was corrected — a library that
still reflected `main`'s code, not the feature branch's own source.

This was reported to the user rather than silently worked around or
silently continued against the wrong (older) pipeline. Per the user's
explicit direction, `origin/main` was merged into
`claude/modest-dirac-a3d7pw` (merge commit `e379d72`), which is the only
non-experimental change to tracked source in this task, was
user-authorized, and consisted entirely of bringing in already-existing,
previously-landed `main` code — no algorithm was hand-edited. After the
merge and a full rebuild, `ctest` returned **52/52**, and the rebuilt
binary reproduced every AP-WIRE-TUNE-001 number bit-for-bit, including
the baseline's 4 AP-WIRE-024 conflict ids and the `maximum_gap` wide-sweep
transition points. `docs/AP-WIRE-TUNE-001_...md` was updated with a short
provenance note; none of its findings changed. Every experiment described
below (Phases 1–11) was run, or re-run, against the real post-merge
build. `git status --short` is clean; no file was modified to conduct
the sweep itself.

## Phase 0 — Baseline verification (post-merge)

```
Branch: claude/modest-dirac-a3d7pw
HEAD:   dc6ef3e (merge e379d72 + this task's provenance note)
origin/main: 862cd80 (untouched — the merge went into the feature
             branch, nothing was pushed to main)
```

Baseline extraction (`samples/trx300ODG.png`, all-default `ExtractionConfig`):

```
conductor_segments = 294      topology_nodes = 692     topology_edges = 877
endpoints          = 210      boundaries_resolved = 36  boundaries_conflicted = 4
physical wires      = 41      electrical_nets = 10      shared_segments = 1
gaps_bridged        = 6       validation_errors = 0     validation_warnings = 30
AP-WIRE-024 conflicted endpoints (4):
  endpoint-candidate-1fd584e37a5c72b5   (CONFLICT-01)
  endpoint-candidate-b0e3d6bb622a227c   (CONFLICT-02)
  endpoint-candidate-c033140e251b7b86   (CONFLICT-03)
  endpoint-candidate-cde07f8718a2b9cd   (CONFLICT-04)
```

`ctest`: **52/52 passing**. Baseline artifacts (raw sweep text, JSON
topology dumps, SVG renders) preserved under `/tmp/tune002/` for this
session; the durable, repo-tracked record is
`tuning/tune002_gap_finegrained_sweep.csv` and
`tuning/tune002_transitions.csv` (both new in this commit).

## Phase 1 — Exact parameter definition

| | |
|---|---|
| Name | `gap_interpretation.maximum_gap` |
| Struct | `GapInterpretationConfig` |
| Source file | `include/eke_dx_wire/topology/gap_interpreter.hpp` |
| Consuming class/function | `GapInterpreter::interpret()`, `src/topology/gap_interpreter.cpp` |
| Default | `18.0` |
| Units | pixels (Euclidean distance between two `TopologyNode` positions) |
| Type | `double` |
| UI/slider mapping | **none** — no slider or CLI flag exists anywhere in the repository; changing it requires constructing a non-default `ExtractionConfig` in code (this task used an external scratch driver, never repository source, exactly as in TUNE-001) |
| Pipeline position | invoked in `ExtractionPipeline::extract()` (`src/pipeline/extraction_pipeline.cpp`) immediately after `TopologyReconstructor::reconstruct()` and *before* `EndpointReconstructor::reconstruct()` |

### The mechanism, read directly from `gap_interpreter.cpp`

`GapInterpreter::interpret()` only considers **degree-1 nodes of type
`TopologyNodeType::ConductorEnd`**. For each such node `a` (in id order,
skipping ones already paired), it searches all other not-yet-used
degree-1 `ConductorEnd` nodes `b` and picks the *nearest* one that
satisfies **all three** of:

1. `minimum_gap <= distance(a, b) <= maximum_gap`
2. `collinear_facing(a, b, ...)` — `a` and `b` must line up on a shared
   horizontal or vertical axis (within `collinear_tolerance`, default
   1.5px) *and* each node's existing edge must point back toward the
   other node (i.e. the two conductor stubs face each other, not away)
3. the straight-line pixel corridor between `a` and `b` has ink density
   `>= minimum_ink_density` (default 0.08) — a deliberate check per the
   source's own comment: *"An annotation occupying the gap is evidence
   for a visual interruption. An empty gap is deliberately not bridged
   here."*

If a pair is found, `GapInterpreter` appends an
`inferred-continuation-edge` between them, **and mutates both nodes'
`TopologyNodeType` from `ConductorEnd` to `Continuation` in place**,
before returning.

This last step is the crux of everything this study found:
`EndpointReconstructor::reconstruct()` (`src/topology/endpoint_reconstructor.cpp:191`)
only manufactures an `EndpointCandidate` for a node that is **still**
`TopologyNodeType::ConductorEnd` with incident-edge count `== 1` *at the
time it runs* — which is after `GapInterpreter` has already run and
already flipped any bridged node's type. A node that gets bridged
therefore **cannot, structurally, ever become an `EndpointCandidate`**,
cannot enter `ConductorBoundaryResolution`, and cannot be flagged as an
AP-WIRE-024 conflict — not because the conflict was resolved by
evidence, but because the node that would have carried it was retyped
before the candidate-generation stage ever saw it.

## Phase 2 — Fine-grained sweep

The prescribed baseline±4 window (14–22) plus every TUNE-001 wide-sweep
transition point (4, 8, 12, 16, 18, 20, 24, 30, 40, 60, 100) were used as
the starting grid, then narrowed with a bisection-style search (down to
0.05px resolution) at every point a metric changed, to pin exact
thresholds rather than report "somewhere between." **89 distinct values**
of `maximum_gap` were tested in total (full table:
`tuning/tune002_gap_finegrained_sweep.csv`). All other parameters were
left at default throughout, per the one-variable-at-a-time rule.

## Phase 3 — Global metrics

Recorded for every value: conductor segments, topology nodes/edges,
endpoint candidates, conductor boundaries (resolved/conflicted), physical
wires, electrical nets, shared conductor segments, validation
errors/warnings, and `gaps_bridged`. Full table in
`tuning/tune002_gap_finegrained_sweep.csv`. SVG object counts were not
separately tracked as a numeric column — `StructuredSvgExporter` renders
directly from the already-computed `WireModel`/`EngineeringDiagram`, so
its object count is a deterministic function of the wire/net/component
counts already tabulated; five representative SVGs were rendered instead
(Phase 9).

The sweep resolves into **exactly seven stable plateaus** separated by
six sharp transitions (Phase 4 below); within each plateau, every tested
value is byte-identical to every other.

| Plateau (maximum_gap range) | wires | boundaries_conflicted | gaps_bridged |
|---|---|---|---|
| [0, 6.95] | 42 | 4 | 0 |
| (6.95, 7.00] | 42 | 4 | 5 |
| (7.00, 22] | **41 (baseline)** | 4 | 6 |
| (22, 32.00] | 41 | 4 | 7 |
| (32.00, 51.0] | 38 | 3 | 10 |
| (51.0, 99.0] | 34 | 2 | 17–21 |
| (99.0, ≥100] | 33 | **1** | 22 |

## Phase 4 — Every topology transition, with individual identities

All six transitions, forensically diffed node-by-node from paired
`TopologyExporter::export_json()` dumps taken immediately either side of
each boundary. Full table: `tuning/tune002_transitions.csv`.

**T1 (6.95 → 7.00), benign:** 5 of the baseline's 6 gap-bridges activate
simultaneously (all at real distance ≈7.0px — several printed
wire-color-label gaps in this diagram happen to share almost exactly the
same width). 10 endpoint candidates vanish (5 node pairs, e.g.
`topology-node-21975ac6cdf2fe0f`↔`topology-node-44e7b7b5cf90ce0b` at
(203,556)/(210,556)). Wire count is **unaffected** (42→42): these nodes
were already reachable within their own wires via another path, so
bridging them only adds a redundant topological edge, not new
connectivity.

**T2 (7.00 → 7.05), the TUNE-001 fragmentation fix, confirmed exactly:**
the 6th and last baseline bridge activates
(`topology-node-9f5c57ccf0042e04` (130,219) ↔
`topology-node-b10fbd7b4ae86f0a` (129.5,212), real distance ≈7.07px).
`wire-f0fdb61aea51319b` — the artifact TUNE-001 identified at
`maximum_gap=4` — disappears here exactly, confirming from the opposite
direction that this specific real conductor gap requires ≥7.05px to
bridge correctly; below it the extractor reports 42 wires instead of the
correct 41.

**T3 (22 → 23), benign:** a 7th bridge (beyond baseline's six) activates
at real distance ≈22.0px, nearly vertical
(`topology-node-30452ea98fcc36d6` (834,458) ↔
`topology-node-6ed87da1e4abb4e3` (834.5,436)). Wire count and the
AP-WIRE-024 conflict set are both unaffected — a legitimate, low-risk
continuation bridge unrelated to any of the four conflicts.

**T4 (32.00 → 32.05), first destructive transition — CONFLICT-01
suppressed:** see Phase 5/7.

**T5 (51.0 → 51.2), CONFLICT-03 suppressed:** see Phase 5/7. Note: a
second, entirely unrelated bridge
(`topology-node-20d5ce68ba9645f3` (403,324.5) ↔
`topology-node-2e8e7f9cea6306ea` (454,324), real distance ≈51.0px,
horizontal) coincidentally crosses its own threshold at almost the same
`maximum_gap` value. It is not part of any AP-WIRE-024 conflict and is
called out here specifically so it is not mistaken for part of the
CONFLICT-03 mechanism.

**T6 (99.0 → 99.1), CONFLICT-04 suppressed, last of the four:** see
Phase 5/7.

## Phase 5 — Forensic trace of all four AP-WIRE-024 conflicts

| Label | Endpoint id | Node id | Position | Boundary kind | Why conflicted at baseline |
|---|---|---|---|---|---|
| CONFLICT-01 | `endpoint-candidate-1fd584e37a5c72b5` | `topology-node-8f6a7c0b370305c5` | (869, 365) | geometric | resolved component id but conflicted boundary status; ground status resolved |
| CONFLICT-02 | `endpoint-candidate-b0e3d6bb622a227c` | `topology-node-40e232d05a45cfb6` | (161.5, 182) | component_terminal | two conflicting candidate shape-region ids |
| CONFLICT-03 | `endpoint-candidate-c033140e251b7b86` | `topology-node-b5a86df129808446` | (881.5, 365) | geometric | same resolved component id as CONFLICT-01; ground status resolved |
| CONFLICT-04 | `endpoint-candidate-cde07f8718a2b9cd` | `topology-node-f23492300e26f06a` | (584, 458) | geometric | two conflicting candidate shape-region ids; ground status resolved |

Notable baseline geometry: CONFLICT-01 and CONFLICT-03 sit only 12.5px
apart on the same horizontal line (869,365)–(881.5,365) — squarely
inside the default `maximum_gap` window — yet the baseline does **not**
bridge them to each other. Verified directly: both remain degree-1 at
every tested `maximum_gap` up to and including the value where each
individually disappears via a *different, vertical* bridge partner. This
confirms the ink-density gate is doing real work here: the horizontal
gap between them is evidence-free (an actual open gap in the diagram),
so the extractor correctly declines to bridge it, and the conflict
between two live, close, genuinely separate stub geometries is preserved
rather than papered over. This is exactly the behavior the mechanism is
supposed to have.

### Per-conflict classification across the full sweep

| Conflict | Behavior across [0, 100] | Classification |
|---|---|---|
| CONFLICT-01 | Present at all values ≤32.00; suppressed at ≥32.05 by a bridge to (868,397), 32px away, nearly vertical | **SUPPRESSED** |
| CONFLICT-02 | Present at every single tested value from 0 to 100, inclusive | **PERSISTENT** |
| CONFLICT-03 | Present at all values ≤51.0; suppressed at ≥51.2 by a bridge to (880.5,416), 51px away, nearly vertical | **SUPPRESSED** |
| CONFLICT-04 | Present at all values ≤99.0; suppressed at ≥99.1 by a bridge to (585.5,557), 99px away, nearly vertical | **SUPPRESSED** |

None of the four is ever `LEGITIMATELY_RESOLVED`, `TRANSFORMED`,
`FRAGMENTED`, or `MERGED` by this parameter in the definitions' technical
sense — three are `SUPPRESSED` outright and one is `PERSISTENT`
throughout. **Zero of the four conflicts are ever resolved by physical
evidence becoming sufficient; every apparent "resolution" is
node-retyping that removes the node from candidacy before the conflict
machinery runs.**

## Phase 6 — Merge vs. bridge, applied to every transition

| Transition | Real distance | Axis | New Wire objects created? | Verdict |
|---|---|---|---|---|
| T1 (5 bridges) | ≈7.0px | vertical | No (0 wires added/removed) | genuine bridge — reconnects an already-continuous wire's own path |
| T2 (fragmentation fix) | ≈7.07px | vertical | wire count 42→41 (one absorbed) | genuine bridge — this is the correct behavior; below this threshold the extractor is wrong (fragmented) |
| T3 | ≈22.0px | vertical | No | genuine bridge, no new false connectivity |
| T4 (CONFLICT-01) | ≈32.0px | vertical | 2 wires disappear, 0 appear | **destructive merge** — see below |
| T5 (CONFLICT-03) | ≈51.0px | vertical | 1 wire disappears | **destructive merge** |
| T6 (CONFLICT-04) | ≈99.0px | vertical | 1 wire disappears | **destructive merge** |

A clear pattern falls out of this table that the handoff's own
"useful bridge vs. destructive merge" diagram anticipated almost
exactly: **every genuinely safe bridge in this diagram sits at ≤22px**,
and **every bridge that suppresses a named engineering conflict sits at
≥32px** — a factor of at least 1.5–2× beyond the last safe bridge, and
up to 5.5× the default `maximum_gap` for the worst case (CONFLICT-04 at
99px). All three destructive merges are near-perfectly vertical
(≤1.5px horizontal offset, i.e. exactly at the edge of
`collinear_tolerance`), which combined with their length is much more
consistent with the ink-density gate being satisfied by *something else
in the vertical corridor* — a crossing conductor, a component symbol, a
second nearby printed label — than by a single compact wire-color label
of the kind the mechanism's comment describes as its intended target. No
component-adjacency check independent of the raw pixel corridor exists
in `GapInterpreter` to distinguish these cases; this is a real
instrumentation gap (Phase 12 §19), not something this task's
constraints permit fixing.

## Phase 7 — Special attention to `maximum_gap = 100`, mechanism-level

Reproduced exactly (33 wires, 1 conflict, matching TUNE-001). Answering
the handoff's specific mechanism checklist for each of the three
disappeared endpoints (`1fd584e37a5c72b5`, `c033140e251b7b86`,
`cde07f8718a2b9cd`):

- **Was the endpoint merged?** No — `EndpointCandidate` objects are not
  merged with each other; the *underlying topology node* was retyped.
- **Was the conductor removed?** No — the conductor segment geometry is
  untouched; only the node's role classification changed.
- **Was the boundary absorbed?** No absorption concept exists here — the
  node never reaches `ConductorBoundaryResolution` at all once retyped,
  so there is nothing to absorb; it simply never appears as an
  `EndpointCandidate` for that stage to see.
- **Did two candidates become one?** No — one candidate (the conflicted
  one) disappears; its bridge partner, if it was itself a
  `ConductorEnd`, also disappears from the candidate list, but as two
  separate deletions, not a merge into a surviving third id.
- **Did a contour disappear?** No — this is topology-graph logic, not
  contour/shape detection; `shapes.contour_threshold` is untouched.
- **Did topology become connected?** Yes — this is the actual mechanism:
  an `inferred-continuation-edge` is added and both endpoint nodes'
  `TopologyNodeType` flips from `ConductorEnd` to `Continuation`.
- **Did the endpoint simply fail a threshold?** No — the endpoint
  candidate itself never failed anything; it was never generated,
  because its node no longer qualified as an eligible input at the time
  `EndpointReconstructor` ran.
- **Did downstream reconstruction stop seeing it?** Yes, precisely: this
  is the correct framing. `EndpointReconstructor` filters on
  `node.type == ConductorEnd && degree == 1`; `GapInterpreter` runs
  first and already disqualified the node from that filter.

**Explicit verdict for `maximum_gap = 100`: this is EVIDENCE SUPPRESSION,
not actual improvement, not a documented alternative mechanism.** The
apparent "resolution" of 3 of the 4 AP-WIRE-024 conflicts is a byproduct
of upstream node-retyping order, not of the conductor-boundary or
wire-identity machinery concluding anything about physical continuity.
CONFLICT-02 surviving even at gap=100 is not because it is somehow a
"harder" conflict — it is simply geometrically isolated enough (no
collinear partner within 100px passing the ink-density gate) that it
never gets the chance to be suppressed the same way; there is no reason
to expect it to remain safe at a still-larger `maximum_gap`.

## Phase 8 — Safe operating region

- **LOWER SAFE BOUND: `maximum_gap` ≈ 7.05px.** Strictly below this,
  `wire-f0fdb61aea51319b` fragments off a real, continuous conductor
  (DESTRUCTIVE — valid topology lost, per T2). At or above it, no
  fragmentation is observed anywhere in the sweep.
- **UPPER SAFE BOUND: `maximum_gap` ≈ 32.0px.** Strictly above this,
  CONFLICT-01 is suppressed (T4) — the first of three destructive,
  evidence-erasing merges found in this sweep. At or below it (down to
  the lower bound), no destructive merge, no unexplained endpoint loss
  beyond the two benign, wire-count-neutral bridges (T1, T3), and no new
  false topology were observed.
- **The shipped default, 18.0, sits inside this safe region**, roughly
  in its lower third — not perched near either edge.
- No CTest regression was introduced by any tested value (52/52 held
  throughout, since no source was changed to run the sweep); validation
  error count was 0 at every single tested value from 0 through 100.

This safe region is **narrower and more precisely bounded** than
TUNE-001's looser "plateau 8–~22, destructive by 30–100" characterization
— TUNE-001 correctly identified the shape of the danger but this study
pins its edges to within 0.05px.

## Phase 9 — Visual forensics

Five SVGs were rendered via `EngineeringDiagramBuilder` +
`StructuredSvgExporter` (both already-existing, unmodified renderer
classes) for: baseline (`maximum_gap=18`), first fragmentation
(`=4`), first topology change beyond the baseline plateau (`=23`), first
destructive merge (`=32.05`), and `=100`. These are working artifacts
under `/tmp/tune002/svg/` (not part of the repository, per the same
convention as TUNE-001's topology JSON dumps) and are offered to the user
separately rather than committed, since the engineering topology JSON —
already fully forensically traced above — remains the authoritative
record per this task's own instruction ("do not judge them only
visually").

## Phase 10 — No source changes during experimentation

Every one of the 89 sweep values, all six transition dumps, and all five
SVG renders were produced by the external scratch driver (`/tmp/tuning_driver.cpp`,
not part of this repository) calling `ExtractionPipeline` with a
non-default `GapInterpretationConfig.maximum_gap` — a pre-existing
constructor parameter. `git status --short` is clean after every phase
of this task except the two commits described in §0 (the merge and its
provenance note), which were explicitly scoped, user-approved, and
touched no algorithm by hand. `AP-WIRE-031`, the wire model, boundary
resolver, electrical-net resolver, semantic resolver, and validator were
not opened for editing at any point.

## Phase 11 — Regression

```
$ cmake --build build -j$(nproc)   # clean build after the merge
$ cd build && ctest
100% tests passed, 0 tests failed out of 52
```

0 new warnings attributable to this task (a handful of pre-existing
`[-Wunused-result]` warnings appear in test files brought in by the
`main` merge itself, not introduced or touched by this task).

## Phase 12 — Final report

1. **Parameter:** `gap_interpretation.maximum_gap`, `GapInterpretationConfig::maximum_gap`,
   `include/eke_dx_wire/topology/gap_interpreter.hpp`, consumed by
   `GapInterpreter::interpret()`.
2. **Baseline value:** `18.0` px.
3. **Fine-grained sweep values:** 89 distinct values from 0 to 100,
   concentrated at baseline±4 and bisected to 0.05px resolution at every
   transition boundary. Full table: `tuning/tune002_gap_finegrained_sweep.csv`.
4. **Global metric table:** `tuning/tune002_gap_finegrained_sweep.csv`.
5. **Every topology transition:** 6 transitions, Phase 4 / `tuning/tune002_transitions.csv`.
6. **Individual Wire/Endpoint/Boundary/ConductorSegment changes:** given
   per-transition in Phase 4, with exact node ids and positions.
7. **All four AP-WIRE-024 conflicts:** traced individually in Phase 5 —
   1 `PERSISTENT` (CONFLICT-02), 3 `SUPPRESSED` (CONFLICT-01/03/04), 0
   `LEGITIMATELY_RESOLVED`.
8. **`maximum_gap=100` forensic analysis:** Phase 7 — confirmed
   mechanism is node-retype-before-candidate-generation; verdict is
   **evidence suppression**, not improvement.
9. **First fragmentation threshold:** `maximum_gap` ≈ 7.00–7.05px (T2).
10. **First meaningful topology-change threshold beyond the baseline
    plateau:** `maximum_gap` ≈ 22–23px (T3, benign).
11. **First destructive-merging threshold:** `maximum_gap` ≈ 32.00–32.05px (T4).
12. **Lower safe bound:** ≈7.05px.
13. **Upper safe bound:** ≈32.0px.
14. **Does any value legitimately resolve an AP-WIRE-024 conflict?**
    **No.** Across the full 0–100px range, all three conflicts that
    disappear do so by suppression (node retyped out of candidacy), never
    by physical evidence becoming sufficient while the endpoints remain
    represented. Zero `LEGITIMATELY_RESOLVED` classifications were found.
15. **Does the 41-wire baseline remain intact?** **Yes** — reproduced
    exactly (41 wires, same 4 conflicted ids) on the honest, post-merge,
    freshly rebuilt binary, matching TUNE-001's number bit-for-bit.
16. **CTest result:** 52/52 passing (post-merge, fresh build).
17. **Build result:** clean Release build, 0 warnings introduced by this
    task.
18. **Files/artifacts generated:** `tuning/tune002_gap_finegrained_sweep.csv`,
    `tuning/tune002_transitions.csv` (both committed); raw sweep logs,
    12 topology JSON dumps, and 5 SVG renders under `/tmp/tune002/`
    (working artifacts, not committed, per the same convention as
    TUNE-001).
19. **Instrumentation limitations:** `GapInterpreter` has no
    component-adjacency or net-role check independent of the raw pixel
    ink-density corridor — it cannot distinguish "a printed label sits in
    this gap" from "a crossing conductor or a component symbol happens to
    darken this long corridor," which is the most plausible explanation
    for why all three destructive merges are long, near-perfectly
    vertical bridges. Confirming that explanation pixel-for-pixel against
    the source raster (rather than inferring it from topology alone) was
    not done in this task and would need either a raster-overlay tool or
    manual inspection of the source image at the six bridge corridors
    identified in Phase 4/6 — flagged here rather than silently assumed.
20. **Recommendation for AP-WIRE-TUNE-003:** characterize
    `topology.snap_tolerance` and the two `morphology.*_kernel_length`
    parameters with this same bisection-to-threshold methodology —
    TUNE-001 already flagged `vertical_kernel_length` as the single most
    consequential parameter in the whole inventory, and it has not yet
    received this level of forensic precision. Separately, the
    ink-density corridor check identified in item 19 above is worth a
    dedicated instrumentation task (not a "fix," since the current
    behavior is exactly what the source comment describes and this task
    was not chartered to redesign it) to determine, per bridge, what
    specifically is producing the qualifying ink — before any future task
    considers whether the default `maximum_gap` should ever change.

**No production slider value is recommended.** The evidence establishes
a safe *region* (≈7.05–32.0px) within which the default already sits
comfortably; it does not establish that any other value in that region
is better than the shipped default, and per this task's charter that
question was never asked.

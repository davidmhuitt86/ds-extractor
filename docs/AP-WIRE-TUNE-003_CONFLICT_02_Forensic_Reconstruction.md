# AP-WIRE-TUNE-003 — CONFLICT-02 Forensic Reconstruction

## Status

**Forensic reconstruction. No algorithm modified, no fix implemented.**
This task determines exactly why AP-WIRE-024's CONFLICT-02 persists
across the entire `gap_interpretation.maximum_gap` range tested in
AP-WIRE-TUNE-002 (0–100px), while CONFLICT-01/03/04 were all eventually
suppressed. Neither `AP-WIRE-024` nor `AP-WIRE-031` was modified.
`gap_interpretation.maximum_gap` was left at its default throughout —
this task's own parameter tests (Phase 5) touched only
`terminals.boundary_tolerance` and `terminal_recognition.*`, each in a
small ±1-increment neighborhood, per the task's explicit instruction not
to run a broad sweep.

## Phase 0 — Baseline integrity

```
Branch: claude/modest-dirac-a3d7pw
HEAD:   4528920 (AP-WIRE-TUNE-002)
Merged main state: e379d72 (confirmed still an ancestor of HEAD)
```

Full rebuild produced no new object files (tree already up to date from
TUNE-002's session); `ctest`: **52/52 passing**.

Baseline extraction (`samples/trx300ODG.png`, all-default `ExtractionConfig`):

```
gap_interpretation.maximum_gap = 18.0 (default, unchanged)
physical wires = 41
AP-WIRE-024 conflicted endpoints = 4:
  endpoint-candidate-1fd584e37a5c72b5   (CONFLICT-01)
  endpoint-candidate-b0e3d6bb622a227c   (CONFLICT-02)  <- this task
  endpoint-candidate-c033140e251b7b86   (CONFLICT-03)
  endpoint-candidate-cde07f8718a2b9cd   (CONFLICT-04)
```

CONFLICT-02 confirmed present, unchanged from every prior AAR in this
project.

## Phase 1 — Exact identity and dependency chain

```
endpoint-candidate-b0e3d6bb622a227c
  node_id: topology-node-40e232d05a45cfb6 @ (161.5, 182)
  conductor-boundary-resolution-3119f42d457ef725
    boundary_kind: component_terminal
    boundary_status: conflicted
    conflicting_component_ids:
      component-candidate-shape-region-845947b0afd7451a   ("candidate A")
      component-candidate-shape-region-c98f6566b0672535   ("candidate B")
    evidence_ids:
      conductor-boundary-evidence-95a5d91c48d1042e   (-> candidate A, High)
      conductor-boundary-evidence-e93195f848e2d097   (-> candidate B, Low)
      conductor-boundary-evidence-1bd4f322aaf0f68f   (endpoint-semantic-reconstruction, unresolved, no opinion)
  wire-430f59213afaef3a
    identity_status: resolved   <- AP-WIRE-031 is unaffected by this conflict
    start_endpoint: endpoint-candidate-ab79957523aeb954 @ (161.5, 157)
    end_endpoint:   endpoint-candidate-b0e3d6bb622a227c @ (161.5, 182)
    topology_edges: [topology-edge-b451366a8529b321]
    conductor_segments: [normalized-conductor-segment-536e21203510a354]
```

No electrical net, connector, or terminal name is involved — this
endpoint's boundary evidence never reaches connector/terminal-identifier
resolution; it stops at the component-association step. Full record:
`tuning/tune003_conflict02_forensic_record.json`.

## Phase 2 — Reconstructing every candidate's physical path

There is **exactly one topological path** here, not two competing ones.
The wire (`wire-430f59213afaef3a`) is a single, straight, 25px vertical
conductor: one topology edge, one conductor segment, two endpoints, no
splice, no crossing, no branch. AP-WIRE-031 correctly reports this
wire's `identity_status` as **Resolved** — the physical-wire-identity
layer has no ambiguity whatsoever and is untouched by anything below.

What genuinely differs between "candidate A" and "candidate B" is not
the wire's path — it is which **real-world component** the wire's
*already-settled* lower endpoint terminates at:

| | Candidate A | Candidate B |
|---|---|---|
| Component id | `...845947b0afd7451a` | `...c98f6566b0672535` |
| Component bounds | (145,158) 18×24 | (158,173) 20×21 |
| Symbol kind | `circular_symbol` (geometrically classified only) | `circular_symbol` (geometrically classified only) |
| Evidence source | `TerminalLocationDetector` (`terminal_candidate`) | `TerminalRecognizer` (`terminal_recognition`, TerminalLead path) |
| Distance to endpoint | 0px (endpoint sits exactly on this box's bottom edge) | ~0px (endpoint sits on/inside a small internal primitive owned by this component) |
| Confidence | **High** (0px ≤ `terminals.high_confidence_distance`=2.5px) | **Low** (capped by the *primitive's own* detection confidence, not by distance) |
| Internal primitives this component reported | **0** | 2 (`symbol-primitive-c9bca4105e248446` @ (160,175)-(163,183); `symbol-primitive-fcd5261d49abac82` @ (173,175)-(176,183)) |

Both are geometrically excellent matches (distance ≈0px). Neither is
supported by any recognized label text, wire color, or connector/terminal
identifier — `text_recognition_evidence` is empty for this entire run
(the default no-op `TextRecognitionProvider` was in effect; see Phase 7).

## Phase 3 — Conflict type classification

**Primary category: G — Component/connector boundary ambiguity.**

Explicitly ruled out by the Phase 2 reconstruction:
- **not** B (multiple plausible conductor *paths*) — there is exactly
  one path, one edge, one segment; nothing about the wire's own routing
  is in question.
- **not** E/F (crossing or splice/junction ambiguity) — this is an
  ordinary degree-1 wire endpoint, not a junction node.
- **not** D (incorrect segmentation) — the conductor segment's own
  geometry is not disputed by either candidate.
- **not** A (duplicate endpoint evidence) — there is one endpoint, one
  node; the dispute is about which component it touches, not about how
  many endpoints exist.

This is supported by evidence, not asserted from the code path alone:
Phase 4 traces exactly how each of the two candidate evidence records
was produced and shows both point at a real, independently-detected
component shape a few pixels from the endpoint.

## Phase 4 — Tracing back to raster geometry

A crop of the source raster around this endpoint (`(161.5,182)`, ±35px)
shows a row of three closely-packed fuse-style symbols (circle-topped
rectangles) connected along a shared bus at top, with two of the three
converging into a single wire at bottom via a visible V-shaped splice.
The ambiguous endpoint sits at the bottom-left corner of the **middle**
symbol's rectangle body, exactly where its own exit lead begins.

**The two competing components are two distinct, real, separately-drawn
symbols — not one shape mistakenly split in two.** Their bounding boxes
are offset diagonally (A: (145,158)-(163,182); B: (158,173)-(178,194)) and
overlap only in a small 5×9px corner (158–163, 173–182) — the signature
of two adjacent real objects placed close together, not a single blob
double-detected (which would produce near-identical or nested boxes,
not a small diagonal-corner overlap).

**The ambiguity itself, however, is not simply "two components are
close together" — it has a specific, locatable mechanical cause:**

`SymbolGeometryExtractor::extract()` (`src/image/symbol_geometry_extractor.cpp`,
≈lines 135–250) processes each `ComponentCandidate`'s bounding box as an
**independent** cropped ROI, thresholds it, and reports every connected
blob inside as an internal `SymbolPrimitive` **owned by that component**.
Each component's own boundary stroke is deliberately masked out via a
margin exclusion so a component's own outline is never mistaken for an
internal primitive (`symbol_geometry_extractor.cpp:164-174`).

Candidate B's disqualifying evidence — `symbol-primitive-c9bca4105e248446`,
bounds (160,175)-(163,183) — is **the last ~7 pixels of `wire-430f59213afaef3a`'s
own vertical stroke**, immediately above where it terminates at the
disputed endpoint. It is not a separate pin or lead glyph: it occupies
exactly the pixels the wire itself is drawn on, right where the wire
exits component A's rectangle. Component A's own extraction correctly
excluded this same ink as its own boundary/exit stroke (margin masking
worked as designed — **A reports zero internal primitives**, confirmed
directly in the dump). But because component B's bounding box
independently overlaps that exact corner, B's own margin exclusion
(computed relative to B's own edges, not A's) does **not** mask it, so
B's ROI scan picks up the same ink as if it were B's own internal
terminal lead.

**Earliest stage where the ambiguity first exists:** component/symbol
geometry extraction — specifically the moment `SymbolGeometryExtractor`
crops component B's ROI and finds a blob at (160,175)-(163,183) without
checking whether that ink is already accounted for as a neighboring
component's own boundary/exit stroke. Every later stage (`TerminalRecognizer`,
`ConductorBoundaryResolver`, AP-WIRE-024 conflict reporting) is behaving
correctly *given the evidence it receives* — none of them invents,
mis-thresholds, or mis-links anything on their own account.

## Phase 5 — Parameter sensitivity (small local neighborhoods only)

Per the task's instruction, only a very small neighborhood (±1 meaningful
increment) around each plausibly-relevant parameter was tested — no
broad sweep.

| Parameter | Values tested | Effect on CONFLICT-02 |
|---|---|---|
| `terminals.boundary_tolerance` (default 8.0, incr 0.4) | 7.6, 8.0, 8.4 | **No change** — persists identically at all three (7.6 loses an unrelated wire elsewhere) |
| `terminal_recognition.terminal_lead_max_distance` (default 6.0, incr 0.5) | 5.5, 6.0, 6.5 | **No change** — persists identically at all three |
| `terminal_recognition.aligned_boundary_max_distance` (default 16.0, incr 0.8) | 15.2, 16.0, 16.8 | **No change** to CONFLICT-02 (16.8 introduces an unrelated new conflict elsewhere, geometrically unconnected to this endpoint) |
| `terminal_recognition.minimum_alignment_cosine` (default 0.85, incr 0.04) | 0.81, 0.85, 0.89 | **No change** — persists identically at all three |

None of the four parameters plausibly touching this conflict's producing
mechanisms moves it at all in a small local neighborhood — consistent
with both candidates' underlying distances being ≈0px, far from any of
these thresholds' boundaries. **CONFLICT-02 is not parameter-sensitive**
in the sense TUNE-002 found `gap_interpretation.maximum_gap` to be for
CONFLICT-01/03/04. No range expansion was warranted (per the task's own
"only expand if the result changes" instruction) since nothing changed
at the first neighborhood tested.

## Phase 6 — Real resolution vs. suppression

**Not applicable.** No tested parameter caused CONFLICT-02 to disappear,
so there is nothing to distinguish. This absence is itself informative:
unlike CONFLICT-01/03/04 (each suppressed once `maximum_gap` grew large
enough to retype its node before candidate generation), CONFLICT-02's
root cause is not a topology-graph node-retyping event at all — it is a
component-boundary evidence conflict produced entirely upstream of, and
independently of, gap interpretation. There is no plausible small
parameter change that would make it disappear without either (a)
suppressing evidence generally (e.g., raising `terminal_lead_max_distance`
enough to stop the low-confidence path from firing at all, which would
be evidence suppression, not resolution, and was not observed to occur
in the tested neighborhood anyway), or (b) fixing the actual defect
identified in Phase 4.

## Phase 7 — Source-diagram determination

Checked explicitly, per the task's list, for a distinguishing feature
the source diagram itself provides:

- **Labels:** `text_recognition_evidence` is **empty** for this run — the
  driver used the default no-op `TextRecognitionProvider` (AP-WIRE-008's
  injectable boundary), so no OCR content was ever produced in this or
  any prior AP-WIRE-TUNE session. Only geometric `TextRegion` boxes and
  their raw pixel distance to each component exist (`semantic_associations`,
  `relation: label_to_component`) — one nearby text region sits 4px from
  candidate A and 19px from candidate B, but its actual printed content
  was never recognized, so it cannot be used as evidence without
  guessing what it says. This is flagged as a genuine instrumentation
  gap, not exploited to break the tie.
- **Wire color:** empty on this endpoint candidate.
- **Line thickness, ground symbol, explicit junction/splice marking,
  crossing convention, connector pin identity:** none present or
  applicable to this specific endpoint — it is an ordinary degree-1
  wire end, not a junction or connector pin.
- **Geometric separation:** the two components are real, distinct,
  closely-packed shapes (Phase 4); their proximity is exactly what
  produces the ambiguity, not a feature that resolves it.

No engineering notation in the (non-OCR) evidence available to this run
distinguishes candidate A from candidate B. **Absent the defect
identified in Phase 4, this would still be a legitimate case for
`Conflicted`** — the diagram's raw geometry alone, without recognized
text, does not by itself prove which of two adjacent components a
wire's exit point belongs to. The forensic value of Phase 4's finding is
that it identifies a *specific, fixable reason one of the two candidates'
evidence is spurious* — not that the ambiguity is unresolvable in
principle.

## Phase 8 — AP-WIRE-029 compliance check

**Compliant.** `ConductorBoundaryResolver::resolve()`
(`src/topology/conductor_boundary_resolver.cpp:140-151`) builds
`distinct_components` from every `TerminalCandidateKind::ComponentBoundary`
evidence record regardless of confidence, and reports `Conflicted` the
instant more than one distinct component id appears — with **no
confidence-based tie-break of any kind**. This is consistent with the
same resolver's explicit "never pick one" comment for the analogous
multi-connector case (line 226). Splice, junction, crossing, and
continuation semantics are untouched: this endpoint is none of those,
and the wire's own AP-WIRE-031 `identity_status` (`Resolved`) is
correctly unaffected by the boundary-level conflict at its far end —
the two layers are cleanly decoupled, exactly as AP-WIRE-029/031 intend.

## Phase 9 — No algorithm modification

`AP-WIRE-024`, `AP-WIRE-029`, `AP-WIRE-030`, `AP-WIRE-031`,
`ElectricalNetResolver`, `PhysicalWireIdentityReconstructor`, and
`WireSemanticResolver` were not opened for editing. No new heuristic,
proximity pairing, shortest-path selection, collinearity pairing,
color-based identity, or topology guessing was introduced. The upstream
defect identified in Phase 4 (`SymbolGeometryExtractor`) is reported,
not fixed, per this task's explicit charter.

## Phase 10 — Forensic artifact

Machine-readable record: `tuning/tune003_conflict02_forensic_record.json`
(committed). Source-raster crop preserved for visual corroboration at
`/tmp/tune003/crop_conflict02_8x.png` (working artifact, sent to the
user directly, not committed — same convention as prior AP-WIRE-TUNE
sessions' raster/SVG artifacts). Full baseline topology dump:
`/tmp/tune003/baseline.json` (not committed — reproducible from the
committed CSVs/records plus the unmodified pipeline).

## Phase 11 — Final determination

# UPSTREAM EXTRACTION DEFECT

`SymbolGeometryExtractor::extract()` in
`src/image/symbol_geometry_extractor.cpp` (per-component internal-primitive
scan, ≈lines 135–250) crops and thresholds each `ComponentCandidate`'s
bounding box **independently**, with no check for whether that box
overlaps a neighboring `ComponentCandidate`'s box. Component A's own
boundary-margin exclusion correctly identifies and discards the wire's
own exit-stroke ink as "the component's own boundary, not an internal
primitive" (confirmed: A reports **zero** internal primitives). Because
component B's bounding box independently overlaps that exact same
corner of ink (a genuine, small, real overlap between two distinct,
closely-packed symbols — not a raster preprocessing or line-detection
artifact), B's own margin exclusion does not remove it, and B's ROI scan
reports it as `symbol-primitive-c9bca4105e248446`, a spurious "internal
terminal lead" — which `TerminalRecognizer` then correctly, mechanically
turns into terminal-association evidence for B, which
`ConductorBoundaryResolver` then correctly, mechanically reports as a
conflict against candidate A's independently well-supported (0px,
High-confidence) claim.

Every stage downstream of `SymbolGeometryExtractor` — `TerminalRecognizer`,
`ConductorBoundaryResolver`, and the AP-WIRE-024 conflict report itself —
is functioning exactly as designed, on evidence that is itself flawed at
its origin. This is why the determination is an **upstream** extraction
defect rather than a boundary-resolution or topology-construction defect:
the resolver's refusal to arbitrate between two distinct components is
correct behavior; the problem is that one of the two components should
never have received this evidence in the first place.

**This is not "source-diagram ambiguity."** The diagram itself draws two
real, separate, adjacent components; the false evidence is a specific,
locatable software behavior (independent, non-overlap-aware per-component
ROI scanning), not an inherent property of the drawing. No fix was
implemented in this task, per its charter — the proposed correction
(exclude or de-duplicate ink lying in the geometric overlap between two
`ComponentCandidate` boxes before attributing an internal primitive to
either) is recorded here and in
`tuning/tune003_conflict02_forensic_record.json` for a future task.

## Phase 12 — Regression

```
$ cd build && ctest
100% tests passed, 0 tests failed out of 52
```

0 new warnings. `git status --short` was clean before this task's
commits and contains only new files (this report, the JSON forensic
record) — no existing file was modified.

## Final report

1. **HEAD/branch:** `claude/modest-dirac-a3d7pw` @ `4528920` at task
   start (this task's own commit follows).
2. **Baseline metrics:** 41 physical wires, 4 AP-WIRE-024 conflicts,
   `gap_interpretation.maximum_gap = 18.0` (unchanged), 52/52 CTest.
3. **CONFLICT-02 identity:** `endpoint-candidate-b0e3d6bb622a227c`,
   node `topology-node-40e232d05a45cfb6` @ (161.5, 182),
   `conductor-boundary-resolution-3119f42d457ef725`.
4. **All candidate identifiers:** candidate A =
   `component-candidate-shape-region-845947b0afd7451a`; candidate B =
   `component-candidate-shape-region-c98f6566b0672535`; source primitive
   = `symbol-primitive-c9bca4105e248446`. Full chain in Phase 1.
5. **Candidate physical paths:** one shared physical path —
   `wire-430f59213afaef3a`, `identity_status: resolved`, a single
   25px vertical conductor; the dispute is over component/terminal
   attachment, not routing (Phase 2).
6. **Conductor segments:** `normalized-conductor-segment-536e21203510a354`
   (the sole segment involved).
7. **Boundaries:** `conductor-boundary-resolution-3119f42d457ef725`
   (Conflicted), with 3 evidence records (Phase 1).
8. **Earliest ambiguity stage:** component/symbol geometry extraction
   (`SymbolGeometryExtractor::extract()`), well before gap interpretation,
   terminal recognition, or boundary resolution ever run (Phase 4).
9. **Parameter tests performed:** 4 parameters, 3 values each (±1
   meaningful increment), no broad sweep (Phase 5).
10. **Parameter effects:** none of the 4 tested parameters changes
    CONFLICT-02 at all in its local neighborhood (Phase 5).
11. **Real resolution vs. suppression:** not applicable — nothing made
    the conflict disappear (Phase 6).
12. **Source-diagram evidence:** none available in this run distinguishes
    the two candidates (no recognized text, no wire color, no explicit
    junction marking) — Phase 7.
13. **AP-WIRE-029 compliance:** confirmed compliant; no confidence-based
    tie-break exists or was added; the wire's own AP-WIRE-031 identity
    is correctly unaffected (Phase 8).
14. **Final determination:** **UPSTREAM EXTRACTION DEFECT** in
    `SymbolGeometryExtractor::extract()` (Phase 11).
15. **Recommended next task:** a dedicated, narrowly-scoped fix task for
    `SymbolGeometryExtractor` to exclude or de-duplicate ink inside the
    geometric overlap between two `ComponentCandidate` bounding boxes
    before attributing an internal primitive to either — then re-run
    this exact forensic reconstruction to confirm CONFLICT-02 either
    resolves legitimately (if the spurious low-confidence evidence was
    the sole cause) or continues to be reported Conflicted for a
    different, still-standing reason.
16. **Build result:** clean; no rebuild needed (tree already current
    from AP-WIRE-TUNE-002).
17. **CTest result:** 52/52 passing.
18. **Files/artifacts changed:** this report and
    `tuning/tune003_conflict02_forensic_record.json` (both new,
    committed); no existing file modified.

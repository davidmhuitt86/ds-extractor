# AP-WIRE-031 — After-Action Report

## 1. Baseline commit

`4db7b40` (AP-WIRE-030 implementation AAR) was the last commit before
this AP began. Authoritative architecture inputs: `a4d0c25`
(AP-WIRE-029 final), `9c5d6fc`/`a4d0c25` (AP-WIRE-030 spec/corrections),
`4db7b40` (AP-WIRE-030 implementation).

## 2. Implementation commit(s)

- `344ba53` — "AP-WIRE-031: extend Wire with physical identity status"
  (user-approved model extension only).
- `feec402` — "AP-WIRE-031: implement physical wire identity
  reconstruction" (`PhysicalWireIdentityReconstructor` + pipeline
  wiring + `WireReconstructor` doc-comment update).
- `ff52146` — "AP-WIRE-031: export wire identity status and evidence"
  (`topology.json` export).
- `8c61417` — "AP-WIRE-031: add regression tests" (13-case test suite).
- `4939b14` — "DX-REVIEW: regenerate extraction review after
  AP-WIRE-031" (real content change: wire count 40→41).
- This AAR is the closing commit.

## 3. Files changed

`include/eke_dx_wire/core/model.hpp`,
`include/eke_dx_wire/topology/physical_wire_identity_reconstructor.hpp`
(new), `src/topology/physical_wire_identity_reconstructor.cpp` (new),
`include/eke_dx_wire/topology/wire_reconstructor.hpp` (doc comment
only), `src/pipeline/extraction_pipeline.cpp`,
`src/export/topology_exporter.cpp`, `CMakeLists.txt`,
`tests/test_physical_wire_identity_reconstructor.cpp` (new), plus the
regenerated `artifacts/extraction_review/*` files. No other AP's
implementation file was touched.

## 4. Exact model changes

Per the user-approved minimal extension (§2 of the handoff), added to
`include/eke_dx_wire/core/model.hpp`:

```cpp
enum class WireIdentityStatus {
    Resolved,
    Unresolved,
    Conflicted
};
```

`Wire` gained exactly two new fields (no existing field renamed, removed,
or altered):

```cpp
WireIdentityStatus identity_status = WireIdentityStatus::Unresolved;
std::vector<std::string> identity_evidence_ids;
```

No larger generic status framework was introduced.

## 5. AP-WIRE-031 implementation architecture

Decision made per §15 of the handoff, from the existing repository
architecture rather than a redesign: **introduce a new stage
(`PhysicalWireIdentityReconstructor`) that becomes the pipeline's sole
authoritative Wire producer, using the existing `WireReconstructor`
internally as a building block rather than duplicating its logic.**

```
                    SOURCE GEOMETRY
                          |
                       TOPOLOGY
                          |
              +-----------+-----------+
              |                       |
              v                       v
      CONDUCTOR BOUNDARY       ELECTRICAL CONNECTIVITY
       RESOLUTION (030)            RESOLUTION (022)
              |                       |
              v                       |
   PHYSICAL WIRE IDENTITY             |
      RECONSTRUCTION (031)            |
              |                       |
              +-----------+-----------+
                          |
                  ENGINEERING MODEL
```

`PhysicalWireIdentityReconstructor::reconstruct()` runs two passes:

1. **Pass 1 (unchanged)**: calls `WireReconstructor::reconstruct()`
   exactly as before — the conservative degree-1 → degree-2-Continuation*
   → degree-1 walk. Every resulting wire is annotated
   `identity_status = Resolved` (the path is structurally unambiguous by
   construction: a degree-2 node has exactly one continuation, so no
   alternative interpretation exists) with `identity_evidence_ids`
   referencing both endpoints' AP-WIRE-030 `ConductorBoundaryResolution`
   ids.
2. **Pass 2 (new)**: for every endpoint Pass 1 left with no wire,
   attempts to extend physical identity through one or more
   Splice/Junction/Crossing nodes using conductor-segment-sharing
   evidence (§7 below). Both endpoints of any resulting wire must
   additionally carry an AP-WIRE-030 `Resolved` boundary — a bare
   `GeometricConductorEnd` is never a new Wire boundary in this pass.

A mid-implementation discovery required documenting, not just coding
around: `ElectricalNetResolver` (AP-WIRE-022) already invokes
`DistributionDecomposer` internally, which is a **third, pre-existing**
Wire producer (anchor-based: a uniquely-identified Ground/
ExternalConnection endpoint plus a tree-shaped electrically-connective
component). Its wires were already being merged into `model.wires` in
`extraction_pipeline.cpp` before this AP, via an existing dedup-by-id
loop. Per the handoff's explicit "do not modify AP-WIRE-022" and "do not
modify electrical-net resolution to solve this problem," `Distribution
Decomposer`/`ElectricalNetResolver`'s own code was **not touched**.
Instead, the pre-existing pipeline merge loop (orchestration code, not
AP-022's resolver itself) was extended by a few lines to annotate any
newly-merged `net_artifacts.wires` with `identity_status = Resolved` and
evidence from the same `ConductorBoundaryResolution` lookup — ensuring
every wire in the final `model.wires`, regardless of which internal
producer created it, carries a coherent, non-default `identity_status`.
On the TRX300 baseline this path contributes exactly **0** wires
(confirmed empirically, §8), so this had zero observable effect on the
validated baseline — it closes a latent gap for future, differently-
shaped diagrams without changing anything measurable here.

## 6. Physical continuity rules implemented

The only evidence source used to justify crossing a distribution/
crossing node: **conductor-segment sharing**. At a node with more than
two incident edges, the walk (arriving via edge E, referencing
`ConductorSegment` S) may continue only via another incident edge that
also references S. This is exactly the representation-level signal
AP-WIRE-029/031 name: the geometry-detection stage judged that stretch
of drawn line to be one continuous conductor, split into multiple
`TopologyEdge`s only because it happens to pass through/near a node. No
other evidence source (wire color, electrical-net membership, proximity,
shortest-path, straightness) is consulted anywhere in this algorithm.

- **Zero** matching edges → the walk stops there; no wire is created for
  that branch (insufficient evidence, `Unresolved` by absence — no Wire
  object is invented merely to hold an unresolved state, matching how
  the pre-existing "zero-wire endpoint" representation already works).
- **Exactly one** matching edge → the walk continues unambiguously.
- **More than one** matching edge → every one of those continuations is
  walked independently, and **every** resulting candidate wire is
  produced with `identity_status = Conflicted` (§9's Conflicted
  semantics — "independent evidence establishes incompatible physical
  identity interpretations" — genuinely occurs here: the same segment
  cannot physically continue in two directions at once, and no winner is
  ever picked).

## 7. Splice behavior

A `Splice` node is never a `Wire` endpoint at any point in this
algorithm — confirmed by construction (the algorithm only ever assigns
`start_endpoint`/`end_endpoint` from `endpoint_by_node`, which is built
exclusively from real `EndpointCandidate`s, never from a `TopologyNode`
id) and by test "3" (`!any_wire_touches(artifacts.wires, "n-splice")`).
A wire may pass through a splice only when segment-sharing evidence
justifies it (§6); the un-evidenced branch at the same splice correctly
receives no wire (test "3"/"4").

## 8. Junction behavior

Identical rule to Splice — the algorithm's branching logic at a node
with more than two incident edges does not inspect `TopologyNodeType`
at all, so `Junction` and `Splice` are handled by the exact same code
path, consistent with AP-WIRE-029 §10's "Junction follows the same
fundamental rule as a Splice." (No `Junction` node exists in the TRX300
baseline — 0 instances, per every prior AP-WIRE-028/029/030 measurement
— so this is verified by code-path identity and the synthetic tests
only, not by a TRX300-observed case.)

## 9. Crossing behavior

Also handled by the identical code path (degree > 2, segment-sharing
required to continue) — confirmed correct specifically for crossings by
test "6": two unrelated conductors crossing with no shared segment never
connect and the crossing node is never a wire endpoint, while a third,
separate conductor legitimately drawn straight through the same crossing
(sharing one segment on both sides) still forms its own wire without
ever touching the other two. No electrical connectivity is created or
implied anywhere in this algorithm — it only ever extends physical Wire
geometry, never touches `ElectricalNet`.

## 10. Continuation behavior

Unchanged from the pre-existing `WireReconstructor` behavior for Pass 1,
and handled identically (no segment-sharing check needed — a degree-2
node has only one possible next edge) inside Pass 2's own walk whenever
it re-enters a Continuation-only stretch after crossing a distribution
node. Confirmed by test "2" and by the real TRX300 wire's 14-edge path
(`continuation`, `splice`, `crossing`, and `conductor_end` node types all
appear along its single path — see §11).

## 11. Resolved / Unresolved / Conflicted behavior

- **Resolved**: every Pass-1 wire (structurally unambiguous), and every
  Pass-2 wire reached via unambiguous (exactly-one-match) segment-sharing
  evidence at every distribution/crossing node along its path, with both
  endpoints carrying an AP-WIRE-030 Resolved boundary.
- **Conflicted**: exactly the case where segment-sharing evidence is
  itself contradictory (matches ≥ 2) — demonstrated by test "8" (a single
  segment referenced by three edges at one splice; both resulting
  candidate wires come back `Conflicted`, neither preferred).
- **Unresolved**: the default on any `Wire` never explicitly assigned by
  this pipeline (none exist in the current TRX300 output — confirmed
  0/41 wires carry `identity_status: "unresolved"`, §14); represented at
  the endpoint level by the continued absence of a wire, exactly as the
  pre-existing "zero-wire endpoint" diagnostic already tracked before
  this AP (AP-WIRE-028 §9a/§10).

`Conflicted` was never used as a synonym for "not enough information" —
every `Conflicted` wire in the test suite is backed by a specific,
genuine evidence contradiction (§6), never mere absence.

## 12. Identity evidence implementation

`identity_evidence_ids` is populated only from real, pre-existing
objects: the two endpoints' `ConductorBoundaryResolution` ids (AP-WIRE-030)
and, for a Pass-2 wire, the `ConductorSegment` id(s) whose sharing
justified each distribution-node crossing along its path. No evidence
record is ever invented — where a boundary resolution isn't found for an
endpoint (shouldn't occur in the real pipeline, since AP-030 covers every
endpoint, but handled defensively), that entry is simply omitted rather
than fabricated.

## 13. Build environment

Linux sandbox, `g++ 13.3.0`, `cmake 3.28.3`. OpenCV **5.1.0**,
`/usr/local/lib/cmake/opencv5/OpenCVConfig.cmake` — unchanged from every
prior AP; `PhysicalWireIdentityReconstructor` has no OpenCV dependency
(operates purely on topology/endpoint/boundary-resolution types).

## 14. Tests added or changed

13 cases in `tests/test_physical_wire_identity_reconstructor.cpp`,
covering all 10 items §19 of the handoff required (items 3 and 5 are
covered by one combined scenario, since a three-way splice with one
evidenced branch and one un-evidenced branch is the natural, minimal way
to demonstrate both "physical continuity through a splice" and "the
un-evidenced branch gets nothing" together) plus 3 additional cases
(a bare geometric conductor end never becoming a new boundary even with
segment-sharing evidence present; determinism across repeated identical
calls; explicit `WireIdentityStatus`/`identity_evidence_ids` field
checks throughout). No existing test file was modified — all 51
pre-existing tests continue to pass unmodified, including
`test_wire_reconstructor.cpp` (the class it tests is byte-for-byte
unchanged) and `test_distribution_decomposer.cpp`
(`DistributionDecomposer` is byte-for-byte unchanged).

## 15. Full test/build result

```
Clean Release build: EXIT 0, zero new warnings (7 pre-existing,
  confirmed present before AP-WIRE-027, unrelated to this AP's files)
100% tests passed, 0 tests failed out of 52
Total Test time (real) =   0.27 sec
```

52 = 51 pre-existing (unchanged) + `dx-wire-test-physical-wire-identity-
reconstructor` (13 cases, new).

## 16. Fresh TRX300 extraction / pipeline safety (§23)

```
conductor segments: 294  (unchanged)
topology nodes:      692  (unchanged)
topology edges:      877  (unchanged)
endpoint candidates: 210  (unchanged)
electrical nets:      10  (unchanged)
validation errors:     0  (unchanged)
validation warnings:  30  (unchanged)
wires:                41  (was 40 - see below)
```

**Exactly one new wire** was discovered:
`wire-15f768907f0643e8`, spanning **14** topology edges, passing through
both a `splice` and a `crossing` node (confirmed by node-type inspection
of every edge along its path), using **3** distinct `ConductorSegment`
sharing evidences plus 2 `ConductorBoundaryResolution` ids —
`identity_status: "resolved"`. This is a single, small, fully-evidenced
discovery, not a broad reinterpretation — matching the AP-WIRE-028
prediction that only ~1 genuinely shared-conductor case exists in the
whole diagram (AP-WIRE-028 §9b's `CONDUCTOR-SHARED: 1` finding).

- **Exactly one authoritative Wire reconstruction result**: confirmed —
  `model.wires` is assigned exactly once in `extraction_pipeline.cpp`,
  from `PhysicalWireIdentityReconstructor`'s output; the old direct
  `WireReconstructor` pipeline call site no longer exists.
- **AP-WIRE-025 still consumes final Wires**: confirmed —
  `WireSemanticResolver` is called unchanged with `model.wires`;
  `wire_semantics.total` = 41, matching the new wire count exactly, no
  code change was needed in that resolver.
- **Validation still consumes final Wires**: confirmed —
  `WireModelValidator` reports `valid_wires: 41`, `errors: 0` on the new
  41-wire model; the new 14-edge wire passes every structural check
  (`WIRE-NONPATH-DEGREE`, `WIRE-DISCONNECTED-TOPOLOGY`, etc.) because
  those checks compute node degree *within* the wire's own edge subset,
  not the full topology graph — a distribution node with only 2 of its
  global-degree-3 edges included in the wire has local degree exactly 2,
  which the validator already correctly treats as an ordinary interior
  path node.
- **Electrical-net resolution remains independent**: confirmed —
  `electrical_nets: 10`, `net_roles` unchanged
  (`ground: 4, unresolved: 6`); `PhysicalWireIdentityReconstructor`'s
  signature takes no `ElectricalNet` parameter at all (structural
  guarantee, also exercised by test "9").
- **Coverage diagnostics still operate**: confirmed and the numbers
  moved exactly as expected — `conductor_segments.topology_only`
  244→241, `normal` 49→52 (the new wire's 3 newly-claimed segments),
  `shared` unchanged at 1; `topology_edges.unowned_by_any_wire`
  827→813 (−14, matching the new wire's edge count exactly).
- **Structured SVG consumers remain compatible**: confirmed —
  `output/wires.svg` renders with 643 unique element ids (was 642, +1
  for the new wire), 0 raster fallback, no code change was needed in
  `StructuredSvgExporter`/`EngineeringDiagramBuilder` (they already
  consume `Wire.topology_edges`/`conductor_segments` generically).

## 17. Determinism

Two independent fresh extractions of `samples/trx300ODG.png` produce
byte-identical `artifacts/topology/topology.json`,
`artifacts/audit/extraction_audit.json`, and `output/wires.svg`. The
walk's branching logic sorts each node's incident-edge list by `edge_id`
once, upfront, before any traversal, so branch-exploration order (and
therefore candidate-discovery order) is fully deterministic; final
`wires`/`evidence` vectors are explicitly re-sorted by `id` before being
returned.

## 18. AP-WIRE-024 conflict preservation

All 4 known conflicts
(`cde07f87...`, `b0e3d6bb...`, `c033140e...`, `1fd584e3...`) remain
`boundary_status: "conflicted"` in `model.conductor_boundary_resolutions`
— unchanged, since this AP never modifies AP-WIRE-030's output, only
reads it. This is the eighth consecutive AP (024→031) to re-verify this
regression-protection case.

## 19. Known limitations

- The conductor-segment-sharing rule is the **only** physical-continuity
  evidence implemented, per the handoff's explicit "do not implement
  speculative computer vision" — a future recognition-evidence source
  (e.g. a recognized wire-color label matching on both sides of a splice,
  itself gated by real OCR evidence, not guessed) could in principle
  justify additional cases this AP does not attempt.
- `DistributionDecomposer`'s anchor-based wire production remains
  architecturally distinct from and unintegrated with the segment-sharing
  rule this AP implements — both now coexist in `model.wires` with a
  coherent `identity_status`, but no attempt was made to unify or
  reconcile their different evidence bases, per the explicit prohibition
  on modifying AP-WIRE-022.
- The recursive branch-walk does not currently cap path length or branch
  count explicitly beyond the existing cycle guard
  (`visited_nodes`) — on pathological inputs with many chained
  ambiguous distribution nodes this could produce a combinatorial number
  of candidate outcomes. Not observed on TRX300 (which has at most a
  handful of edges sharing any one segment at any one node); a bound
  was not added since no such case exists to test against and adding an
  arbitrary cutoff without justification would itself be exactly the
  kind of unjustified heuristic this project avoids.

## 20. Git commit SHA

Final commit for this AP: pending (this AAR's own commit).
Implementation commits: `344ba53`, `feec402`, `ff52146`, `8c61417`,
`4939b14`.

## Final AP-WIRE-031 assessment

`PhysicalWireIdentityReconstructor` closes the exact gap AP-WIRE-028
measured and AP-WIRE-029/030 specified against: physical Wire identity
can now be established through a distribution or crossing node when —
and only when — explicit conductor-segment-sharing evidence justifies
it, never by graph degree, electrical-net membership, wire color, or
convenience. On the TRX300 baseline this yields exactly one new,
fully-evidenced wire (40→41) with zero disruption to every other
structural invariant, and demonstrates correct `Conflicted` behavior on
synthetic genuine-contradiction input. **AP-WIRE-031 implementation is
complete; all required tests and builds pass.**

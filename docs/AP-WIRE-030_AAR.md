# AP-WIRE-030 — After-Action Report

## 1. Baseline commit

`13b11b6`'s parent history begins at `969322f` → `69d40c6` → `48f98d3`
→ `9c5d6fc` (AP-WIRE-030 specification) → `a4d0c25` (AP-WIRE-029 final)
→ `0dca7a7` (AP-WIRE-028 corrected validation baseline, the authoritative
measurement baseline this AP is validated against).

## 2. Implementation commit(s)

- `48f98d3` — "AP-WIRE-030: add boundary resolution model additions"
  (Model-Change Gate approved addition only; see §14 for the gate
  analysis).
- `69d40c6` — "AP-WIRE-030: implement conductor boundary resolution"
  (`ConductorBoundaryResolver` + pipeline wiring).
- `969322f` — "AP-WIRE-030: add provenance/conflict coverage export"
  (`topology.json` + `extraction_audit.json` export blocks).
- `13b11b6` — "AP-WIRE-030: add regression tests" (19-case test suite).
- This AAR is the closing commit.

No pipeline/export commit beyond the three above was required — the
resolver was inserted at the single existing pipeline boundary
(immediately after `ConnectorTerminalModelBuilder` runs), and the
`topology.json`/`extraction_audit.json` exporters were the only existing
export surfaces the wire_semantics/symbol_families precedent already
established for this kind of resolution result.

## 3. Build environment

Linux sandbox, `g++ 13.3.0`, `cmake 3.28.3`. Full clean rebuild performed
from a deleted `build/` directory immediately before this AAR: `EXIT:0`,
zero errors, zero new warnings (the same 7 pre-existing warnings from
files this AP never touched — confirmed present before AP-WIRE-027 —
remain; nothing in `conductor_boundary_resolver.*`,
`extraction_pipeline.cpp`, `extraction_audit.cpp`, `artifact_writer.cpp`,
or `topology_exporter.cpp` produces a warning).

## 4. OpenCV 5.x version/path

`OpenCV_VERSION 5.1.0`, `/usr/local/lib/cmake/opencv5/OpenCVConfig.cmake`
— unchanged from every prior AP. `ConductorBoundaryResolver` has no
OpenCV dependency at all (it operates purely on `EndpointCandidate`,
`TerminalCandidate`, `EndpointSemanticReconstruction`, and
`ConnectorTerminal` — no image or geometry-detection type).

## 5. CTest result

```
100% tests passed, 0 tests failed out of 51
Total Test time (real) =   0.28 sec
```

51 = 50 pre-existing tests (unchanged) + `dx-wire-test-conductor-
boundary-resolver` (19 cases), new in this AP.

## 6. Baseline metrics (AP-WIRE-028 / commit `0dca7a7`)

```
210 endpoint candidates (174 geometric, 29 component-terminal, 7 ground,
    0 connector-terminal, 0 external-connection, 0 splice, 0 unresolved)
59 real components, 50 DiagramFurniture (109 total)
56 TerminalCandidates
0 connectors, 0 connector terminals
40 Wires, 10 electrical nets
0 validation errors, 30 warnings
```

## 7. Post-implementation metrics (fresh extraction, this AP)

```
294 conductor segments
692 topology nodes
877 topology edges
210 endpoint candidates (174 geometric, 29 component-terminal, 7 ground,
    0 connector-terminal, 0 external-connection, 0 splice, 0 unresolved)
109 component candidates (59 real, 50 DiagramFurniture)
40 Wires, 10 electrical nets
0 validation errors, 30 warnings (NET-ROLE-UNRESOLVED: 6,
    WIRE-GEOMETRIC-ENDPOINTS: 24)
```

## 8. Exact deltas

| Metric | Baseline | Post | Delta |
|---|---|---|---|
| Conductor segments | 294 | 294 | 0 |
| Topology nodes | 692 | 692 | 0 |
| Topology edges | 877 | 877 | 0 |
| Endpoint candidates | 210 | 210 | 0 |
| Components (total) | 109 | 109 | 0 |
| TerminalCandidates | 56 | 56 | 0 |
| Connectors | 0 | 0 | 0 |
| Connector terminals | 0 | 0 | 0 |
| Wires | 40 | 40 | 0 |
| Electrical nets | 10 | 10 | 0 |
| Validation errors | 0 | 0 | 0 |
| Validation warnings | 30 | 30 | 0 |

**Zero deltas across every AP-WIRE-028 baseline metric.** This is the
expected and required outcome for a pure semantic-annotation layer; it
is reported as confirmation of non-mutation, not as evidence of
"improvement" (per §32/§the handoff's explicit instruction not to report
a coverage increase as success by itself — no object count was expected
or intended to move).

## 9. Boundary-resolution coverage (new in this AP)

From a fresh extraction's `artifacts/audit/extraction_audit.json`
`conductor_boundaries` block (all 210 endpoints resolved through
`ConductorBoundaryResolver`):

| Facet | Resolved | Unresolved | Conflicted |
|---|---|---|---|
| Boundary classification | 36 | 170 | 4 |
| Component association | 31 | 177 | 2 |
| Terminal identity | 0 | 210 | 0 |
| Connector association | 0 | 210 | 0 |
| Connector-terminal/pin identity | 0 | 210 | 0 |
| Ground boundary | 10 (resolved-count only) | — | — |
| External boundary | 0 (resolved-count only) | — | — |

**Boundary-kind breakdown of the 36 resolved boundaries**: exactly 29
`ComponentTerminal` + 7 `Ground` = 36 — matching the pre-existing
`endpoint_kinds.component_terminal` (29) and `endpoint_kinds.ground` (7)
counts exactly, confirming the resolver's independent recomputation from
raw `TerminalCandidate`/`ConnectorTerminal` evidence agrees with the
existing `EndpointSemanticReconstructor`-derived `endpoint.kind` field
wherever both are defined.

**Terminal identity is 0/210 resolved** — fully expected, not a defect:
no current pipeline stage ever populates a specific terminal/pin
identifier for the `ComponentTerminal` case (`TerminalCandidate` carries
no such field, and `TerminalSemanticEvidence.terminal_name` has no
producer in this deterministic baseline — traced by full code reading
during design, §7 of the AP-WIRE-030 spec's design intent). **Connector
and connector-terminal facets are 0/210 resolved** for the same reason
AP-WIRE-028 §12 already traced: this fixture produces zero
`PrimitiveSymbol`-kind components, so the connector-materialization
chain never reaches a `ConnectorTerminal` for any endpoint. Neither
number was expected to move by implementing this AP, and neither did.

## 10. Conflict preservation

All **4** of the AP-WIRE-024 known conflicts remain `boundary_status:
"conflicted"` in the fresh extraction, with `component_id: ""` (no
winner ever selected):

| Endpoint | Component status | Ground status | Boundary status | Conflicting components |
|---|---|---|---|---|
| `endpoint-candidate-cde07f8718a2b9cd` | conflicted | resolved | **conflicted** | `320c9114...`, `64afaa2a...` |
| `endpoint-candidate-b0e3d6bb622a227c` | conflicted | unresolved | **conflicted** | `845947b0...`, `c98f6566...` |
| `endpoint-candidate-c033140e251b7b86` | resolved (`4f1e5de1...`) | resolved | **conflicted** | (cross-category conflict, see below) |
| `endpoint-candidate-1fd584e37a5c72b5` | resolved (`4f1e5de1...`) | resolved | **conflicted** | (cross-category conflict, see below) |

**A genuinely new finding, not a regression**: for two of the four
(`c033140e...` and `1fd584e3...`), `ConductorBoundaryResolver`'s
finer-grained per-category evidence shows the underlying conflict is not
"two components disagree" (as `EndpointSemanticReconstruction`'s single
collapsed status implied) but a **cross-category conflict**: exactly one
`ComponentBoundary`-kind candidate (`4f1e5de1...`, individually
consistent) *and* exactly one `GroundConnection`-kind candidate
(individually consistent) both claim the same endpoint. Both facts are
individually `Resolved` in their own right — `component_status:
resolved`, `ground_status: resolved` — while the **overall
`boundary_status` correctly stays `Conflicted`**, because AP-WIRE-030
§9's decision-matrix case 9 ("conflicting ground/component evidence")
requires exactly this: never silently pick component-over-ground or
ground-over-component. This is a more precise diagnosis of an already-
known conflict, not a new or different conflict, and not a resolution of
it — the endpoint is exactly as unresolved-to-a-single-truth as it was
before this AP.

No conflict was converted to a resolution; no resolution was converted
to a conflict; the conflicted-endpoint set is byte-identical
(`{cde07f87..., b0e3d6bb..., c033140e..., 1fd584e3...}`) to every prior
AP's re-verification of this same case.

## 11. Unresolved evidence

170/210 endpoints (81%) have `boundary_status: unresolved` — no evidence
at all promotes them past `GeometricConductorEnd`. None were forced into
a boundary classification; all remain represented (not discarded) in
`model.conductor_boundary_resolutions`, satisfying AP-WIRE-030 §8's
requirement that unresolved conductor ends stay real, retained model
objects rather than being dropped.

## 12. Provenance coverage

Every resolved or conflicted resolution carries a non-empty
`evidence_ids` list resolving to real entries in
`model.conductor_boundary_evidence` (verified both by the unit test
"S. Provenance remains traceable" and by direct inspection of the fresh
extraction's `topology.json` — e.g. the `cde07f87...` conflict traces to
4 distinct evidence entries spanning `TerminalCandidateEvidence` and
`EndpointSemanticReconstructionEvidence` kinds). Conflicted resolutions
additionally carry `conflicting_component_ids` (never emptied on
conflict, unlike the pre-existing `EndpointCandidate.component_id`
field, which `EndpointSemanticReconstructor` clears).

## 13. Topology / Wire / electrical-net invariance

- **Topology**: `topology_nodes` (692), `topology_edges` (877), and
  every `topology_node_types`/`endpoint_kinds` breakdown are unchanged
  (§8). `ConductorBoundaryResolver::resolve()` takes no `TopologyNode`/
  `TopologyEdge` parameter at all — confirmed both by the function
  signature (`include/eke_dx_wire/topology/conductor_boundary_resolver.hpp`)
  and by tests I/J/K/P, which exercise this structurally rather than by
  convention alone.
- **Wire**: `wires` (40), `valid_wires` (40), `heavy_cable_wires` (0),
  `unresolved_wires` (0) are unchanged. `output/wires.svg` is
  byte-identical across two independent fresh-extraction runs and was
  never touched by this AP (`StructuredSvgExporter`/
  `EngineeringDiagramBuilder` reference no new field this AP added).
- **Electrical net**: `electrical_nets` (10), `net_roles` (4 ground / 0
  power_feed / 0 shared_function_feed / 6 unresolved) are unchanged.
  `ConductorBoundaryResolver::resolve()` takes no `ElectricalNet`
  parameter at all.

## 14. Model-Change Gate record

Before writing any resolver code, `EndpointSemanticReconstructor`,
`TerminalSemanticEvidenceBuilder`, `TerminalCandidate`, and
`ConnectorTerminal` were read in full. Finding: `EndpointSemanticReconstruction`
collapses `component_id`/`endpoint_kind`/`terminal_role` conflicts into
**one** status (`component_conflict || kind_conflict || role_conflict`),
with no way to represent "component association Resolved, terminal
identity Unresolved" as two independent facts for the same endpoint —
exactly AP-WIRE-030 §15's required example. This was reported to the
user per §26's mandatory 5-point format (fact / closest existing
structure / why insufficient / smallest proposed change / downstream
effects) and **explicitly approved** before any model change was made:
one new additive struct pair (`ConductorBoundaryEvidence`,
`ConductorBoundaryResolution`) plus one coverage struct
(`ConductorBoundaryCoverage`), following the `WireSemanticResolution`/
`SymbolFamilyResolution` precedent exactly. No existing struct or field
was changed; `boundary_kind` reuses the existing `EndpointKind` enum
rather than introducing a parallel `ConductorBoundaryKind`, per the
spec's explicit prohibition.

## 15. Determinism

Two independent fresh extractions of `samples/trx300ODG.png` produce
byte-identical `artifacts/topology/topology.json`,
`artifacts/audit/extraction_audit.json`, and `output/wires.svg` (`diff`
reports no differences on all three). `ConductorBoundaryResolver::resolve()`
is a pure function of its four `const&` inputs: no timestamp, random ID,
memory address, or unordered-container iteration reaches output (all
`std::map` keys are `std::string` in sorted order; the final `evidence`/
`resolutions` vectors are explicitly re-sorted by `id` before being
returned). Confirmed additionally at the unit level by test "R.
Deterministic output across repeated runs."

## 16. Regression-test coverage

19 cases in `tests/test_conductor_boundary_resolver.cpp`, covering every
item the handoff's §28 list required:

- **A** geometric end with no evidence, **B** explicit component
  terminal resolves, **C** component-resolved/terminal-unresolved
  independence, **D** explicit connector terminal resolves, **E**
  connector-resolved/pin-unresolved independence, **F** explicit ground
  evidence resolves `GroundTerminal`, **G** a chassis-ground component
  elsewhere never leaking into an unrelated endpoint's ground status,
  **H** external connection resolves, **I** `Splice` never produced as a
  boundary result even when the input endpoint itself carries
  `EndpointKind::Splice`, **J**/**K** the structural guarantee (no
  `TopologyNode`/`TopologyEdge` parameter exists on the resolver at all,
  so crossing/continuation topology can never be consulted), **L**
  `Unknown`-kind terminal-candidate evidence contributes nothing, **M**
  equal competing component candidates remain `Conflicted` with no
  winner, **N** equal competing connector-terminal/pin identities remain
  `Conflicted`, **P** the structural guarantee that no `ElectricalNet`
  parameter exists on the resolver either, **R** deterministic output
  across repeated calls, **S** full evidence-id traceability.

Three items (**O** — the real AP-WIRE-024 four conflicts, **Q** — Wire
reconstruction unchanged) were **not** encoded as synthetic unit tests
with hardcoded production content-hash IDs (which would be fragile and
opaque); they are instead verified directly against the real TRX300
extraction in §10/§13 above, which is a stronger check than a synthetic
stand-in would be. Two additional cases beyond the required list were
added: a cross-category (component-vs-ground) conflict, and confirmation
that `EndpointSemanticReconstruction` is consulted as corroborating
evidence only, never as an authoritative override of the per-category
facts.

No existing test was weakened, modified, or removed to make the new
suite pass — `git diff --stat` for every commit in this AP shows only
new files plus the minimal pipeline/export/model insertion points named
in §2.

## 17. Known limitations

- **Terminal identity is structurally always `Unresolved` on this
  baseline** (0/210) — not a defect in the resolver, but a direct
  consequence of no current pipeline stage ever producing a specific
  terminal/pin identifier for the `ComponentTerminal` case. This would
  change only if a future OCR/label-evidence pathway independently
  populates `EndpointCandidate.terminal_name`, which the resolver
  already consumes generically (see the "terminal identity" branch in
  `conductor_boundary_resolver.cpp`) without any further change needed.
- **Connector/connector-terminal facets are structurally always
  `Unresolved`** on this baseline, for the same AP-WIRE-028 §12 reason
  (0 `PrimitiveSymbol` components → 0 connectors materialized). This AP
  does not, and was explicitly told not to, manufacture connectors to
  increase coverage.
- **`review_artifact_writer.cpp` was not extended** with a dedicated
  conductor-boundary visual layer — matching the existing precedent that
  `symbol_family_resolutions` also has no dedicated review-layer image;
  the coverage is fully inspectable via `extraction_audit.json` and
  `topology.json` instead.
- **`EngineeringDiagram`/`StructuredSvgExporter` do not yet reference**
  `ConductorBoundaryResolution` — out of this AP's explicit scope (no
  rendering changes were authorized or made); a future AP could add a
  reference field analogous to `symbol_family_resolution_id` if boundary/
  terminal status ever needs to be visually distinguished in the SVG.

## 18. Open questions

Carried forward unresolved from AP-WIRE-030's specification, per its own
§33 and the handoff's explicit "do not silently resolve, stop and
report" instruction — none were encountered in a way that blocked
implementation, so none required stopping mid-AP, but all three remain
genuinely open:

1. **Concrete evidence threshold for ground endpoint ↔ chassis-ground
   symbol association.** Not needed for this AP: ground boundary
   resolution here operates entirely on existing `TerminalCandidate`
   evidence of kind `GroundConnection` (itself already produced by
   AP-WIRE-024's `TerminalRecognizer` from the `ChassisGround` shape
   detector) — no new association logic between a ground *endpoint* and
   a ground *symbol* was implemented or needed.
2. **Whether `ConnectorTerminal`'s empty-`terminal_name` convention is
   sufficient for genuinely unresolved terminal identity.** Exercised by
   test E and confirmed workable for this AP's purposes (an empty
   `terminal_name` on a `Resolved`-status `ConnectorTerminal` correctly
   yields `connector_terminal_status: Unresolved` here) — but the
   question of whether this is the *right* long-term representation
   remains open, unchanged from AP-WIRE-030.
3. **Final provenance representation mechanism.** Resolved for this AP's
   scope via `ConductorBoundaryEvidence` (a dedicated evidence struct,
   following the `SymbolFamilyEvidence` precedent) — but whether this is
   the mechanism future APs (e.g. a hypothetical richer OCR-linked
   terminal-label pathway) should also adopt, versus extending
   `Provenance` directly, was not decided and remains open.

## 19. Final AP-WIRE-030 assessment

`ConductorBoundaryResolver` is a purely additive, read-only semantic
layer: every AP-WIRE-028 structural count is unchanged (§8), all four
AP-WIRE-024 conflicts remain explicitly unresolved with no winner
selected (§10), and the new boundary/component/terminal/connector/
ground/external facets are independently tracked exactly as
AP-WIRE-030 §15 required — closing the one representational gap
(`EndpointSemanticReconstruction`'s single collapsed status) that made
this AP's Model-Change Gate necessary in the first place, via the
smallest additive change that gap required. 36/210 endpoints (17%) now
have a defensible boundary classification (29 `ComponentTerminal` + 7
`Ground`), 0/210 have a specific terminal identifier (correctly, given
no evidence source for one exists in this baseline), and 0/210 have a
connector association (correctly, given AP-WIRE-028's already-traced
zero-connector root cause) — none of these numbers is reported as
"improvement," only as an honest measurement of what the existing
evidence, viewed through a more precise lens, actually supports.

**AP-WIRE-030 implementation is complete.** No AP-WIRE-031 work
(branch-pairing, distribution-node decomposition, Wire path assembly,
or any Wire-identity scoring/graph search) was implemented, per the
explicit prohibition in §5/§23/§34 of the handoff.

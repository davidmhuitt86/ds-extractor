# AP-DIAG-AUDIT-004 — Terminal Attribution & Boundary Evidence Audit

This is an audit document. **No production classifier, extractor, topology,
terminal-resolver, Wire, ground, scope, or model code was modified to
produce it.** Every count was pulled from a fresh extraction on this AP's
own build; every visual claim was checked by cropping and viewing the
actual source raster; determinism was verified by re-running the
extraction and diffing every terminal-related field.

## 1. Baseline Verification

- `git rev-parse HEAD` (before audit work): `cc1a757bbf54b28b2a79883558fb01cf4167c232`
  — matches AP-DIAG-FIX-004's reported Final SHA exactly.
- `git branch --show-current`: `main`
- `git status --short` (before audit work): empty (clean)
- `git worktree list`: only `main` — no worktree created or left behind
  in this AP.
- No branch was created or switched at any point.
- Full assertion-enabled Release test suite: **56/56 passing**,
  reconfirmed before any audit work. No new compiler warnings (same 8
  pre-existing).

## 2. Fresh Extractions

Both freshly run, neither fixture modified:

- Unscoped: `dx-extract extract samples/trx300ODG.png --output <dir>`
- Scoped: `--scope fixtures/trx300/scope_production.json`

Wire count: 35 in both (unchanged from AP-DIAG-FIX-004's established
baseline). Every finding below was independently confirmed present and
byte-identical in both the unscoped and scoped extraction (scope-
independent).

## 3. Complete Terminal Inventory

Full detail in `artifacts/audit/terminal_attribution_inventory.json`
(203 entries, one per `EndpointCandidate`).

| Kind | Count |
|---|---:|
| `geometric` (GeometricConductorEnd) | 183 |
| `component_terminal` (ComponentTerminal) | 16 |
| `ground` (Ground) | 4 |
| `connector_terminal` (ConnectorTerminal) | 0 |
| `ExternalConnection` | 0 (no such endpoint kind is produced by the current model) |
| `Unresolved` | 0 endpoint-kind entries; however 1 `component_terminal` endpoint (`endpoint-candidate-9da9c73ab52013a3`) is dangling — not an endpoint of any Wire — see §7.4 |

For every endpoint the inventory records: endpoint ID, node ID,
coordinates, kind, terminal role, confidence, attributed component ID,
the attributed component's owned `SymbolPrimitive` count, every
`ConductorBoundaryResolution`/`ConductorBoundaryEvidence`/
`EndpointSemanticReconstruction` ID in its evidence chain, every Wire ID
it participates in, and every `ElectricalNet` ID reachable through those
wires — this is every field the current model exposes per Section 5's
list; `primitive_id` and standalone `conductor segment ID` /
`topology node ID` beyond the endpoint's own node are reachable through
the Wire's `topology_edges`/`conductor_segments` rather than stored
per-endpoint, and are included at the Wire level in the findings
artifact instead of duplicated per-endpoint.

## 4. Terminal Attribution Consistency (All 70 Wire Endpoint Instances)

Every one of the 35 physical Wires' 2 endpoints (70 endpoint instances)
was inspected: kind, attributed component, that component's owned
`SymbolPrimitive` count, and — where the component owns zero primitives
— the actual source raster at that location.

| Verdict | Count |
|---|---:|
| CONFIRMED CORRECT | 66 |
| PLAUSIBLE / AMBIGUOUS | 4 |
| INCORRECT | 0 (at the raw-count level — see §4 note) |
| UNRESOLVED | 0 |

Note: 3 of the 4 PLAUSIBLE/AMBIGUOUS instances were escalated to
**INCORRECT** after manual forensic review with source-image evidence
(§7); the automated first pass could not distinguish "adjacent to a real
symbol, plausibly its lead" from "adjacent to a non-symbol annotation
glyph" without visual inspection — this is exactly why the task
description prohibits using aggregate counts alone as proof. After
manual review:

| Verdict (post-review) | Count |
|---|---:|
| CONFIRMED CORRECT | 66 |
| PLAUSIBLE / AMBIGUOUS | 1 |
| INCORRECT | 3 |
| UNRESOLVED | 0 |

All 66 CONFIRMED entries were CONFIRMED on one of three grounds: (a)
`geometric`-kind with no primitive-bearing component within
`boundary_tolerance` (183 total geometric endpoints, 0 of which sit
close to an unattributed primitive-bearing component — see §11), (b)
`ground`-kind attributed to one of the 6 components AP-DIAG-AUDIT-003
independently confirmed genuine, or (c) `component_terminal`-kind
attributed to a component that owns at least one `SymbolPrimitive`.

The 5 flagged instances (4 wire endpoints + 1 dangling) are exactly the
`component_terminal`-kind endpoints whose attributed component owns
**zero** `SymbolPrimitive`s — see §7 and §8.

## 5. wire-ded6cca62fbe3cd9 — Complete Forensic Reconstruction

```
Wire ID:            wire-ded6cca62fbe3cd9
Endpoint A:          endpoint-candidate-7b1232ec85583772  (880.5, 416)
Endpoint B:          endpoint-candidate-8b9c74742885946f  (880.5, 442)
Topology edge:       topology-edge-9760e3ea7c75d9c9
Conductor segment:   normalized-conductor-segment-01eda2b703cc25ab
identity_status:     resolved
identity_evidence_ids: conductor-boundary-resolution-a70fd7d3bbc0f36c,
                        conductor-boundary-resolution-266c2ac4f90cf2dc
```

Both endpoints attribute to the **same** component:
`component-candidate-shape-region-c2335cc575d107b1`, bounds
`(874, 430, 7, 7)`, `symbol_kind=circular_symbol`,
`status=geometrically_classified`, confidence `high`, **0 owned
`SymbolPrimitive`s**.

Evidence chain for endpoint A:
`conductor-boundary-resolution-a70fd7d3bbc0f36c` (boundary_kind=
component_terminal, boundary_confidence=low) <- evidence
`conductor-boundary-evidence-51e31976b4c5107e` (kind=terminal_candidate,
source `terminal-recognition-489b9e82fae5a1e3`) and
`conductor-boundary-evidence-3e92d17328389115` (kind=
endpoint_semantic_reconstruction, source
`endpoint-semantic-reconstruction-62426f840886e8d4`). Endpoint B's chain
is structurally identical (medium confidence instead of low).

**Direct visual inspection** of `samples/trx300ODG.png`
(crop (830,350)-(950,500), 5x nearest-neighbor) shows: a bulb-holder
connector box above, two vertical conductors labeled `P/W`, a fuse-holder
enclosure below containing two zigzag "SUB FUSE 15A" / "MAIN FUSE 15A"
symbols, and a leader line running from the "SUB FUSE 15A" text label to
a small dot/pointer glyph sitting almost exactly on the right-hand
vertical conductor, between the connector box and the fuse enclosure.
The component at `(874,430,7,7)` **is that leader-line pointer glyph** —
annotation geometry, not an engineering symbol. It has no leads because
it isn't a real component.

**Verdict: BOTH endpoint attributions on wire-ded6cca62fbe3cd9 are
INCORRECT.** The wire's physical identity (topology path, conductor
path, `identity_status=resolved`) is not in question — only the semantic
claim that either endpoint terminates at this component.

**Competing attribution**: none was found nearby. The bulb-holder
connector box directly above this wire is not represented as any
`ComponentCandidate` at all in the model (a search of all 81
`component_candidates` within `(800-960, 350-420)` found nothing) — a
separate, unaddressed coverage gap this AP does not attempt to
characterize further, since doing so would require production
instrumentation beyond audit scope. The fuse-holder enclosure below
(`component-candidate-shape-region-0771bb11fe6c347d`,
`(870,517,52,29)`) is 75-92px from these endpoints — far outside
`boundary_tolerance` (8.0px) — so it was never a candidate.

**Earliest defective pipeline stage**: `ShapeDetector`'s geometric
classification of the annotation leader-line dot as a `circular_symbol`
`ComponentCandidate` is the earliest point this could have been
prevented (an annotation glyph should not become a component candidate
at all). `TerminalLocationDetector` then compounds the defect by
attributing wire-endpoint terminal identity to that component without
any check that it owns a `SymbolPrimitive`.

## 6. Not Isolated — Third Independent Instance Found

A **separate** Wire, `wire-e49784ea9fbccace`
(`endpoint-candidate-70eec6b8e36a24e8` <-> `endpoint-candidate-e9392250710fc19a`),
has its `e9392250710fc19a` endpoint (885.5, 435) — a **distinct topology
node** (`topology-node-cf10dd596d2bfc16`), not shared with either
endpoint of `wire-ded6cca62fbe3cd9` — **also** attributed to the exact
same spurious component `component-candidate-shape-region-c2335cc575d107b1`.

Three separate, genuinely distinct `conductor_end`-type topology nodes
(not a shared splice/crossing point) independently converge on the same
non-engineering-symbol component. This rules out the "single isolated
attribution defect" hypothesis (§8 Option A) — this **is** a broader
failure mode (§8 Option B).

## 7. Same-Failure-Mode Search Across All Endpoints

Systematic sweep of all 26 `circular_symbol` components in the diagram
against their owned `SymbolPrimitive` count:

| | Count |
|---|---:|
| Total `circular_symbol` components | 26 |
| ...owning **zero** `SymbolPrimitive`s | 21 (81%) |
| ...of those 21, actually attracting a terminal attribution | 3 |

The 3 that did are exactly the components already found:
`c2335cc575d107b1` (3 endpoint instances, §5-6),
`d1aefcaca5503ad9` (§7.3), `845947b0afd7451a` (§7.4). The other 18
zero-primitive `circular_symbol` components simply have no wire endpoint
close enough to trigger `TerminalLocationDetector`'s `boundary_tolerance`
— they are not protected by any ownership check, only by geometric luck.

**This is the core finding of this AP**: `TerminalLocationDetector`
(`src/topology/terminal_location_detector.cpp`) computes
component-terminal attribution purely from
`attachment_distance`/`point_to_rect_boundary` — geometric proximity
between an endpoint and a component's bounding box — gated only by
`TerminalLocationConfig::boundary_tolerance` (8.0px, with `high`/`medium`
confidence bands at 2.5px/5.0px). **It never checks whether the
candidate component owns any `SymbolPrimitive` evidence** establishing
that the component actually has a terminal/lead at that location.
`EndpointSemanticReconstructor` (`src/topology/endpoint_semantic_reconstructor.cpp`)
only aggregates/reconciles whatever component ID(s) upstream evidence
already proposed — it introduces no independent distance or ownership
check of its own, so it cannot catch this gap either.

### 7.1 wire-ded6cca62fbe3cd9 (both endpoints) — §5-6, component `c2335cc575d107b1`

Verdict: **INCORRECT** (confirmed annotation glyph).

### 7.2 wire-e49784ea9fbccace (`endpoint-candidate-e9392250710fc19a`) — component `c2335cc575d107b1`

Same spurious component as §5-6. Verdict: **INCORRECT**.
`identity_status=resolved`, `electrical_net_id` unresolved in
`wire_semantics` (WIRE-ONLY impact).

### 7.3 wire-fd53d84a92e53bb4 (`endpoint-candidate-87250d4701edd231`) — component `d1aefcaca5503ad9`

Bounds `(629, 543, 8, 10)`, `circular_symbol`, 0 owned primitives.
Visual inspection (crop (580,500)-(680,600)) shows this small rectangle
sits directly above a round zigzag sensor symbol (near "OIL TEMP" /
"PULSE GEN" labels) — plausibly a real, geometrically-undetected lead-in
transition for that sensor. This wire's *other* endpoint
(`endpoint-candidate-9d7e7957c56e3777`) is one of the 6 ChassisGround
components AP-DIAG-AUDIT-003 independently confirmed genuine
(`component-candidate-shape-region-1acbaeb7ac6ac87a`), and the wire's
`electrical_net_id` (`electrical-net-5de51cd3d5bac578`) is resolved at
`high` confidence as a genuine ground-role net — the wire's identity and
net role are not in question, only this one endpoint's component
attribution. Verdict: **PLAUSIBLE / AMBIGUOUS**, not confirmed either
way. Severity elevated to MEDIUM because it touches a wire that
participates in a resolved, high-confidence net (see §14 impact
classification).

### 7.4 Dangling endpoint `endpoint-candidate-9da9c73ab52013a3` — component `845947b0afd7451a`

Bounds `(145, 158, 18, 24)`, `circular_symbol`, 0 owned primitives, `high`
confidence. This endpoint is **not** an endpoint of any of the 35 Wire
records (dangling). Visual inspection (crop (100,130)-(220,220)) shows
it sits at a wire T-junction directly below a bulb symbol's socket
rectangle — plausibly belonging to that bulb, though not confirmed by an
owned primitive. Verdict: **PLAUSIBLE / AMBIGUOUS**. Downstream impact:
**NO DOWNSTREAM IMPACT** (no Wire, topology edge beyond its own node, or
net is affected).

## 8. Answer to the Core AP Question

> A. it is a single isolated attribution defect, or B. it represents a
> broader terminal-attribution failure mode.

**Determination: B.** The failure mode is architectural
(`TerminalLocationDetector` never verifies `SymbolPrimitive` ownership),
demonstrated to recur against the exact same spurious component from a
third, independent topology node, and present (with varying plausibility)
in 5 of the 16 `component_terminal`-kind endpoints in the current model —
31% of all component-terminal attributions rest on zero primitive-based
ownership evidence. Only 3 of 21 zero-primitive `circular_symbol`
components are currently affected in *outcome*, but that is a
consequence of geometric luck, not of any protective check — per Section
20's explicit instruction, this is not minimized to a localized/isolated
severity merely because current downstream impact is small.

## 9. Component Ownership Audit

For every flagged endpoint, the question "does the attributed component
actually own the geometry that establishes the terminal?" was checked
directly against `symbol_primitives[].component_id`:

| Endpoint | Attributed component | Owned primitives | Ownership established? |
|---|---|---:|---|
| `7b1232ec85583772` | `c2335cc575d107b1` | 0 | No |
| `8b9c74742885946f` | `c2335cc575d107b1` | 0 | No |
| `e9392250710fc19a` | `c2335cc575d107b1` | 0 | No |
| `87250d4701edd231` | `d1aefcaca5503ad9` | 0 | No (plausible adjacency only) |
| `9da9c73ab52013a3` | `845947b0afd7451a` | 0 | No (plausible adjacency only) |

**Reverse condition** (a legitimate terminal's geometry accidentally
attributed to a *neighboring* component instead of its true owner): not
found. All 16 `component_terminal` endpoints resolve to exactly one
`component_id` each (`EndpointSemanticReconstructor`'s
`component_conflict` detection, which clears `component_id` back to
empty on disagreement between evidence items, was not observed to have
fired incorrectly in this dataset — no endpoint shows a cleared/empty
`component_id` alongside conflicting evidence). No case of "endpoint
attributed to component X, but the conductor geometry actually belongs
to neighboring component Y" was found among the 16.

## 10. Terminal Evidence Strength

Using the audit-reporting-only classification from Section 10 of the
task (not a new production scoring system):

| Class | Definition (as observed in this model) | Count (of 16 `component_terminal` endpoints) |
|---|---|---:|
| STRONG | component owns >=1 `SymbolPrimitive` establishing the lead | 11 |
| MEDIUM | valid symbol/component geometry + independently-verified adjacent relationship (e.g. confirmed-genuine ChassisGround at the wire's other end) | 0 additional (the one case with a MEDIUM-strength *wire*, §7.3, is itself only WEAK at this specific endpoint) |
| WEAK | proximity / isolated primitive resemblance only, no owned lead evidence | 4 (§7.1-7.3, the wire-attached zero-primitive instances) |
| CONTRADICTED | competing ownership or geometry disproves the attribution | 3 (§7.1, §7.2 — confirmed by visual inspection to be annotation geometry, actively disproving the component_terminal claim) — counted within the WEAK total above since they were WEAK by evidence, then downgraded to CONTRADICTED by manual visual review |

(The dangling endpoint, §7.4, is WEAK; not double counted as it is not a
wire endpoint.)

## 11. ComponentTerminal vs GeometricConductorEnd

All 183 `geometric`-kind endpoints were checked for the reverse gap: is
there a `geometric` endpoint within `boundary_tolerance` (8.0px) of a
component that **does** own >=1 `SymbolPrimitive`, that should plausibly
have been classified `component_terminal` but wasn't? **Zero** such
cases were found. 11 `geometric` endpoints were found within
`boundary_tolerance` of *some* component's bounding box (several at
distance 0.0, i.e. literally inside it), but in every one of those 11
cases the owning component is `diagram_furniture`-kind (the
switch-continuity table cells identified as false positives in
AP-DIAG-AUDIT-002/003) — see §16. This is reported as a **negative
finding**: `diagram_furniture` does not exhibit the same failure mode,
narrowing where a future fix would need to focus.

No case of a legitimate `component_terminal` being downgraded to
`geometric` merely because of geometric closeness was found either — the
11 `diagram_furniture` cases were never real component terminals to
begin with (per AUDIT-002/003's confirmation that this table geometry is
furniture, not electrical).

## 12. Ground Endpoint Coverage Gap Re-Audit

Re-confirmed AP-DIAG-AUDIT-003's finding: 2 of 6 genuine ChassisGround
components produce no resolved Ground endpoint —
`component-candidate-shape-region-5aa211846bb7891d` and
`component-candidate-shape-region-6b6ccc2d59afe578`.

| Component | Nearest endpoint | Distance | boundary_tolerance |
|---|---|---:|---:|
| `5aa211846bb7891d` (709,547,21,13) | `endpoint-candidate-159125bfa711f45b` (707.5,536) | 16.3px | 8.0px |
| `6b6ccc2d59afe578` (749,547,20,13) | `endpoint-candidate-bae93adb5ee5ca59` (736,518) | 37.0px | 8.0px |

Both nearest endpoints are `geometric`-kind, `boundary_status=unresolved`,
`ground_status=unresolved` in `conductor_boundary_resolutions` — no
`ConductorBoundaryEvidence` or `TerminalCandidate` was ever generated for
either, because both distances exceed `boundary_tolerance` before any
ground-specific logic is reached.

**Determination**: this is a **geometry/boundary-tolerance gap**, not a
terminal-recognition logic defect, not a boundary-resolution logic
defect, and not an intentional representation limitation. It is **not**
combined with the AUDIT-004-001/002/003 failure mode — that pattern is a
spurious *positive* match at short distance with no ownership check;
this is a genuine *absence* of any candidate at all, at a distance
`TerminalLocationDetector` was never designed to reach. No shared root
cause was established between the two findings, so per the task's
explicit instruction they are reported separately, unchanged in
severity (MEDIUM, per AP-DIAG-AUDIT-003).

## 13. Connector Terminal Audit

`connector_candidates`: 0. `connector_terminals`: 0. Confirmed in both
unscoped and scoped extractions — matches the stated TRX300 baseline of
no recognized connector terminals. No connector-related geometry exists
in this model to search for the same failure mode; nothing was
fabricated. N/A for this diagram.

## 14. Downstream Impact Classification

| Finding | Wire(s) | Topology | Conductor boundary | Net | Classification |
|---|---|---|---|---|---|
| wire-ded6cca62fbe3cd9 (§5) | valid, unaffected | valid, unaffected | resolved (evidence IDs point to the wrong component but the resolution itself is structurally valid) | none (`electrical_net_id` unresolved) | **WIRE-ONLY** (wire_semantics component-attribution metadata only) |
| wire-e49784ea9fbccace (§7.2) | valid, unaffected | valid, unaffected | resolved | none | **WIRE-ONLY** |
| wire-fd53d84a92e53bb4 (§7.3) | valid, unaffected | valid, unaffected | resolved | **resolved, high-confidence, independently verified genuine ground-role net — net role/membership itself unaffected by the endpoint's uncertain attribution** | **WIRE-ONLY** (touches a net-participating wire but does not change net role or membership) |
| dangling endpoint (§7.4) | none (not a Wire endpoint) | its own node only | resolved | none | **NO DOWNSTREAM IMPACT** |
| ground coverage gap (§12) | n/a | n/a | unresolved (no evidence generated) | 2 ChassisGround components have no net membership, unchanged from AUDIT-003 | **NO DOWNSTREAM IMPACT** (already known, unchanged) |

No finding in this audit reaches TOPOLOGY IMPACT, NET IMPACT, or
MULTI-STAGE IMPACT as those categories are defined (i.e. corrupting a
topology node/edge, or changing an ElectricalNet's resolved role or
membership). All three wire-level findings are Wire-semantic-metadata
only.

## 15. Wire Impact

All 35 distinct physical Wire records were re-verified unchanged (same
IDs, same `identity_status=resolved` count, same topology/conductor
paths) — this audit performed zero production changes and zero Wire
mutations. For every flagged endpoint: **Wire identity remains valid**;
**Wire path (topology_edges/conductor_segments) is correct**; only the
**semantic endpoint/component attribution** is uncertain or wrong. This
distinction is exactly what AP-DIAG-FIX-004 relied on and this audit
confirms it still holds: a bad terminal attribution does not imply the
underlying physical Wire is bad.

## 16. False-Positive Terminal Search

Searched for terminal candidates plausibly originating from text
glyphs, table markers, connector decoration, symbol fragments,
neighboring-component geometry, ground-symbol fragments, crossing
geometry, conductor artifacts, or annotation geometry:

- **Annotation geometry**: confirmed present and causal — the
  `SUB FUSE 15A` leader-line pointer glyph (§5-6). This is the clearest,
  visually-confirmed false-positive-terminal source found.
- **Table/furniture markers**: 11 `geometric` endpoints overlap
  `diagram_furniture` bounding boxes but do **not** produce spurious
  terminal candidates (§11, §7 negative finding) — this specific source
  is not causing false-positive terminals, unlike annotation geometry.
- **Text glyphs, connector decoration, symbol fragments, ground-symbol
  fragments, crossing geometry, conductor artifacts**: no additional
  instances found beyond the circular_symbol/zero-primitive pattern
  already documented (§7). No candidate was removed; this is a reporting
  finding only, per the task's explicit instruction.

## 17. Determinism

The full unscoped extraction was re-run once more (three total runs
across this AP's evidence-gathering) with no other change. Diffed:
`endpoint_candidates`, `conductor_boundary_resolutions`,
`conductor_boundary_evidence`, `endpoint_semantic_reconstructions`,
`wires`, `component_candidates`, `symbol_primitives` — **all fields
byte-identical** (Python `==` on parsed JSON) across runs. The same 5
flagged endpoints, with identical component attributions and identical
owned-primitive counts, were independently confirmed present and
unchanged in the scoped extraction as well (§2, §7).

## 18. No-Fix Confirmation

No production file was modified. `git status --short` after this AP's
audit work shows only new files under `docs/` and `artifacts/audit/`
(this document and its three companion JSON artifacts) — no source,
header, or CMake change. `TerminalLocationDetector`,
`EndpointSemanticReconstructor`, `ConductorBoundaryResolver`,
`ShapeDetector`, `PhysicalWireIdentityReconstructor`,
`ElectricalNetResolver`, and `SourceScoper` are all byte-identical to
HEAD `cc1a757`.

## 19. Recommended Next AP

A future fix AP should add a `SymbolPrimitive`-ownership check to
`TerminalLocationDetector` (or a narrow boundary immediately downstream
of it) before a `component_terminal` attribution is emitted — requiring
the candidate component to own at least one `SymbolPrimitive` whose
geometry plausibly reaches the endpoint, not merely that the endpoint
falls within a fixed pixel tolerance of the component's overall bounding
box. This would need its own AUDIT-003/004-style test-first regression
(reproducing `wire-ded6cca62fbe3cd9` and `wire-e49784ea9fbccace` as
failing cases) and its own whole-diagram regression to confirm the 66
CONFIRMED-CORRECT attributions in this audit remain unaffected. The
ground endpoint coverage gap (§12) and the terminal-attribution defect
are unrelated root causes and should be addressed as separate AP scopes,
consistent with AP-DIAG-AUDIT-003's original recommendation.

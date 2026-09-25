# AP-DIAG-AUDIT-001 — Baseline Representation Gap Audit

This is an audit document. No production code, test, or extraction
parameter was modified to produce it. Every count and coordinate below
was pulled from a fresh, freshly-verified extraction; every visual claim
was checked by cropping and marking the actual source raster and/or the
pipeline's own review overlays, not inferred from names or convention.

## 1. Audit Scope

Forensic correspondence audit of the current EKE-DX-WIRE extraction
output against the TRX300 source diagram, covering every stage from
image loading through SVG/JSON export. No fixes proposed as code; any
change that would be worth making is recorded as `PROPOSED-FIX-NNN` only.

## 2. Baseline Identity

- Expected baseline commit: `2a7298fd8e167e5ab68d73ad62ea353c23cfd3d4`
  (AP-BASELINE-001) — confirmed via `git rev-parse HEAD` before any work
  began; `git status --short` was empty (clean).
- Clean Release rebuild performed for this audit
  (`rm -rf build`, fresh `cmake`/`build`): 53/53 CTest passing.
  `compile_commands.json` re-confirmed: production (`dx-extract`) has
  `-DNDEBUG` with no `-UNDEBUG`; all 53 test targets have both, in that
  order.

## 3. Source Input

`samples/trx300ODG.png` (1056×816, RGBA), the file AP-BASELINE-001
establishes as canonical — not `trx300OGD.pdf`, which does not exist in
this repository (see AP-BASELINE-001 section G for the same note).

## 4. Pipeline Under Audit

Image loading → preprocessing → shape detection → conductor extraction
→ conductor normalization → geometry classification → endpoint detection
→ boundary resolution → topology reconstruction → physical Wire
reconstruction → electrical net resolution → semantic resolution →
validation → export (SVG + JSON), per `src/pipeline/extraction_pipeline.cpp`.

## 5. Global Metrics

Fresh extraction, `dx-extract extract samples/trx300ODG.png --output <dir>`,
identical invocation to every prior AP measurement:

| Metric | Value |
|---|---|
| Physical wires | 40 (40 Resolved, 0 Conflicted, 0 Unresolved) |
| Electrical nets | 10 (4 Ground role, 0 PowerFeed, 0 SharedFunctionFeed, 6 Unknown/unresolved) |
| Validation errors | 0 |
| Validation warnings | 30 (`NET-ROLE-UNRESOLVED`×6, `WIRE-GEOMETRIC-ENDPOINTS`×24) |
| `extraction_audit.json` / `wires.svg` / `topology.json` SHA-256 | Identical to the AP-BASELINE-001 hash table — reproducibility re-confirmed |
| Component candidates | 85 (Enclosure 2, CircularSymbol 26, ChassisGround 10, PrimitiveSymbol 0, DiagramFurniture 47, Unknown 0) |
| Connector candidates / terminals | **0 / 0** |
| Symbol primitives / component symbol geometries | 39 / 38 |
| Topology nodes | 692 (ConductorEnd 210, Continuation 139, Splice 106, Crossing 237, Junction 0, ComponentBoundary 0, Unresolved 0) |
| Topology edges | 877 |
| Endpoint candidates | 210 (geometric 172, component_terminal 26, ground 12, connector_terminal 0, external_connection 0, splice 0, unresolved 0) |
| Conductor boundary resolutions | 210 total; boundary 38 resolved/172 unresolved/0 conflicted; terminal 0/210/0; connector 0/210/0 |
| Text regions detected | 115 (packaged for external recognition) |
| Text recognition evidence | **0** |
| Semantic associations (geometric, proximity-only) | 1793 |
| Rejected geometry | 1 (`insufficient_supporting_ink`) |

No aggregate count above is treated as proof of correctness by itself —
see sections 6–16 for the correspondence work behind each one.

## 6. Wire Correspondence

All 40 wires were pulled with their structural facts (endpoints,
`identity_status`, evidence, segment/edge counts). Of these, **10 were
individually visually verified** against the source raster (marking each
claimed endpoint coordinate on `samples/trx300ODG.png` and cropping):
5 because a structural screen (both endpoints landing on the same
component's own bounding-box perimeter) flagged them as suspect, and 5
more as unrelated spot checks/positive controls spanning different
diagram regions and endpoint kinds. The remaining 30 are reported with
their structural facts only; per the "do not invent ground truth"
instruction, they are **not** individually claimed as CORRECT — that
would require pixel-level verification this audit did not perform for
each of them. Full endpoint/kind data for the un-spot-checked 30 is
available in `topology.json` for a follow-on audit to complete.

| Classification | Count | Basis |
|---|---|---|
| CORRECT (visually confirmed against source) | 5 | Spot-checked, endpoints and path match real drawn conductors |
| INCORRECT (visually confirmed against source) | 5 | Both endpoints are corners of one component's own body outline — not a conductor at all (Finding AP-DIAG-001) |
| UNVERIFIABLE at this audit's depth | 30 | Structurally self-consistent (Resolved status, evidence present, no perimeter-collision flag) but not individually pixel-checked |

The five INCORRECT wires (12.5% of the total 40):

| Wire ID | Endpoint A | Endpoint B | Source object actually there |
|---|---|---|---|
| `wire-33f921558320cea8` | (229,127) component_terminal | (284,166) component_terminal | CDI UNIT housing, top+right edge (L-path) |
| `wire-549cb3ad15309b45` | (243,167) component_terminal | (215,128) component_terminal | CDI UNIT housing, bottom+left edge (L-path) |
| `wire-430f59213afaef3a` | (161.5,157) component_terminal | (161.5,182) component_terminal | Indicator-lamp base/socket rectangle, right edge |
| `wire-69a0a0492bd97a07` | (144.5,182) component_terminal | (144.5,157) component_terminal | Indicator-lamp base/socket rectangle, left edge |
| `wire-b945dc3bfb9c54d2` | (869,544) component_terminal | (922,545) component_terminal | Battery (12V12AH) case body, bottom edge |

The five CORRECT spot checks (for contrast): `wire-0a3ab2e5ace89210`
(848,436)–(848,460), `wire-17ba2741981c182` (174,157)–(174,182),
`wire-2b012258f9c71a24` (448,161)–(414,161, see Finding AP-DIAG-006
below — the conductor path itself is real even though its endpoint
*kind* is wrong), `wire-e5aa23bee5b2384` region near the
Regulator/Rectifier, and `wire-8b05f44e08427e1` (935,534)–(935,488) —
all trace to real drawn conductor ink between real drawn objects.

## 7. Conductor Extraction Findings

See Finding AP-DIAG-001 and AP-DIAG-002 below. In addition: the
`rejected_geometry` list contains exactly one entry
(`conductor-segment-86e7a7c4e630ebba`, a 1px-wide, 29px vertical stroke
at the image's left margin, reason `insufficient_supporting_ink`) — a
correctly-rejected scan-noise artifact, not a real conductor. No other
conductor-extraction anomalies were found beyond the component/furniture
boundary-ink issue detailed in Findings 001–003.

## 8. Endpoint/Boundary Findings

172/210 endpoints (82%) carry `kind: geometric` — no semantic
classification at all. 26 are `component_terminal`, 12 `ground`, and
**zero** are `connector_terminal`, `external_connection`, or `splice`,
despite `EndpointKind::ConnectorTerminal` existing as a defined value
(`include/eke_dx_wire/core/model.hpp:26`). This traces directly to
Finding AP-DIAG-002 (no `ConnectorCandidate`/`ConnectorTerminal` is ever
produced for this diagram, so no endpoint can ever receive that kind).
Boundary resolution itself reports 0 conflicts everywhere it applies
(`boundary`, `component`, `terminal`, `connector` sub-counts) — where it
resolves something, it does so without contradiction; the gap is in
coverage, not in correctness of what little it does resolve.

## 9. Topology Findings

Node types: 210 `conductor_end`, 139 `continuation`, 106 `splice`, 237
`crossing`, 0 `junction` (consistent with every prior AP-WIRE-028/-029/
-030/-031 TRX300 measurement — the fixture has no `Junction` node),
0 `component_boundary`. The last figure is itself a finding: there is no
node type reserved for "this is where a component's own outline turns a
corner," so that geometry is indistinguishable, in the topology graph,
from a real conductor bend — see Finding AP-DIAG-003.

## 10. Physical Wire Findings

40 wires, all `identity_status: resolved`, 0 `conflicted`, 0
`unresolved` — reconfirmed both from `extraction_audit.json` and by
directly counting `identity_status` values in `topology.json`. Section 6
above establishes that "all Resolved" does not mean "all real": 5 of the
40 (12.5%) are Resolved, evidence-backed, and definitively **not**
physical conductors (Finding AP-DIAG-001). `identity_evidence_ids` on
every wire, including the 5 spurious ones, references real, existing
`ConductorBoundaryResolution`/`ConductorSegment` ids — the evidence
chain is internally consistent, it is simply evidence for the wrong
claim (a component's own boundary line, misread as a conductor).

## 11. Electrical Net Findings

10 nets: 4 `ground` role, 0 `power_feed`, 0 `shared_function_feed`, 6
`unknown`/unresolved (`NET-ROLE-UNRESOLVED`). One of the four
`ground`-role nets (`electrical-net-082a9ad2226c9737`, confidence
`high`, anchored at `endpoint-candidate-731ae6a45fb30f5a`) is
**confirmed incorrect**: see Finding AP-DIAG-005 — its anchor endpoint
sits on a diode symbol, not a chassis-ground symbol, and the net's two
endpoints are the two ends of a real diode-gated ignition-switch wire,
not a ground distribution. The other 3 ground-role nets and all 6
unresolved nets were **not** individually re-verified in this audit;
given Finding AP-DIAG-004/005's pattern, they are flagged as
UNVERIFIABLE/at-risk rather than assumed correct, and are named as a
follow-on item (AP-XXX-002 below). Electrical-Net membership and
physical-Wire identity were confirmed to remain separate concepts
throughout: no net record borrows a Wire's `identity_status`, and no
Wire record references `ElectricalNet` membership as evidence.

## 12. Component/Symbol Findings

85 `ComponentCandidate`s: 2 Enclosure, 26 CircularSymbol, 10
ChassisGround, 47 DiagramFurniture, 0 PrimitiveSymbol/Unknown. Of the 7
distinct components referenced by a `ground`-kind endpoint (of the 10
total ChassisGround-kind detections), 5 were visually spot-checked
against the source:

| Component (x,y,w,h) | What it actually is | Correct ChassisGround? |
|---|---|---|
| (449,154,26,17) | A rectifier diode symbol (rectangle+triangle), part of the IGI→P/W diode-gated pair | **No** — Finding AP-DIAG-005 |
| (398,114,12,15) | The word "SWITCH" in the "IGNITION SWITCH" text label | **No** — text glyphs, not a symbol at all |
| (677,132,38,19) | Small pin-label text characters above the Regulator/Rectifier connector | **No** — text glyphs |
| (861,366,29,23) | A small in-line 2-pin connector/splice housing near the tail light, labeled "(R)" | **No** — a connector body, not a ground symbol |
| (923,535,24,16) | The genuine chassis-ground symbol next to the battery negative terminal | **Yes** |

4 of 5 spot-checked ChassisGround detections (80%) are false positives —
see Finding AP-DIAG-004. The remaining 2 ChassisGround-kind components
not referenced by any ground-kind endpoint, and the remaining 2
referenced ones not shown above, were not individually checked in this
audit (UNVERIFIABLE at this depth; named as a follow-on item).

No evidence of a **missing** real component (a drawn, labeled component
box with zero corresponding `ComponentCandidate` anywhere near it) was
found in the regions inspected; the CDI Unit, Alarm Unit, indicator
lamps, and battery are all represented by *some* `ComponentCandidate`,
just not always the correct *kind* or without spurious wire artifacts
from their own boundaries. A systematic component-by-component inventory
against all ~25 labeled boxes in the source was not completed (time
budget); this is named as a follow-on item.

`DiagramFurniture` classification (the color legend and 5 switch-
continuity tables, 47 of 85 candidates) is **confirmed correct
behavior**: none of the 40 Wire endpoints fall inside the legend/table
region (checked computationally: zero wires have either endpoint below
y=600, the tables' vertical extent), so despite the tables' internal
grid lines being absorbed into general conductor/topology extraction
(Finding AP-DIAG-003), no spurious Wire has been traced back to them.

## 13. Crossing/Splice/Junction Findings

0 `Junction` nodes (consistent with the documented absence of any
Junction-type node anywhere in this fixture, per AP-WIRE-031_AAR.md
§8). 106 `Splice` and 237 `Crossing` nodes exist; the extraction-review
overlay `06_splices.png` visually confirms splice markers correctly
populate real component-lead junction points (e.g. the relay boxes'
internal contacts) **and** confirms they also densely populate the
switch-continuity tables' grid intersections — non-circuit content
being parsed as if it were real splice topology (Finding AP-DIAG-003).
No discrepancy was found in how a genuine splice or crossing affects
physical-Wire or electrical-net behavior beyond what AP-WIRE-031/
AP-WIRE-FIX-003 already establish (a Splice/Junction/Crossing is never a
Wire endpoint; confirmed still true here — 0 wires touch any Splice or
Crossing node as a start/end endpoint).

## 14. Label/Color Findings

Wire color and function-label information is **structurally present but
never semantically read** in this baseline: 115 text regions are
correctly geometrically detected and packaged
(`artifacts/recognition/recognition_input.json`, `instructions.md`,
per-region crops and nearby-object context), and 1793 purely-geometric
`label_to_endpoint`/`label_to_component` proximity associations are
computed — but `text_recognition_evidence` is empty (0 entries), because
the baseline CLI invocation used throughout every AP task
(`dx-extract extract samples/trx300ODG.png --output <dir>`) passes
neither `--recognition <observations.json>` nor `--vision-recognition`
(confirmed by reading `src/app/main.cpp`'s argument handling). As a
direct, deterministic consequence: `wire_color_status` is `unresolved`
for all 40/40 wires, `function_status` is `unresolved` for all 40/40, and
no `DistributionRole` other than `Ground` (which comes from geometric
ground-symbol detection, not text) can ever be assigned — confirmed by
reading `circuit_role_evidence_builder.cpp`'s `explicit_label_role()`,
which requires an exact-match text label ("GROUND", "POWER FEED", "B+",
etc.) that never exists without a recognition pass. This is Finding
AP-DIAG-006/007 below. Component labels (e.g. "CDI UNIT", "ALARM UNIT")
and the color-key legend are likewise geometrically located but never
read as text content.

## 15. SVG Findings

`output/wires.svg` carries exactly 40 `data-wire-id` groups, one per
Wire record — confirmed by count. All 5 spurious wires from Finding
AP-DIAG-001 are present in the SVG (e.g.
`data-wire-id="wire-33f921558320cea8"` with its real
`data-start-endpoint`/`data-end-endpoint` attributes), meaning they
render as ordinary wire lines directly on top of the CDI Unit/lamp/
battery symbols they were spuriously derived from. This is a **faithful
export of an upstream defect**, not an export-layer defect in its own
right: the SVG accurately reflects what the internal model says.
No duplicated, fragmented, or missing wire was found among the 40 groups
(exactly 40 in, 40 out).

## 16. JSON Export Findings

`topology.json` carries the full internal model (`identity_status`,
`identity_evidence_ids`, `wire_semantics`, boundary resolutions, etc.).
`engineering_diagram.json` is a deliberately reduced projection: a
sampled wire record there
(`wire-0a3ab2e5ace89210`) carries `wire_id`, both endpoint ids, topology
edge/segment ids, `geometry_confidence`, `heavy_cable`, and a
`wire_semantic_resolution_id` cross-reference, but **not**
`identity_status` or `identity_evidence_ids` directly. Since the richer
fields remain available in `topology.json` and the omission is uniform
across all 40 wires (not a selective loss), this is classified as an
**intentional export-schema simplification**, not an export defect — a
consumer of `engineering_diagram.json` alone cannot distinguish a
Resolved wire from a Conflicted one, which is worth naming as a
follow-on schema question but is not a "defect" by this audit's
evidentiary standard (no requirement was found stating
`engineering_diagram.json` must carry identity status).

## 17. Existing Warning Analysis

- **`NET-ROLE-UNRESOLVED` (×6)**: given zero text-recognition evidence
  (Finding AP-DIAG-006), `Unknown` is the only defensible role for any
  net that isn't Ground-anchored — assigning `PowerFeed` or
  `SharedFunctionFeed` without text evidence would be exactly the kind
  of guess AP-WIRE-029 forbids. **Classification: representation
  limitation, correctly and conservatively surfaced by the validator;
  not a validator defect.** (Separately, one of the *resolved* Ground
  nets is itself wrong — Finding AP-DIAG-005 — which the validator
  cannot detect, since nothing about that net's *structure* is invalid.)
- **`WIRE-GEOMETRIC-ENDPOINTS` (×24)**: a direct, correct report that 24
  of 40 wires have zero semantic classification on either end. Two
  contributing upstream causes were confirmed: the connector-terminal
  gap (Finding AP-DIAG-002 — `ConnectorTerminal` can never be assigned)
  and the broad 82% "geometric" endpoint-kind rate generally. **Classification:
  genuine, correctly-surfaced extraction/classification gap** — not a
  validator defect, but also not merely "expected" in the way the net-role
  warning is; TerminalRecognizer's effective coverage on this diagram is
  a concrete, addressable limitation.

## 18. Confirmed Representation Gaps

### AP-DIAG-001 — Component/symbol body outlines fabricated into physical Wires
- **Severity**: CRITICAL
- **Object Class**: Physical Wire / Component boundary
- **Source Evidence**: `samples/trx300ODG.png` — CDI Unit housing
  (~216,128–284,167), indicator-lamp base/socket rectangle (~145,158–
  163,182), battery case body (~870,517–922,546). Each visually confirmed
  by marking the claimed wire endpoints directly on the raster (see
  section 6 crops).
- **Current Representation**: 5 of 40 `Wire` records
  (`wire-33f921558320cea8`, `wire-549cb3ad15309b45`,
  `wire-430f59213afaef3a`, `wire-69a0a0492bd97a07`,
  `wire-b945dc3bfb9c54d2`), all `identity_status: resolved`, both
  endpoints `component_terminal`, backed by real (but misapplied)
  `ConductorBoundaryResolution`/`ConductorSegment` evidence.
- **Expected Representation**: no Wire record at all — a component's own
  boundary line is not a conductor.
- **First Loss Stage**: shape/conductor extraction (a component's own
  detected boundary ink is not excluded from the conductor-segment/
  topology graph it also participates in as a shape).
- **Downstream Consequence**: inflates the physical-wire count (40
  reported, ≤35 confirmed real from this sample), inflates
  `topology_nodes`/`topology_edges`, and — where such a spurious wire's
  endpoint happens to coincide with a real terminal region — could mask
  or double-count real connectivity.
- **Confidence**: HIGH (5/5 visually confirmed, pixel-level correspondence).
- **Recommended Follow-on AP**: `PROPOSED-FIX-001`.

### AP-DIAG-002 — No Connector modeling: taxonomy has no Connector kind
- **Severity**: HIGH
- **Object Class**: Connector / Connector terminal
- **Source Evidence**: CDI Unit, Alarm Unit, and Regulator/Rectifier all
  draw an explicit multi-pin connector-plug glyph (a notched trapezoid
  feeding into separate pin leads) at their base; the Ignition Switch and
  the Lighting+Engine Stop+Starter Switch block are drawn as literal
  multi-pin connector housings.
- **Current Representation**: `connector_candidates: 0`,
  `connector_terminals: 0` for the entire extraction.
  `ComponentCandidateKind` (`include/eke_dx_wire/core/model.hpp:175-188`)
  has exactly six values — `Enclosure, CircularSymbol, ChassisGround,
  PrimitiveSymbol, DiagramFurniture, Unknown` — none of which represents
  "connector." `ConnectorTerminalModelBuilder` (invoked in
  `extraction_pipeline.cpp:342-349`) can only materialize a connector
  from evidence that this taxonomy never produces.
- **Expected Representation**: at least the visually unambiguous
  connector-plug glyphs (CDI Unit, Alarm Unit) represented as
  `ConnectorCandidate`+`ConnectorTerminal` objects.
- **First Loss Stage**: component/shape classification (taxonomy gap) —
  not connector-terminal-model building itself, which is a correct,
  conservative consumer of upstream evidence that doesn't exist.
- **Downstream Consequence**: every endpoint that should be a
  `ConnectorTerminal` instead stays `geometric` or, at best,
  `component_terminal`, directly inflating the 82% "geometric" endpoint
  rate (Finding basis for `WIRE-GEOMETRIC-ENDPOINTS`, §17), and
  `wire_semantics.start_connector_status`/`end_connector_status` stay
  `unresolved` for every wire.
- **Confidence**: HIGH (confirmed structurally via source + zero output;
  connector glyphs visually confirmed present in source).
- **Recommended Follow-on AP**: `PROPOSED-FIX-002`.

### AP-DIAG-003 — Component/furniture boundary ink absorbed into general topology
- **Severity**: MEDIUM
- **Object Class**: Component boundary / Topology
- **Source Evidence**: `09_topology.png` review overlay traces the CDI
  Unit, Alarm Unit, and Regulator/Rectifier housing outlines in the same
  color/style as real conductor wires (visually confirmed, section 12 of
  the working audit). `06_splices.png` shows dense Splice markers inside
  all 5 switch-continuity tables' grid lines.
- **Current Representation**: 0 `component_boundary`-type topology
  nodes exist (`node types` breakdown, §9) — there is no node type
  reserved for "this is a component/furniture outline corner." Every
  such corner is an ordinary `Continuation`/`Splice`/`Crossing` node,
  indistinguishable in the graph from real conductor topology.
- **Expected Representation**: component/furniture boundary ink excluded
  from conductor/topology extraction, or tagged as a distinct
  non-electrical node/edge kind.
- **First Loss Stage**: shape detection / conductor extraction (same
  stage as AP-DIAG-001, broader manifestation).
- **Downstream Consequence**: confirmed to inflate `topology_nodes`
  (692) and `topology_edges` (877) well beyond the true circuit;
  confirmed **not** to produce spurious Wires from the fully-enclosed
  furniture tables specifically (0 wire endpoints below y=600). Real
  component enclosures (not fully enclosed by other ink) are the
  mechanism behind AP-DIAG-001's specific spurious wires.
- **Confidence**: HIGH (direct visual confirmation of both the housing
  and furniture cases).
- **Recommended Follow-on AP**: `PROPOSED-FIX-001` (shared root cause
  with AP-DIAG-001).

### AP-DIAG-004 — ChassisGround shape classification has a high false-positive rate
- **Severity**: HIGH
- **Object Class**: Component / Ground symbol
- **Source Evidence**: of 5 ChassisGround-kind components spot-checked
  against the source raster (see §12 table), 4 were confirmed to be
  something other than a ground symbol: a rectifier diode glyph, two
  instances of plain text-label characters, and a small 2-pin connector/
  splice housing. Only 1 of 5 (the component next to the battery
  negative terminal) is a genuine chassis-ground triangle symbol.
- **Current Representation**: 10 components carry
  `ComponentCandidateKind::ChassisGround`; 12 endpoints carry
  `EndpointKind::Ground` / `terminal_role: ground_terminal`, several at
  `confidence: high`.
- **Expected Representation**: only the visually genuine ground symbol
  should carry ChassisGround/Ground classification.
- **First Loss Stage**: shape detection / component classification
  (`ShapeDetector`/component-candidate-classification stage — whatever
  gate distinguishes a ground triangle from a diode rectangle-triangle or
  from small text glyphs is evidently under-constrained on this source).
- **Downstream Consequence**: directly caused Finding AP-DIAG-005 (a
  wrong Ground-role electrical net). The 2 remaining un-checked
  ChassisGround-referenced components and the 3 ChassisGround components
  not referenced by any ground-kind endpoint were not verified in this
  audit (UNVERIFIABLE at this depth) and may hide further instances.
- **Confidence**: HIGH on the 4 confirmed false positives; MEDIUM on the
  overall false-positive *rate* (small sample, n=5 of 10).
- **Recommended Follow-on AP**: `PROPOSED-FIX-003`.

### AP-DIAG-005 — A reported Ground-role electrical net is not a ground distribution
- **Severity**: CRITICAL
- **Object Class**: Electrical Net
- **Source Evidence**: `endpoint-candidate-731ae6a45fb30f5a` at (448,161)
  sits exactly on the rectifier-diode glyph identified in AP-DIAG-004,
  not on any ground symbol; the wire it anchors
  (`wire-2b012258f9c71a24`, endpoints (448,161)–(414,161)) is a real,
  correctly-extracted conductor — the IGI pin's drop to a diode leading
  toward the "P/W" wire — but it is not a ground wire.
- **Current Representation**: `electrical-net-082a9ad2226c9737`,
  `role: "ground"`, `confidence: "high"`, anchored at that same
  misclassified endpoint.
- **Expected Representation**: this net should not be classified
  `Ground` (its actual role, per this audit's raster-only evidence
  standard, is UNVERIFIABLE — no text was read, so no legitimate
  alternative role can be asserted either; it should be `Unknown` absent
  the ground misclassification).
- **First Loss Stage**: component/shape classification (same defect as
  AP-DIAG-004), propagated through `DistributionDecomposer`'s
  correctly-implemented (but garbage-in) anchor logic
  (`distribution_decomposer.cpp:224`: `anchor->kind == EndpointKind::Ground
  ? DistributionRole::Ground : ...`).
- **Downstream Consequence**: 1 of the 4 headline "Ground-role
  electrical nets" is confirmed wrong. This directly undermines the
  reliability of the `net_roles.ground: 4` statistic reported in every
  prior AP measurement's audit JSON, including AP-BASELINE-001's.
- **Confidence**: HIGH (endpoint position, anchor linkage, and the
  underlying diode misclassification all independently confirmed).
- **Recommended Follow-on AP**: `PROPOSED-FIX-003` (same root cause as
  AP-DIAG-004; this is its electrical-net-level consequence).

### AP-DIAG-006 — No wire-color or function-label semantic resolution occurs
- **Severity**: MEDIUM (expected limitation, not an algorithmic defect —
  see classification note)
- **Object Class**: Label / Wire color
- **Source Evidence**: the diagram draws an explicit wire-color legend
  (11 named colors) and per-wire color labels (e.g. "Br", "G", "P/W")
  throughout; 115 text regions are correctly geometrically located.
- **Current Representation**: `text_recognition_evidence: 0` for the
  whole extraction; `wire_color_status: unresolved` for all 40/40 wires;
  `function_status: unresolved` for all 40/40.
- **Expected Representation**: wire color and function labels read and
  attached, given the CLI's own `--recognition`/`--vision-recognition`
  options exist for exactly this purpose.
- **First Loss Stage**: semantic resolution (recognition never invoked —
  an invocation choice, not a bug in the recognition/association code
  itself, which correctly produces region geometry and proximity
  associations without it).
- **Downstream Consequence**: Finding AP-DIAG-007 (net role resolution)
  and the `NET-ROLE-UNRESOLVED` warning population (§17).
- **Confidence**: HIGH.
- **Classification note**: this is an **EXPECTED LIMITATION of the
  baseline CLI invocation**, not an EXTRACTION or MODELING DEFECT — the
  capability is implemented and architecturally sound
  (`recognition_input.json`'s explicit `do_not_invent_text: true`
  contract), simply not exercised by the command every AP task has used.
- **Recommended Follow-on AP**: `PROPOSED-FIX-004` (baseline-invocation
  change, not an algorithm change).

### AP-DIAG-007 — 6 of 10 electrical nets have no resolvable circuit role
- **Severity**: LOW (given Finding AP-DIAG-006 is the root cause and the
  validator's `Unknown` assignment is itself correct)
- **Object Class**: Electrical Net
- **Source Evidence**: `explicit_label_role()`
  (`circuit_role_evidence_builder.cpp:34-52`) requires an exact-match
  text label; with zero text evidence (AP-DIAG-006), no net lacking a
  Ground-kind anchor can ever receive a role.
- **Current Representation**: `NET-ROLE-UNRESOLVED` ×6.
- **Expected Representation**: unchanged — `Unknown` is the correct,
  non-guessing answer given the available evidence.
- **First Loss Stage**: semantic resolution (same root cause as
  AP-DIAG-006).
- **Downstream Consequence**: none beyond the warning itself; this is
  the validator doing its job.
- **Confidence**: HIGH.
- **Recommended Follow-on AP**: none beyond AP-DIAG-006's.

## 19. Suspected Gaps

- **SUSPECT-001**: the 2 ChassisGround-kind components not referenced by
  any ground-kind endpoint, and 2 of the 7 referenced ones, were not
  individually visually checked. Given AP-DIAG-004's 4-of-5
  false-positive rate, more misclassifications likely exist among them.
  UNVERIFIABLE at this audit's depth.
- **SUSPECT-002**: the 3 non-anchor Ground-role electrical nets and all
  6 Unknown-role nets were not individually re-verified for anchor
  correctness the way AP-DIAG-005's net was. Given the demonstrated
  pattern, at least one more may be similarly wrong. UNVERIFIABLE at
  this audit's depth.
- **SUSPECT-003**: the 30 wires not individually spot-checked in §6 are
  presumed structurally plausible (Resolved, evidence present, no
  perimeter-collision flag) but were not raster-verified. Given
  AP-DIAG-001 was found via a narrow structural heuristic (both
  endpoints on the *same* component's perimeter) that would miss a
  spurious wire touching only *one* component corner plus one otherwise-
  unremarkable point, more instances of the AP-DIAG-001 pattern cannot
  be ruled out among the 30. UNVERIFIABLE at this audit's depth.
- **SUSPECT-004**: whether any of the 26 `component_terminal`-kind
  endpoints not involved in an AP-DIAG-001 wire are themselves affected
  by the same component-boundary-vs-conductor ambiguity (e.g. correctly
  landing on a real lead, but with a path that clips a few pixels of the
  component's own outline) was not checked at the individual-segment
  level. UNVERIFIABLE at this audit's depth.
- **SUSPECT-005**: a full component-by-component inventory of all ~25
  labeled boxes in the source diagram against the 85 `ComponentCandidate`
  records (to find any component with zero corresponding candidate
  anywhere, i.e. a true "missing component" rather than a
  misclassified one) was not completed. Not ruled in or out.

## 20. Proposed Follow-on APs

- **PROPOSED-FIX-001**: exclude a `ComponentCandidate`'s own detected
  boundary geometry from conductor-segment/topology extraction (targets
  AP-DIAG-001 and AP-DIAG-003's shared root cause). Likely touches shape
  detection / conductor extraction, not `PhysicalWireIdentityReconstructor`
  itself, which is correctly consuming whatever evidence it's given.
- **PROPOSED-FIX-002**: add a `Connector`-class value to
  `ComponentCandidateKind` (or an equivalent classification path) so
  `ConnectorTerminalModelBuilder` has evidence to work from on diagrams
  like TRX300 that draw explicit multi-pin connector-plug glyphs.
- **PROPOSED-FIX-003**: tighten the ChassisGround shape-classification
  gate to reject diode glyphs, text-label ink, and small connector/splice
  housings (targets AP-DIAG-004 and its AP-DIAG-005 consequence).
- **PROPOSED-FIX-004**: not a code change — consider whether the
  established AP-WIRE baseline invocation should include
  `--recognition`/`--vision-recognition` so that wire-color/function-
  label/connector-identity resolution is exercised in the numbers every
  AP task reports (targets AP-DIAG-006/007). This would change the
  baseline's expected metrics and needs its own dedicated AP with
  explicit sign-off, not a silent parameter change.
- **AP-XXX (audit completion)**: finish SUSPECT-001 through SUSPECT-005
  — a full, systematic (not spot-check) sweep of all 10 ChassisGround
  components, all 10 electrical nets' anchors, and all 40 wires' endpoint
  pairs against component-boundary perimeters, ideally via an automated
  check added to the audit tooling rather than manual crops.

## 21. Non-Issues / Correct Behavior

- `DiagramFurniture` classification of the color legend and 5
  switch-continuity tables (47/85 components) is confirmed correct and
  effective: these regions contribute zero Wires, zero Splice/Crossing
  false-connectivity into the real circuit's Wire output, despite their
  internal ink being absorbed into general topology bookkeeping
  (AP-DIAG-003).
- Splice/Junction/Crossing nodes are confirmed to never become a Wire
  endpoint anywhere in this extraction (0 wires touch any such node as
  start/end) — the AP-WIRE-029/031 invariant holds.
- `Junction` node type: 0 instances, consistent with every prior
  AP-WIRE measurement of this exact fixture — not a defect, a fact about
  this diagram.
- The one `rejected_geometry` entry is a correct rejection of scan-noise
  ink, not a missed conductor.
- `identity_evidence_ids` were confirmed, on every wire including the 5
  spurious ones, to reference only real, pre-existing
  `ConductorBoundaryResolution`/`ConductorSegment` records — no evidence
  is ever fabricated, even when the evidence supports a wrong conclusion.
- Electrical Net membership and physical Wire identity remain
  structurally independent, as AP-WIRE-029 requires: confirmed no
  cross-contamination of one concept's fields into the other's records.
- The recognition hand-off package
  (`recognition_input.json`/`instructions.md`) is well-formed, explicit
  about not inventing text, and correctly scoped — the *invocation*
  gap (AP-DIAG-006) is not a defect in this package.

## 22. Audit Conclusions

The current baseline's headline numbers (40 wires, 0 conflicts, 10 nets,
0 validation errors) describe a pipeline that is internally consistent
and free of the specific defect classes AP-WIRE-FIX-001/002/003 already
targeted — but they do **not** describe a pipeline whose output is 40/40
correct. Concretely, in the areas this audit could check in depth:

- **12.5% of reported physical wires (5/40) are confirmed fabrications**
  of component/symbol body outlines, not conductors (AP-DIAG-001).
- **Connectors do not exist in the model at all** — a taxonomy gap, not
  a detection failure on a specific instance (AP-DIAG-002).
- **80% of a small ChassisGround sample (4/5) are false positives**,
  and at least one of the resulting electrical nets is confirmed
  misclassified as a Ground distribution when it is a diode-gated signal
  wire (AP-DIAG-004/005).
- **No wire color, function label, or non-Ground circuit role is ever
  resolved** in this baseline, by design of the invocation used, not a
  bug — but this means roughly a quarter of the diagram's semantic
  content (colors, labels, connector identity) is currently invisible to
  every downstream consumer of this extraction (AP-DIAG-006/007).

These are concrete, evidence-backed gaps between what the source diagram
shows and what the structured model currently represents, sitting mostly
at the shape-detection/component-classification stage rather than in
topology reconstruction, Wire identity, or electrical-net logic
themselves — which is worth noting, since AP-WIRE-FIX-001/002/003 all
targeted exactly that earlier stage and this audit's two most severe
findings (AP-DIAG-001, AP-DIAG-004/005) are further instances of the same
general defect family (component/shape classification leaking into
conductor and electrical semantics) rather than anything new in
topology/identity reconstruction. No finding in this audit implicates
`PhysicalWireIdentityReconstructor`, `ConductorBoundaryResolver`,
`ElectricalNetResolver`, or `DistributionDecomposer`'s own logic as
incorrect given the evidence they were handed — in every confirmed case,
the defect is upstream of them, in what evidence exists to hand them.

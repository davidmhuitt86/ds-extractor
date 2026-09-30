# AP-DIAG-018 — Source-to-Object Visual Reconciliation

Strictly diagnostic. No detector, threshold, morphology, endpoint, connector,
wire reconstruction, topology, net resolution, or semantic logic was touched
in this AP.

## Authoritative extraction

- Source: `samples/trx300ODG.png`, 898×549, page 0
- Extraction-results commit: `a111cdd27b855fc9a3c6d2d28a7a4d0ab05c77cf`
- Verified against a fresh build of the current (post AP-DIAG-017A) code:
  re-running the same extraction reproduces identical population counts and
  an internally-agreeing `review_manifest.json`/`extraction_audit.json` pair.

## Methodology

Every object in every required population received a **structural**
classification computed deterministically from `extraction_audit.json`
(topology node types/degree, wire↔edge↔segment ownership joins, net
membership joins). This gives full population coverage — no object is
silently omitted — but structural inference alone is capped at **MEDIUM**
confidence with `evidence_type` `STRUCTURED_TOPOLOGY` /
`STRUCTURED_OBJECT_RELATIONSHIP` / `GEOMETRIC_RELATIONSHIP`.

A representative, stratified sample per population (listed in
`evidence.source_visual_sample_object_ids` in the JSON artifact) was
additionally verified by directly cropping and visually inspecting
`00_source.png` and cross-referencing `01_wires.png`, `09_topology.png`,
`11_endpoint_debug.png`, `05_connectors.png` at the object's exact
coordinates. Those objects carry `evidence_type: SOURCE_VISUAL` and
confidence `HIGH` (or `MEDIUM` where the crop was suggestive but not fully
conclusive). **No object was assigned HIGH confidence from detector
confidence or structural inference alone** — HIGH requires an actual
inspected crop.

This is an honest limitation: with 1,146 records across 8 populations,
full individual visual inspection of every object was not performed. Where
a population's classification rests on a small visual sample extrapolated
by structural analogy, the report says so explicitly rather than
overstating coverage.

## Object Reconciliation

### 1. Zero-Wire endpoints (117/191)

All 117 share one structural signature, found by a full-population join
(not sampling): every one is a `conductor_end` topology node of **degree
1** (exactly one incident edge), and that single incident edge is itself
always a member of the 602 "unowned by any Wire" population. There are
**zero** zero-Wire endpoints of kind `splice`, `ground`, or
`connector_terminal` — consistent with the project's Wire definition
(splices are never a Wire's own endpoint) and with grounds/connectors
independently resolving cleanly elsewhere in this same run.

| kind | count | classification | confidence basis |
|---|---|---|---|
| `component_terminal` | 7 | `COMPONENT_TERMINAL` | 1 visually confirmed HIGH (`endpoint-candidate-f116d72e55f48cc6`, cross-referenced against AP-DIAG-017's independent finding), 6 by structural analogy (MEDIUM) |
| `geometric` | 110 | `WIRE_INTERRUPTION` | 4 visually confirmed (3 HIGH, 1 MEDIUM), 106 by structural analogy (MEDIUM) |

Visually confirmed samples (crops taken at the exact `(x,y)`):

- `endpoint-candidate-d795bf8dbfc260d3` (85, 63): a vertical branch from the
  indicator-lamp common bus visibly descends into the lamp's own terminal;
  the ink is continuous and electrical, but no Wire reaches this point.
- `endpoint-candidate-6a653f03c6888e27` (133, 329): a horizontal wire from a
  real component visibly continues past this point; the dead-end coincides
  with a wire-color-code text label sitting on the wire ink.
- `endpoint-candidate-f116d72e55f48cc6` (81.5, 106): the same headlight
  terminal AP-DIAG-017 independently confirmed as a real `ComponentTerminal`
  resolved via `RejectedGeometryEvidence` — a genuine component terminal,
  not a detection artifact, that simply has no Wire reaching it.
- `endpoint-candidate-f5bc21218ece9993` (853, 280): near a dense multi-line
  crossing with adjacent wire-color labels; suggestive but less clean
  (MEDIUM).

**Answer to Question A**: the 117 zero-Wire endpoints are **not**
predominantly detection noise. In every visually-inspected case the
underlying ink is real and electrically continuous; the defect (where one
exists at all) is that Wire reconstruction did not extend a Wire object to
that point, not that the endpoint itself is spurious. 7 are additionally
confirmed/analogous real component terminals.

### 2. Unowned topology edges (602/643)

Full-population structural join by node-type pair:

| classification | count | % |
|---|---|---|
| `SHARED_CONDUCTOR_PATH` | 406 | 67% |
| `VALID_CONDUCTOR_NOT_RECONSTRUCTED` | 117 | 19% |
| `SPLICE_TOPOLOGY` | 35 | 6% |
| `UNDETERMINED` | 44 | 7% |

`SHARED_CONDUCTOR_PATH` (67%) is every edge touching a `crossing` node — and
per the project's own Wire definition, "Crossing has no electrical
meaning." These edges are the topology graph's bookkeeping for a real
conductor's path around a visual line crossing, not an independent
electrical relationship and not evidence of reconstruction failure.
`VALID_CONDUCTOR_NOT_RECONSTRUCTED` (117, 19%) is an **exact** join, not an
estimate: these are precisely the single incident edges of the 117
zero-Wire dead-ends above (see Correlated Failure Patterns). `SPLICE_TOPOLOGY`
(35, 6%) touches splice nodes, consistent with the Wire definition's
explicit allowance that "a Wire may pass through one or more splice/junction
locations" without those locations being Wire endpoints. 44 (7%) were not
matched to a confident pattern and are left `UNDETERMINED`.

**Answer to Question B**: primarily **intentional topology representation**
(73% combined `SHARED_CONDUCTOR_PATH` + `SPLICE_TOPOLOGY`), with a real but
minority (19%) incomplete-Wire-reconstruction component, and a small (7%)
genuinely undetermined remainder. This is not primarily detector
over-generation.

### 3. Topology-only conductor segments (189/230)

| classification | count | % |
|---|---|---|
| `SHARED_BRANCH_GEOMETRY` | 127 | 67% |
| `UNDETERMINED` | 62 | 33% |

**Answer to Question C**: the majority (67%) is consistent with — expected
under — the endpoint-to-endpoint Wire model's explicit allowance that
"shared conductor geometry may... participate in multiple endpoint-to-endpoint
relationships when topology branches": these segments underlie
crossing/splice topology and are not required to be owned by any single
Wire's own segment list. The remaining 33% (62 segments) could not be
matched to a confident pattern from structural data alone and are left
`UNDETERMINED` rather than assumed benign — this is the honest limit of
what this AP established without further visual sampling of that specific
subset.

### 4. Components without terminal evidence (23/34)

Full-population join against `component_semantic_audit.json` (which exposes
`ComponentSymbolRecognition`/`SymbolFamilyResolution`, not present in
`extraction_audit.json`):

| classification | count | confidence basis |
|---|---|---|
| `DIAGRAM_SYMBOL` | 21 | 3 visually confirmed HIGH, 18 by structural analogy (MEDIUM) |
| `REAL_COMPONENT_BUT_TERMINAL_NOT_RECOGNIZED` | 2 | both visually confirmed HIGH |

All 6 `ChassisGround` component candidates and 5 of 26 `circular_symbol`
candidates **do** have terminal evidence (matching this session's earlier,
independently-verified finding that all 6 ground symbols resolve cleanly).
The 21 without evidence are overwhelmingly tiny (most ≤22×20px)
`circular_symbol` candidates. Three, in three different regions of the
diagram, were directly inspected and are **filled junction/connection dots
at wire intersections** (the "switch matrix" grid area the user described
masking to avoid) — not discrete electrical components at all, and
structurally incapable of ever acquiring `TerminalCandidate` records because
they are not terminal-bearing symbols.

The 2 `enclosure`-kind candidates without terminal evidence are real,
substantial, and visually unambiguous: the **CDI UNIT** box (visible pin
leads at its base) and the **BATTERY 12V12AH** symbol (visible + / −
terminal posts with heavy-cable leads). Both are genuine components whose
terminals the current terminal detector did not capture.

**Answer to Question D**: mostly **not** missed real components — 21 of 23
(91%) are a diagram-symbol misclassification (junction dots read as
`circular_symbol` candidates), and only 2 of 23 (9%) are a genuine
terminal-detection gap on real components.

### 5. Electrical-net endpoint coverage (153/191)

Exact join (not sampled) against the zero-Wire population:

| classification | count | % |
|---|---|---|
| `FOLLOWS_FROM_ZERO_WIRE` | 91 | 59% |
| `SEPARATE_NET_RESOLUTION_GAP` | 62 | 41% |

**Answer to Question E**: 59% follows directly and necessarily from the
zero-Wire population (an endpoint with no Wire cannot reach net resolution,
which is downstream of Wire reconstruction) — not an independent defect.
The remaining 62 (41%) **do** own at least one Wire yet still lack net
membership: a genuinely separate net-resolution gap, not explained by
anything else in this reconciliation. This is a real, distinct finding
worth a future diagnostic AP of its own — not fixed here.

### 6. Fully unresolved Wires (27)

Every one of the 27 has `identity_status: resolved` (geometric identity is
fine) and both endpoints of kind `geometric` (neither end reached any
semantic role: component, connector, ground, or net). All 27 are short
(24–85px) and mostly single-segment/single-edge — the structural signature
of small connecting jumpers between nearby symbols, not long unexplained
runs.

| classification | count | confidence basis |
|---|---|---|
| `SOURCE_CONFIRMED` | 2 | visually confirmed HIGH |
| `PARTIALLY_CONFIRMED` | 25 | short/simple geometry matching the confirmed pattern by analogy (MEDIUM), not individually inspected |
| `AMBIGUOUS` | 0 | — |

Visually confirmed: `wire-054e53a69a62ad22` (real stub feeding the
visually-confirmed MINI connector near (600,289)) and
`wire-2498c8429f52632b` (real bus segment feeding a visually-confirmed
connector-body notch profile).

**Answer to Question F**: of the individually-inspected subset, 2/2 are
visually real. The remaining 25 share the same short, simple structural
signature and are classified `PARTIALLY_CONFIRMED` by analogy rather than
`SOURCE_CONFIRMED` — this AP does not claim to have visually verified all
27, and none were found visually contradicted.

### 7. Geometric endpoint warnings (28)

`WIRE-GEOMETRIC-ENDPOINTS` is a **Wire**-level warning (fires when neither
endpoint reached semantic resolution), not inherently a raster/label
warning. The wire-color-label-interruption hypothesis was **explicitly
checked, not assumed**, against 2 visually-inspected samples
(`wire-054e53a69a62ad22`, `wire-0db033162c44ab92`): in both, the underlying
wire geometry is real and visually unambiguous (one feeds the confirmed
MINI connector, the other feeds a confirmed connector-body notch), and nothing
in the crop suggests a label physically interrupted the raster at either
endpoint. The warning in these 2 cases reflects an **incomplete semantic
resolution step**, not a label artifact.

All 28 are classified `endpoints whose semantic terminal was not
recognized` (confidence HIGH for the 2 inspected, MEDIUM for the remaining
26 by analogy). The label-interruption phenomenon **is real** elsewhere in
this extraction (see Correlated Failure Patterns, PATTERN-001) but was not
found to be the operative cause of this specific warning class in the
samples checked.

### 8. Connector reconciliation (7)

All 7 inspected directly in the source raster:

| connector | bbox | detector topology | detector confidence | classification | confidence |
|---|---|---|---|---|---|
| `connector-a228885f25dd1f6b` | (600,289,18,63) | inline | medium | `REAL_CONNECTOR` | HIGH |
| `connector-3bb50861590cea40` | (355,352,17,26) | inline | high | `REAL_CONNECTOR` | HIGH |
| `connector-08bf96db0b914922` | (369,81,28,11) | inline | high | `FALSE_POSITIVE` | HIGH |
| `connector-fdd33a2a7f942823` | (371,249,49,10) | **unknown** | low | `FALSE_POSITIVE` | HIGH |
| `connector-7a793f0ead4a15ca` | (371,259,49,10) | component_attached | low | `FALSE_POSITIVE` | HIGH |
| `connector-0aa656ccf7f56e0e` | (365,345,46,10) | component_attached | medium | `FALSE_POSITIVE` | HIGH |
| `connector-713147ddb5e260d6` | (420,304,57,51) | component_attached | medium | `FALSE_POSITIVE` | LOW |

- `connector-a228885f25dd1f6b` and `connector-3bb50861590cea40`: genuine
  connector bodies, matching the project's notched/interlocking connector
  profile, labeled `[MINI]`.
- `connector-08bf96db0b914922`: a **diode symbol** (rectangle + triangle)
  inline on a labeled wire — not a connector body.
- `connector-fdd33a2a7f942823` (**the `unknown`-topology connector, given
  special attention per the AP**): its bounding box contains the literal
  text characters **"G/R"** (a wire-color code) sitting directly on a
  horizontal wire. There is no connector body here at all. `topology:
  unknown` is the detector's own admission that the notch/pass-through
  geometry it found does not match either recognized topology — consistent
  with this being detector noise from text ink, not a real connector of
  indeterminate topology.
- `connector-7a793f0ead4a15ca` and `connector-0aa656ccf7f56e0e`: the same
  phenomenon, single wire-color-code characters on wire ink, one row above
  and one row away from the `unknown` connector respectively.
- `connector-713147ddb5e260d6`: the largest of the 7 (57×51), in a dense
  switch-matrix staircase-routing area. Visible geometry is consistent with
  stepped wire routing rather than a connector body, but this read is lower
  confidence than the other 6.

**Answer to Question G**: **no** — only 2 of 7 (29%) are legitimate
connectors. 5 of 7 (71%) are false positives, dominated by wire-color-code
text labels sitting on wire ink (3 cases) plus one diode symbol and one
lower-confidence switch-matrix geometry case. The `unknown`-topology
connector is specifically a text-label false positive, not an
indeterminate-but-real connector.

## Correlated Failure Patterns

```
PATTERN-001
description: Wire-color-code text labels (1-3 printed characters) sitting
  directly on conductor ink are detected as connector bodies by
  ConnectorGeometryDetector, and independently may coincide with topology
  breaks at the same location.
affected object classes: connector_reconciliation (FALSE_POSITIVE),
  endpoint_reconciliation (WIRE_INTERRUPTION, 1 sampled case)
object IDs: connector-fdd33a2a7f942823, connector-7a793f0ead4a15ca,
  connector-0aa656ccf7f56e0e, endpoint-candidate-6a653f03c6888e27
source evidence: direct crops of samples/trx300ODG.png at each bounding
  box show printed wire-color-code text ("G/R", single characters)
  overlapping wire ink, not a connector body
likely shared boundary: text/label ink overlapping conductor raster is a
  single underlying visual condition that both misleads
  ConnectorGeometryDetector's contour/notch matching and can locally break
  conductor-detection continuity
confidence: MEDIUM-HIGH (3 connector misdetections directly confirmed; the
  endpoint coincidence checked for only 1 case)

PATTERN-002
description: ConnectorGeometryDetector's notch/pass-through matching also
  triggers on non-connector symbols that share a similar rectangle+notch
  silhouette - specifically a diode symbol (rectangle + triangle).
affected object classes: connector_reconciliation (FALSE_POSITIVE)
object IDs: connector-08bf96db0b914922
source evidence: direct crop shows a rectangle-plus-triangle diode symbol
  on a labeled wire, not a connector body
likely shared boundary: geometric silhouette overlap between the
  project's connector-body notch profile and a diode schematic symbol
confidence: MEDIUM (single directly-confirmed instance; not established
  whether other diodes in the diagram produce the same false positive)

PATTERN-003
description: Small circular_symbol component candidates (mostly <=22x20px)
  are predominantly filled junction/connection dots at wire intersections
  in dense grid areas, not discrete electrical components, and
  structurally never acquire TerminalCandidate records.
affected object classes: component_reconciliation (DIAGRAM_SYMBOL)
object IDs: 21 components, including component-candidate-shape-region-
  02445f041ac03529, -026ddcef0511ad1d, -bbf83caa39a83a48 (directly
  confirmed) and 18 more by structural analogy (see JSON artifact)
source evidence: 3 direct crops in 3 different diagram regions, all
  showing a filled circular junction dot at a wire intersection
likely shared boundary: this single misclassification explains the
  overwhelming majority (21/23, 91%) of the "components without terminal
  evidence" population - it is not 21 independent terminal-detection
  failures
confidence: HIGH

PATTERN-004
description: Topology "crossing" nodes (explicitly defined as having no
  electrical meaning) fragment real, continuous conductor geometry into
  many small topology edges/segments that individual Wire objects do not
  enumerate as "owned" in their topology_edges/conductor_segments lists.
affected object classes: topology_edge_reconciliation
  (SHARED_CONDUCTOR_PATH), conductor_segment_reconciliation
  (SHARED_BRANCH_GEOMETRY)
object IDs: 406 topology edges + 127 conductor segments (see JSON artifact
  classification_counts and per-object detail)
source evidence: numeric pattern established by a full-population
  structural join (not sampling); partially corroborated by the
  crossing-adjacent zero-wire endpoint visual samples in PATTERN-005
likely shared boundary: this single structural mechanism (how the
  topology graph represents visual crossings vs. how Wire ownership lists
  are populated) explains the majority (67%) of both the 602-edge and
  189-segment "unowned/topology-only" populations - not two independent
  defect classes
confidence: MEDIUM (strong numeric correlation; the exact ownership-
  enumeration code path was not inspected in this AP, only the outcome)

PATTERN-005
description: The 117 zero-Wire dead-end endpoints, 117 of the 602 unowned
  topology edges, and 91 of the 153 net-outside endpoints are not four
  independent defects - they are frequently the SAME underlying boundary
  (a conductor_end node where Wire reconstruction stopped) counted once
  each in three different coverage metrics.
affected object classes: endpoint_reconciliation (WIRE_INTERRUPTION /
  COMPONENT_TERMINAL), topology_edge_reconciliation
  (VALID_CONDUCTOR_NOT_RECONSTRUCTED), net_endpoint_reconciliation
  (FOLLOWS_FROM_ZERO_WIRE)
object IDs: the full 117-object zero-Wire endpoint population and its 117
  directly-incident topology edges (exact 1:1 join); 91 of these same
  endpoints also appear in the net-outside population
source evidence: exact ID-level joins against extraction_audit.json (not
  a sample); 4 of the 117 endpoints additionally visually confirmed as
  real electrical dead-ends (see Section 1)
likely shared boundary: a single incomplete-Wire-reconstruction boundary
  at each affected conductor_end node, not three/four separate problems
confidence: HIGH for the correlation itself (exact joins); MEDIUM for why
  reconstruction stops at each specific boundary (partially visually
  sampled, not exhaustively)
```

## Source-Confirmed Findings

Directly visually confirmed against `00_source.png` (and cross-referenced
against other review layers) in this AP:

- `endpoint-candidate-d795bf8dbfc260d3`, `endpoint-candidate-6a653f03c6888e27`,
  `endpoint-candidate-f116d72e55f48cc6` — real electrical dead-ends /
  component terminal, not detection artifacts.
- `component-candidate-shape-region-02445f041ac03529`, `-026ddcef0511ad1d`,
  `-bbf83caa39a83a48` — junction dots, not components.
- `component-candidate-shape-region-208074f9e8b89bbc` (CDI UNIT),
  `-d086dcb89ca28a4d` (BATTERY) — real components with visible,
  unrecognized terminals.
- `wire-054e53a69a62ad22`, `wire-2498c8429f52632b` — real, visually
  confirmed Wires despite being "fully unresolved."
- `connector-a228885f25dd1f6b`, `connector-3bb50861590cea40` — real
  connector bodies.
- `connector-08bf96db0b914922`, `connector-fdd33a2a7f942823`,
  `connector-7a793f0ead4a15ca`, `connector-0aa656ccf7f56e0e` — false-positive
  connectors (diode / text-label ink).

## Artifact Integrity Correction

AP-DIAG-018A corrected the aggregate population count to **1,146 records**, deduplicated the visual sample list to **19 unique object IDs**, and recorded **16 distinct source-confirmed object IDs**. The regression test now derives exact machine-defined populations from the same-run extraction audit, rejects duplicate IDs, and requires stable `object_id` ordering.

## Undetermined Findings

107 objects across the populations were left `UNDETERMINED`/`AMBIGUOUS` or
kept at `LOW` confidence rather than forced into a classification without
adequate evidence (full list in
`artifacts/audit/source_object_reconciliation.json#undetermined_objects`).
The largest blocks: 44 topology edges and 62 conductor segments involving
`continuation`↔`continuation` node pairs (no confident structural pattern
found and not visually sampled), and `connector-713147ddb5e260d6` (LOW
confidence false-positive read in a visually dense area).

## Next AP

Not opened here, per the "do not open a detector-fix AP merely because an
object is unowned" instruction. Candidates a future AP could scope,
strictly based on what this reconciliation established:

- The wire-color-label / diode false-positive pattern in
  `ConnectorGeometryDetector` (PATTERN-001/002) — 5 of 7 currently-detected
  connectors in this run are false positives.
- The `circular_symbol` vs. junction-dot ambiguity in component candidate
  classification (PATTERN-003) — 21 of 34 component candidates in this run
  are junction dots, not components.
- The separate net-resolution gap for endpoints that own a Wire but no net
  (62 objects, Section 5) — independent of the zero-Wire population.

None of these should be scoped without first re-confirming against a full,
individually-inspected population (this AP sampled, it did not exhaustively
verify every object in the larger populations).

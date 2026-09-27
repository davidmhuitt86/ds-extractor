# AP-DIAG-AUDIT-008B — TRX300 Engineering Ground-Truth Extension

This is an audit document. **No production classifier, resolver, extractor,
topology, terminal, ground, connector, scope, or model code was modified
to produce it.** All findings come from read-only inspection of the
unmodified `WireModel` (via the existing `dx-audit-component-semantic`
tool from AP-DIAG-AUDIT-008 and the standard `dx-extract` CLI), direct
source-code reading of the resolution/reconstruction logic, and direct
pixel inspection of `samples/trx300ODG.png`. No new tool was needed;
this AP extends AP-DIAG-AUDIT-008's findings with newly-supplied manual
engineering ground truth rather than repeating that audit's work.
`git status --short` was empty before this document and its companion
JSON artifact were added.

## Starting State

- `git rev-parse HEAD`: `62131871ae3fd233d46d3772fc12f6d3ecb9ebde`... — verified
  actual value: `39d5ab80283a4526c748f62ab31fda48ccdffde6` (matches the
  task's expected `39d5ab8`).
- `git branch --show-current`: `main`; `git status --short`: empty.
- Baseline re-verified via rebuild + `ctest`: 61/61 tests, assertions
  active, 8 compiler warnings, 34 runtime warnings, 37 physical wires,
  12 electrical nets, 6/6 ChassisGround endpoints, 81/0/28/53
  ComponentCandidate/resolved/unresolved/rejected, 0 validation errors —
  all match the task's expected baseline exactly.

## Manual Ground Truth (Reference Only)

| Item | Manual value |
|---|---:|
| Electrical components/modules (includes 2 fuses) | 24 |
| Component/module terminals | 64 |
| Inline connectors | 8 |
| Connector terminals | 46 |
| Splices | 15 |
| Chassis-ground locations | 8 |
| Total non-ground terminals (64+46) | 110 |

These values are used only as a comparison reference throughout this
document, never as a target the extractor was tuned toward.

## Engineering-Graph Matrix

| Item | Manual | Extracted | Status |
|---|---:|---:|---|
| Components/modules | 24 | 0 resolved (81 candidates, 28 unresolved) | audited |
| Component terminals | 64 | 12 (ComponentBoundary TerminalCandidates) | audited |
| Inline connectors | 8 | 0 | audited |
| Connector terminals | 46 | 0 | audited |
| Splices | 15 | 105 (raw `TopologyNodeType::Splice` nodes) | audited, not directly comparable |
| Ground locations | 8 | 6 resolved (+1 misclassified, +2 missing = 9 raster locations found) | audited |
| Physical wires | — | 37 | audited against topology rules only |
| Electrical nets | — | 12 | not independently re-audited (see AP-DIAG-AUDIT-007) |

## 1. Component Count Audit

**Headline finding**: of the 28 unresolved `ComponentCandidate`s, only
**2** — `component-candidate-shape-region-0771bb11fe6c347d` (visually a
battery enclosure) and `component-candidate-shape-region-16df4d7df3b94a27`
(visually a CDI-unit enclosure) — directly correspond to a real
component's own drawn symbol body.

Direct visual cross-referencing of the source raster against every one
of the 81 `ComponentCandidate` bounding boxes found **no candidate at
all**, of any kind, for the following labeled component bodies: Alarm
Unit, Ignition Switch (enclosure body), D.C. Consent, Rectifier,
Regulator/Rectifier, the Indicators module housing, Tail Light,
Headlights (×2), Ignition Coil (symbol body), Reverse Switch (symbol
body), Neutral Switch (symbol body), Oil Temperature Sensor (symbol
body), Pulse Generator (symbol body), Alternator (symbol body), Starter
Relay Switch (symbol body), Starter Motor (symbol body), Main Fuse, Sub
Fuse, Starter Switch (symbol body), and Spark Plug. In several of these
cases (Reverse Switch, Neutral Switch, Oil Temperature Sensor, Pulse
Generator, Alternator, Starter Motor) the *only* nearby candidate is an
adjacent, separately-drawn chassis-ground bar mark (Section 6) — not the
switch/sensor/generator symbol itself.

Two more candidates, `bfae05a427189376` (a multi-lead boxed body with 7
terminals and 2 connected physical Wires) and `c98f6566b0672535` (a
small strap between two adjacent legs of what appears to be a
multi-position fuse/terminal block), may correspond to real objects or
fragments of them, per AP-DIAG-AUDIT-008's own findings — this remains
unconfirmed.

**No multiple-candidates-for-one-object situation was found among the
28** beyond the fragment suspicion AP-DIAG-AUDIT-008 already documented
for `0a02724ece05df31`/`9c4d423986244e04` (two small bracket shapes,
each above a different round zigzag symbol) and
`9c4d423986244e04`/`c98f6566b0672535`.

**Both fuses named in the manual ground truth have zero candidate
representation.** Direct inspection of their exact drawn location
(~864-885, 437-457) found only one nearby candidate,
`c2335cc575d107b1` — already established by AP-DIAG-AUDIT-004/007/008 as
the confirmed false-positive annotation-leader dot for the "SUB FUSE
15A" text label, not either fuse's own zigzag symbol body. This directly
confirms, from the extraction side, exactly the object class the manual
ground truth specifically called out as previously under-counted.

## 2. Component-Terminal Audit

18 `TerminalCandidate`s exist in the entire model: 12 `ComponentBoundary`
kind (attributed to exactly 5 of the 28 unresolved candidates —
`bfae05a427189376` owns 7, `c98f6566b0672535` owns 2,
`763aea0b9f4edcf6`/`845947b0afd7451a`/`9c4d423986244e04` own 1 each) and
6 `GroundConnection` kind (one per genuine `ChassisGround`). **0** are
`ConnectorBoundary` kind, and **0** are `Unknown` kind. All 18 have a
non-empty `component_candidate_id` (0 unowned) and reference 18 distinct
`endpoint_id`s (0 duplicates).

**12 of the reference 64 component/module terminals have a
`TerminalCandidate`/`EndpointCandidate`.** Per this task's own caution,
this is reported as a raw extraction-layer count, not a validated 1:1
correspondence with the manually-counted physical terminals — no
attempt was made to match individual terminal candidates to specific
named terminals on specific named components.

## 3. Connector Audit

`ConnectorCandidate`/`ConnectorTerminal` remain **0/0**. Root cause
traced precisely through the code (not assumed):

- `ConnectorTerminalModelBuilder` (`connector_terminal_model.cpp:46`)
  only ever materializes a `ConnectorCandidate` from a `TerminalCandidate`
  whose `kind == TerminalCandidateKind::ConnectorBoundary`.
- That kind is assigned, in both `terminal_location_detector.cpp:158`
  and `terminal_recognizer.cpp:82`, if and only if the owning
  `ComponentCandidate.kind == ComponentCandidateKind::PrimitiveSymbol`.
- **Zero of the 81 current `ComponentCandidate`s have kind
  `PrimitiveSymbol`** (confirmed directly: the full kind distribution is
  `diagram_furniture`=47, `circular_symbol`=26, `chassis_ground`=6,
  `enclosure`=2 — these sum to 81, leaving 0 for `PrimitiveSymbol`).
- `PrimitiveSymbol` itself (`component_candidate_classifier.cpp:18`) is
  only assigned to a `ShapeKind::Rectangle` shape whose `ShapeRole` is
  **not** `Enclosure`. On TRX300, apparently every rectangle
  `ShapeDetector` found received the `Enclosure` role (only 2
  `Enclosure`-kind candidates exist; 0 non-enclosure rectangles).

**Evidence-gap classification**: this is **absent connector
classification**, specifically an upstream `ShapeRole` assignment gap —
not absence of candidate geometry (81 candidates exist), not primarily a
terminal-extraction gap, and not an ownership gap. The classification
path that would ever produce a connector-eligible terminal is simply
never reached on this diagram. **0 of the 8 reference connectors and 0
of the 46 reference connector terminals have any representation.**

## 4. Splice Audit

Current topology node types: `Splice`=105, `Junction`=0, `Crossing`=239,
`Continuation`=139, `ConductorEnd`=207 (690 total).

The classification rule (`topology_reconstructor.cpp`) promotes a node
to `Splice` whenever it is electrically connective and has graph degree
≥3 after edge splitting — a purely geometric/topological test, with no
distinction between a deliberately-drawn junction dot and an incidental
3-way meeting of ordinary bus/drop-wire geometry.

**The 105 `Splice` nodes substantially exceed (7×) the 15 manually-
identified genuine splice locations.** Every genuine drawn splice
necessarily has degree ≥3 and would therefore be classified `Splice`,
so the 15 real locations are very likely a *subset* of the 105 — but the
current topology provides no mechanism to distinguish which ~15 of the
105 are the manually-identified genuine engineering splices from the
remaining ~90 ordinary multi-wire meeting points. This audit does not
attempt to enumerate that subset, since doing so for all 105 nodes would
require per-node visual verification beyond this AP's scope and risks
exactly the kind of unsupported assertion the task prohibits.

## 5. Chassis-Ground Audit

The 6 already-resolved genuine `ChassisGround` components correspond,
via direct visual inspection, to: Reverse Switch (`0edf5ce35037fec3`),
Oil Temperature Sensor (`1acbaeb7ac6ac87a`), Pulse Generator
(`791276441805b2e0`), Alternator (`5aa211846bb7891d`, AP-DIAG-FIX-007
recovered), Starter Motor (`6b6ccc2d59afe578`, AP-DIAG-FIX-007
recovered), and Battery (`a434a925670e9b65`).

Direct raster inspection of the rest of the diagram — comparing every
candidate ground-bar-shaped mark's pixel pattern against the 6 confirmed
genuine symbols — found **three further locations bearing the
identical visual pattern** (a thick bar followed by a fainter, shorter
second bar, matching the decreasing-width chassis-ground convention
established in AP-DIAG-AUDIT-006):

1. **Near the "STARTER SWITCH" label** (~425,537,17,16): **has** a
   `ComponentCandidate` (`763aea0b9f4edcf6`), but it is classified
   `circular_symbol`, never `chassis_ground`. This is a shape-
   classification gap, not a missing-candidate gap.
2. **Below the Neutral Switch symbol** (~600,586,20,13): **no**
   `ComponentCandidate` exists anywhere near this location — confirmed
   by an exhaustive search of all 81 candidate bounding boxes.
3. **Below the Spark Plug arrow symbol** (~467,612,20,11): **no**
   `ComponentCandidate` exists anywhere near this location — confirmed
   the same way.

**This yields 9 distinct locations bearing the chassis-ground visual
pattern (6 correctly detected + 1 misclassified + 2 entirely missing),
one more than the manually-reported 8.** This discrepancy is reported,
not resolved: it is not assumed that the manual count of 8 is wrong, nor
that one of these 9 raster observations is not a true chassis-ground —
either is possible, and distinguishing them would require engineering
judgment (e.g. whether the Starter-Switch-area ground is a physically
distinct frame point or shares an already-counted location) this audit
will not fabricate. **No production change was made based on this
finding**, and no attempt was made to force 9 down to 8 or up to a
"corrected" number.

## 6. Wire Crossing Audit

**Orthogonality guarantee (verified by direct code inspection, not
assumed)**: `MorphologyWireDetector` extracts conductor geometry
exclusively via a 25×1 horizontal and 1×25 vertical `MORPH_RECT`
structuring element — every `ConductorSegment` it produces is by
construction horizontal or vertical. `ConductorNormalizer` independently
re-classifies every segment via `is_horizontal()`/`is_vertical()` (0.5px
tolerance) before any merge; a segment satisfying neither is left
unmerged, never reclassified or rotated. The only code path capable of
producing a genuinely diagonal `ConductorSegment` is
`GroundApproachConductorRecovery` (AP-DIAG-FIX-007), and
AP-DIAG-AUDIT-007 already directly verified that both of its diagonal
sub-segments are rejected by `ConductorEvidenceEvaluator` before joining
any Wire — only the final straight run survives into each of the two
recovered Wires.

**Direction change at crossings — verified impossible by code
inspection**: `WireReconstructor`'s traversal
(`wire_reconstructor.cpp` lines ~117-124) stops unconditionally at any
node whose incident-edge count is not exactly 2 (the code's own comment:
*"A true endpoint must have exactly one incident edge. A continuation
has exactly two. Any distribution node has three or more..."*). A
`Crossing`-type node always has degree 4 (two edges from each of the two
crossing conductors) and is therefore structurally unreachable as an
intermediate hop in any Wire's path — the walk stops before it. This is
a pure degree check, independent of the node's `electrically_connective`
flag.

**X-crossing wire transfer — verified impossible for the same reason**:
no mechanism in the current algorithm could hop a Wire's path from one
crossing conductor onto the other, since doing so would require
traversing the shared degree-4 node, which the degree check forbids.

**L-corners**: a degree-2 `Continuation` node (exactly two incidents
meeting at one point) is walked through by `WireReconstructor`
regardless of the two segments' relative orientation, so a genuine
L-corner (a horizontal segment ending exactly where a vertical segment
begins) is correctly represented as a single continuous Wire with a
direction change at its own explicit geometry — matching the domain
rule exactly.

**Crossing-gap false positives**: the 12 AP-DIAG-AUDIT-008 suspected
false positives (found via direct raster inspection: a bounding box at a
plain bus/grid crossing with no drawn circle) are confirmed, on
re-inspection here, to match the exact mechanism AP-WIRE-FIX-002
previously and partially corrected for 3 different coordinates — these
are additional, unaddressed instances of that same known false-positive
class, not a new or different mechanism.

## 7. Answers to the Required Questions

1. **How many of the 24 actual components have any current candidate
   representation?** At most 2 confirmed (Battery, CDI Unit) + 2 more
   possible but unconfirmed (`bfae05a427189376`, `c98f6566b0672535`).
   The remaining ~20 real components' own symbol bodies have no
   `ComponentCandidate` at all.
2. **How many of the 64 component terminals are currently
   represented?** 12 (raw `TerminalCandidate` count, not a validated
   1:1 correspondence).
3. **How many of the 8 inline connectors have candidate
   representation?** 0.
4. **How many of the 46 connector terminals are represented?** 0.
5. **How many of the 15 splices are represented?** Likely all 15, as an
   unidentified subset of the 105 raw `Splice`-classified nodes; cannot
   be enumerated without fabricating identifications this audit
   declined to make.
6. **Which 8 chassis-ground locations are represented/missing?** 6
   correctly detected (Reverse Switch, Oil Temperature Sensor, Pulse
   Generator, Alternator, Starter Motor, Battery); 1 has a candidate but
   is misclassified (near Starter Switch); 2 have no candidate at all
   (Neutral Switch, Spark Plug). This totals 9 raster-confirmed
   locations against a manual reference of 8 — an unresolved
   discrepancy, reported as observed.
7. **Does any current Wire change direction at an intersection?** No —
   structurally impossible per the degree-2-only traversal rule verified
   directly in `wire_reconstructor.cpp`.
8. **Does any X crossing cause an incorrect Wire transfer?** No — same
   structural guarantee.
9. **Does any semantic Wire contain a diagonal segment?** No — verified
   for all 37 Wires via the extraction/normalization pipeline's
   structural guarantees plus AP-DIAG-AUDIT-007's direct prior
   verification of the 2 AP-DIAG-FIX-007-recovered Wires.
10. **Which evidence layer is currently preventing semantic
    reconstruction?** Predominantly **upstream shape/symbol detection**,
    not semantic resolution: the overwhelming majority of the 24 real
    components' own symbol bodies never produce a `ComponentCandidate`
    at all, prior to and independent of `SymbolFamily` recognition,
    terminal attribution, or OCR. Connector detection is blocked one
    layer further upstream still (a `ShapeRole` classification gap). No
    single fix closes this: upstream geometry/shape classification must
    exist before terminal-lead extraction can matter, which must exist
    before text/OCR's absence becomes the binding constraint for the
    candidates that do get detected.

## 8. Regression

Re-verified via rebuild + `ctest`: 61/61 tests, 8 compiler warnings (0
new), 34 runtime warnings, 37 physical wires, 12 electrical nets, 6/6
ground endpoints, 0 validation errors, 81/0/28/53
ComponentCandidate/resolved/unresolved/rejected — every value identical
to the AP-DIAG-AUDIT-008 baseline and the task's expected values. No
production file was modified.

## Explicit Non-Changes

No change to: `ComponentCandidate` extraction, `ElectricalComponentResolver`
behavior, `SymbolFamily` recognition, `TopologyReconstructor`,
`WireReconstructor`, `ConductorNormalizer`, `MorphologyWireDetector`,
`ConnectorTerminalModelBuilder`, `ShapeDetector`, ground/terminal/
connector detection, extraction scope, OCR, or any existing export.

## Files Changed

`docs/AP-DIAG-AUDIT-008B_TRX300_Ground_Truth_Extension.md` (new),
`artifacts/trx300/component_semantic_ground_truth_audit.json` (new). No
other file was touched.

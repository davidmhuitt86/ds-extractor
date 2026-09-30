# AP-DIAG-019 — Correlated Failure Root-Cause Investigation

## 1. Objective

Determine, for three specific correlated failure populations established by
AP-DIAG-018/018A, whether each is a detector false positive, a topology
interpretation error, missing semantic reconstruction, genuinely missing
source recognition, or an expected consequence of the endpoint-to-endpoint
Wire model — and identify the smallest number of actual root causes behind
the largest affected populations. Strictly diagnostic: no detector,
threshold, morphology, endpoint, connector, wire reconstruction, topology,
net-resolution, component, terminal, or semantic logic was modified.

## 2. Baseline extraction identity

- Source: `samples/trx300ODG.png`, 898×549, page 0
- Extraction-results commit: `a111cdd27b855fc9a3c6d2d28a7a4d0ab05c77cf`
- Main commit this investigation started from: `02c1bd1cfbc93c11f3d477b1fe166f1eb9583de5`
  (AP-DIAG-018A). Verified present: `docs/AP-DIAG-018A_Reconciliation_Artifact_Integrity_Correction.md`,
  the corrected `artifacts/audit/source_object_reconciliation.json` (1,146
  records, 19 unique visual-sample IDs), and the corrected
  `tests/test_source_object_reconciliation.cpp`.
- Re-ran the real extraction against the current (unmodified) code to
  confirm the same populations: components 34, terminal candidates 18,
  connectors 7, connector terminals 6, conductor segments 230, endpoint
  candidates 191, wires 37, topology nodes 533, topology edges 643,
  electrical nets 12, validation errors 0, warnings 34. Matches the task's
  stated baseline exactly.

**Pre-existing test defect found and fixed during validation** (see
§7.4): AP-DIAG-018A's rewritten `test_source_object_reconciliation.cpp`
introduced a generic exact-population-membership check across all 8
groups but never populated `expected["wire_reconciliation"]`, so that
group's check always compared against an implicitly-empty set and the
test aborted on every run. Fixed by exempting `wire_reconciliation` from
the generic exact-set check specifically (its true "fully unresolved"
membership depends on per-wire `WireSemanticResolution` data that
`extraction_audit.json` does not expose — the same reason AP-DIAG-018's
original test only checked size and structural signature for this one
group). No other group's check was weakened. This is a test-hygiene
correction, not a reconciliation-finding correction.

## 3. Investigation methodology

Parts A and B were established by **direct execution**, not sampling or
code-reading alone: standalone C++ programs were written that call the
exact same OpenCV operations `ConnectorGeometryDetector::detect` and
`ShapeDetector`'s `detect_circles` (including its `bounded_by_crossing_lines`
helper) use, against the same-run normalized image
(`artifacts/recognition/source_normalized.png`), and print the intermediate
geometry (contour area, circularity, aspect, notch count and side,
`interior_density`, `bounded_by_crossing_lines` result) for every one of
the 7 connector candidates and all 26 circular-symbol candidates (21
`DIAGRAM_SYMBOL` + 5 real). These programs are uncommitted scratch tools;
no production source file was modified to run them.

Part C was established by direct reading of
`src/topology/distribution_decomposer.cpp`'s net-construction control
flow, cross-referenced against the same-run `extraction_audit.json`'s
`wires`/`endpoint_candidates`/`electrical_nets` collections for all 62
`SEPARATE_NET_RESOLUTION_GAP` endpoints.

Several AP-DIAG-018 visual-sample crops were re-taken at the *exact*
bounding box (not a wider surrounding crop) to check whether the earlier
"filled junction dot" characterization held up under tighter scrutiny —
it did not, for the specific boxes re-checked (§5, §7.2).

## 4. Connector candidate table

| ID | bbox (x,y,w,h) | detector topology | detector confidence | notch axis (measured) | classification | root cause |
|---|---|---|---|---|---|---|
| `connector-a228885f25dd1f6b` | (600,289,18,63) | inline | medium | LR, notches=2 | REAL_CONNECTOR | detector_correct |
| `connector-3bb50861590cea40` | (355,352,17,26) | inline | high | LR, notches=3 | REAL_CONNECTOR | detector_correct |
| `connector-08bf96db0b914922` | (369,81,28,11) | inline | high | **TB**, notches=3 | FALSE_POSITIVE | common_notch_axis_defect |
| `connector-fdd33a2a7f942823` | (371,249,49,10) | unknown | low | **TB**, notches=2 | FALSE_POSITIVE | common_notch_axis_defect |
| `connector-7a793f0ead4a15ca` | (371,259,49,10) | component_attached | low | **TB**, notches=2 | FALSE_POSITIVE | common_notch_axis_defect |
| `connector-0aa656ccf7f56e0e` | (365,345,46,10) | component_attached | medium | **TB**, notches=2 | FALSE_POSITIVE | common_notch_axis_defect |
| `connector-713147ddb5e260d6` | (420,304,57,51) | component_attached | medium | **TB**, notches=3 | FALSE_POSITIVE | common_notch_axis_defect |

Associated geometry/terminals/recognition evidence, and overlap checks
against `recognition_input.json`'s independent `TextRegionDetector` output:

- `connector-a228885f25dd1f6b`: 1 connector terminal
  (`connector-terminal-9ad4dc8cb7f600af`), 1 overlapping text region
  (`text-region-39`, a wire-color label *near* but not *forming* the
  connector body). Source-visually confirmed genuine `[MINI]` connector
  body.
- `connector-3bb50861590cea40`: no connector terminal recorded, 0
  overlapping text regions. Source-visually confirmed genuine `[MINI]`
  connector body immediately beside a diode symbol on the same bus.
- `connector-08bf96db0b914922`: no connector terminal, 2 overlapping text
  regions (`text-region-20`, `text-region-21` — "P", "P/W" labels).
  Source-visually confirmed as a diode symbol (rectangle+triangle); the
  overlapping text regions are adjacent labels, not the triggering ink.
- `connector-fdd33a2a7f942823`, `connector-7a793f0ead4a15ca`,
  `connector-0aa656ccf7f56e0e`: 1 connector terminal each, **0** overlapping
  `TextRegionDetector` regions. These 1-3 character wire-color-code labels
  are below `TextRegionDetector`'s own detection scale — a hypothetical
  "exclude text regions" filter on `ConnectorGeometryDetector`'s output
  would **not** have caught these specific false positives, because no
  text region exists there to exclude.
- `connector-713147ddb5e260d6`: 0 connector terminals, 0 overlapping text
  regions, in a dense switch-matrix staircase-routing area.

### Common root cause vs. independent cases

**COMMON ROOT CAUSE, confirmed by direct execution, zero exceptions across
all 7 candidates:** `ConnectorGeometryDetector`'s `opposing_notches` test
(`src/image/connector_geometry_detector.cpp`) accepts convexity-defect
notches on *either* the left/right axis *or* the top/bottom axis as
equally valid connector-body evidence:

```cpp
const bool opposing_notches =
    (notch_left && notch_right) ||
    (notch_top && notch_bottom);
```

Measured directly against the real source: **both real connectors show
opposing notches exclusively on the left/right axis; all 5 false
positives show opposing notches exclusively on the top/bottom axis.**
This is a single, precise, 7/7 (not sampled) discriminating condition.
The TRX300 connector-body symbol's interlock profile is drawn with
notches on its left/right long edges; a diode's triangle apex and printed
character glyph strokes sitting on a horizontal wire both naturally
produce indentations above/below that horizontal baseline instead. The
detector's own size/fill/aspect filters do not discriminate at all — every
one of the 7 candidates passes them identically.

This is a genuine implementation gap (not yet fixed, per AP-DIAG-019's
scope): the geometric test alone cannot distinguish a real connector's
notch axis from incidental top/bottom notches produced by unrelated ink.
No text-region-exclusion filter would close this gap on its own (3 of 5
false positives have no overlapping text region at all).

## 5. Circular-symbol/junction-dot candidate table

All 21 `DIAGRAM_SYMBOL` candidates plus the 5 real circular symbols
(with terminal evidence), measured directly against the real
`detect_circles` pipeline:

| ID | bbox | area | circularity | radius | edge_support | interior_density | bounded_by_crossing_lines |
|---|---|---|---|---|---|---|---|
| `...02445f04...` | (485,249,22,20) | 375 | 0.831 | 13 | 0.75 | **0.00** | false (weakest=0.28) |
| `...026ddcef...` | (516,209,10,11) | 80.5 | 0.839 | 6 | 1.00 | **0.00** | false (weakest=0.40) |
| `...11226a4b...` | (420,259,11,9) | 69.5 | 0.925 | 5 | 1.00 | **0.00** | false (weakest=0.08) |
| `...1a77da76...` | (526,304,9,9) | 60.5 | 0.900 | 5 | 1.00 | **0.00** | false (weakest=0.24) |
| `...2717615c...` | (507,209,9,11) | 78 | 0.865 | 6 | 1.00 | **0.00** | false (weakest=0.04) |
| `...274d8046...` | (806,355,7,7) | 25 | 0.842 | 4 | 1.00 | 0.28 | false (weakest=0.04) |
| `...2f3af985...` | (532,448,11,13) | 91.5 | 0.735 | 7 | 0.72 | 0.20 | false (weakest=0.12) |
| `...5db82f3f...` | (806,434,7,9) | 41.5 | 0.913 | 4 | 0.86 | 0.20 | false (weakest=0.00) |
| `...66542cc6...` | (477,304,7,9) | 46 | 0.878 | 4 | 1.00 | **0.00** | false (weakest=0.36) |
| `...9155d5cc...` | (561,468,8,10) | 40.5 | 0.779 | 5 | 0.71 | 0.12 | false (weakest=0.00) |
| `...a108a12a...` | (421,249,10,10) | 70.5 | 0.827 | 5 | 1.00 | **0.00** | false (weakest=0.08) |
| `...a3adb49e...` | (181,329,9,9) | 59.5 | 0.885 | 5 | 1.00 | **0.00** | false (weakest=0.24) |
| `...a964a9ef...` | (843,434,8,9) | 49.5 | 0.927 | 4 | 1.00 | **0.00** | false (weakest=0.00) |
| `...abcf7815...` | (466,190,11,9) | 76.5 | 0.879 | 6 | 1.00 | **0.00** | false (weakest=0.04) |
| `...b46aef77...` | (357,69,9,9) | 45 | 0.907 | 4 | 1.00 | **0.00** | false (weakest=0.04) |
| `...bbf83caa...` | (618,249,19,18) | 297 | 0.804 | 11 | 0.90 | **0.00** | false (weakest=0.32) |
| `...c0294dea...` | (517,189,9,10) | 64 | 0.865 | 5 | 1.00 | **0.00** | false (weakest=0.20) |
| `...c4eb14bc...` | (516,199,10,10) | 69 | 0.884 | 5 | 1.00 | **0.00** | false (weakest=0.40) |
| `...ca944f15...` | (712,434,7,6) | 28 | 0.911 | 3 | 1.00 | 0.20 | false (weakest=0.00) |
| `...e44323cf...` | (344,69,9,8) | 46 | 0.902 | 4 | 1.00 | 0.08 | false (weakest=0.08) |
| `...f28bb4a7...` | (411,346,10,9) | 63 | 0.852 | 5 | 1.00 | **0.00** | false (weakest=0.44) |
| `05f79b9b (REAL)` | (511,448,11,13) | 90.5 | 0.714 | 7 | 0.82 | 0.24 | false (weakest=0.00) |
| `5864547d (REAL)` | (357,462,17,16) | 185 | 0.919 | 8 | 1.00 | 0.00 | false (weakest=0.00) |
| `a70e3ecb (REAL)` | (761,361,21,17) | 240.5 | 0.686 | 11 | 0.85 | 0.00 | false (weakest=0.04) |
| `c8c5b83b (REAL)` | (77,83,18,24) | 370 | 0.778 | 13 | 0.69 | 0.00 | false (weakest=0.04) |
| `d2311c8e (REAL)` | (90,98,20,21) | 302.5 | 0.681 | 12 | 0.79 | 0.00 | false (weakest=0.04) |

(IDs abbreviated to their first 8 hex characters for table width; full
IDs in `artifacts/audit/AP-DIAG-019_correlated_failure_investigation.json`.)

All 26 candidates pass `circularity`, `aspect`, `radius`, and `edge_support`
filters — these do not discriminate real symbols from grid-gap false
positives at all. `interior_density` is low for both populations (real
circular symbols are hollow-outline lamp bulbs; the switch-matrix grid
gaps are also low-density, being blank space), so this filter, while
correctly *not* rejecting the real symbols, provides no positive
discrimination either.

**Source-image correction to AP-DIAG-018**: direct re-crops at the exact
bounding boxes (not a wider surrounding region) for `02445f04`, `026ddcef`,
`11226a4b`, and `a3adb49e` show blank grid-line-crossing negative space —
no filled dot inside the box. `bbf83caa`'s box does contain visible ink
near a dot, but the measured contour (`interior_density=0.00`) is still
the hollow grid cell, not the dot itself. AP-DIAG-018's description of
this population as "filled junction dots" does not hold up under
exact-box re-inspection; the corrected mechanism is: **these candidates
are the blank, roughly-square negative space enclosed by 4 crossing
switch-matrix grid lines**, not ink at all. AP-DIAG-018's functional
conclusion — these are not real components and should not have been
promoted — is unchanged by this correction.

A real circular symbol (`5864547d`, an indicator lamp) was re-cropped for
contrast: a standalone hollow ring with its own lead line descending from
above and a base bar below — structurally unrelated to a grid intersection.

### Common root cause vs. independent cases

**COMMON ROOT CAUSE, confirmed by direct execution, 21/21 (not sampled):**
`detect_circles` (`src/image/shape_detector.cpp`) already contains a
purpose-built defense for exactly this failure mode —
`bounded_by_crossing_lines`, whose own comment states it exists to
distinguish "a small enclosed gap between two pairs of crossing straight
conductors... from a genuinely drawn circular symbol." Measured directly:
**this check returns `false` (does not reject) for all 21 false
positives** — `weakest_side` (the minimum of all four sides' line-
continuation continuity) never reaches the required 0.5 threshold, values
ranging 0.00–0.44. The check's `circle_crossing_probe_distance` is a fixed
25px, but the switch-matrix candidates' own bounding boxes are only
7–22px — in a grid this tightly spaced, probing 25px beyond each side
frequently lands on a *different* part of the grid rather than a
continuation of the *same* line, so the continuity measurement reads low
and the check never fires. The check also correctly does not fire on any
of the 5 real symbols (true negative), so it is not merely "always off" —
it is specifically ineffective at the grid spacing this diagram uses.

This is a genuine, precisely-located implementation gap in an existing,
intentional anti-false-positive check (not a missing feature) — not yet
fixed, per AP-DIAG-019's scope.

## 6. Independent net-resolution-gap table

All 62 `SEPARATE_NET_RESOLUTION_GAP` endpoints own exactly 1 Wire each (no
endpoint in this population owns 0 or 2+ wires). Full detail (endpoint
kind, coordinates, wire ID, topology edges, conductor segments, current
net membership — always none) is in
`artifacts/audit/AP-DIAG-019_correlated_failure_investigation.json`'s
`net_resolution_gap_reconciliation` array. Summary:

| subset | count | wire's endpoints | tied to a fully-unresolved wire (category 6)? |
|---|---|---|---|
| geometric-kind, wire fully unresolved | 54 | both geometric | yes (27 of the 27 category-6 wires, both ends) |
| component_terminal-kind, wire resolved | 5 | one/both `component_terminal` | no |
| geometric-kind, wire partially resolved | 3 | mixed | no (1 additional wire, `wire-6352c2500ffa1edf`, in the 28-warning set but not the 27-unresolved set) |

The 4 "not tied to category 6" wires, examined individually:

- `wire-5827b8e84dd6f843` and `wire-649c42e2d82c827c`: **both** endpoints
  are `component_terminal`, and **both** belong to the *same* real,
  terminal-evidenced component (`component-candidate-shape-region-a70e3ecba418cc1a`,
  a confirmed real indicator lamp). Two separate short wires, each with
  both ends terminating at the same component's own two recognized
  terminals.
- `wire-269675807cc29475`: one geometric endpoint, one `component_terminal`
  endpoint on `component-candidate-shape-region-d2311c8e9d9e2708` (also a
  confirmed real component with terminal evidence).
- `wire-6352c2500ffa1edf`: both endpoints geometric, but not part of the
  27 fully-unresolved set (it received partial `WireSemanticResolution`
  evidence on some other field even though its endpoints stayed
  geometric-kind).

### Why NetResolver did not assign a net — traced to source

`ElectricalNetResolver::resolve` delegates net construction to
`DistributionDecomposer::decompose` (`src/topology/distribution_decomposer.cpp`).
That function walks each connected topology component and, before
constructing any `ElectricalNet`, applies an explicit, commented, and
deliberate skip:

```cpp
// Ordinary two-terminal wires do not form distribution nets merely
// because they are connected. ...
if (component_endpoints.size() < 2) {
    continue;
}
if (anchors.empty() && component_endpoints.size() == 2) {
    continue;
}
if (splice_nodes.empty() && anchors.empty()) {
    continue;
}
```

An "anchor" is a `Ground` or `ExternalConnection`-kind endpoint
(`anchor_endpoint()`, same file). **Every one of the 62 gap endpoints'
connected topology components have exactly 2 semantic endpoints and no
Ground/ExternalConnection anchor** — confirmed by the wire data itself
(each wire has exactly 2 endpoints; none of the 4 "independent" wires'
endpoints are `Ground`/`ExternalConnection` kind). This single code
condition is sufficient, on its own, to explain **100% of the 62-endpoint
population** — it applies identically whether the wire's endpoints are
unresolved `geometric` candidates or fully-recognized `component_terminal`
endpoints on a real component.

This is **by the code's own documentation, intentional, not a bug**: "a
two-terminal component is still an ordinary endpoint-to-endpoint wire even
when its geometry passes through one or more splice nodes." The system
deliberately does not manufacture an `ElectricalNet` object for a plain
two-terminal wire with no explicit ground/external anchor, to avoid
inventing unresolvable source-selection among branches. Whether this
scope should be widened (e.g., so that every resolved Wire eventually
gets *some* net identity) is a product/design question this AP does not
decide — see §11.

Stage attribution, per the AP's requested breakdown:

- **Not** endpoint classification: the 5 `component_terminal`-kind gap
  endpoints are correctly classified; classification is not the blocker.
- **Not** topology construction: the connected-component graph walk itself
  is correct; it correctly finds exactly 2 endpoints and no anchor.
- **Not** wire reconstruction: the wires exist and are geometrically
  `resolved` (`identity_status: resolved` on all 4 independent wires).
- **The blocking stage is net construction** (`DistributionDecomposer::decompose`),
  specifically its documented two-terminal/no-anchor skip.
- For the 54 endpoints tied to the 27 fully-unresolved wires, there is an
  **additional, separate, upstream** question — why those wires' endpoints
  never received *any* semantic role (component/connector/ground) in the
  first place (affecting `wire_color`/`function`/`component`/`connector`
  status, not just `electrical_net`). That upstream mechanism was not
  traced to a specific code location in this AP (it would require tracing
  `EndpointSemanticReconstructor` and its upstream terminal/connector
  association inputs) — reported as **ROOT CAUSE: UNDETERMINED** for that
  narrower sub-question, not forced.

## 7. Root-cause analysis

### 7.1 Part A — connector false positives

**COMMON ROOT CAUSE** (confidence: HIGH, direct execution, 7/7 population,
zero exceptions): `ConnectorGeometryDetector`'s notch-axis-agnostic
`opposing_notches` test. Category per the AP's taxonomy: **detector false
positive** (a geometric test that is necessary but not sufficient for
"connector body," with no downstream semantic veto — `ConnectorCandidate`
promotion in `src/pipeline/extraction_pipeline.cpp` is unconditional on
every `ConnectorGeometryDetectionArtifacts::regions` entry).

### 7.2 Part B — circular-symbol/junction-dot ambiguity

**COMMON ROOT CAUSE** (confidence: HIGH, direct execution, 21/21
population, zero exceptions): `bounded_by_crossing_lines`'s fixed probe
distance does not match this diagram's switch-matrix grid spacing, so an
existing, purpose-built anti-false-positive check never fires for this
population. Category: **detector false positive** — more precisely, an
existing false-positive *guard* that is ineffective at the grid scale
actually present in the source, compounded by `ComponentCandidateClassifier::classify`
performing an unconditional `ShapeKind::Circle` → `ComponentCandidateKind::CircularSymbol`
mapping with no secondary check.

### 7.3 Part C — independent net-resolution gap

**COMMON ROOT CAUSE for "why no net"** (confidence: HIGH, direct code
trace, 62/62 population, zero exceptions): `DistributionDecomposer::decompose`'s
explicit, documented, intentional skip of two-terminal/no-anchor connected
components. Category per the AP's taxonomy: **expected consequence of the
endpoint-to-endpoint Wire model's net-construction scope** — not a
detector false positive, not a topology interpretation error, and (for
this specific symptom) not missing semantic reconstruction. A narrower,
separate sub-question (why 54 of the 62 endpoints' wires additionally
have zero `WireSemanticResolution` evidence on every field, not just net)
is **ROOT CAUSE: UNDETERMINED** — plausibly missing semantic
reconstruction upstream, but not traced to a specific code location in
this AP.

### 7.4 Test-hygiene defect found during validation

See §2. Fixed in `tests/test_source_object_reconciliation.cpp`. Not a
detector or reconciliation-finding change.

## 8. Cross-population correlation

**Confirmed, exact-join correlations** (not estimated):

- The 27 `wire_reconciliation` (category 6, "fully unresolved wires") IDs
  are an **exact subset** of the 28 `geometric_endpoint_warning_reconciliation`
  (category 7) wire IDs (27/27 overlap; the 28th,
  `wire-6352c2500ffa1edf`, is the sole non-overlapping member). Both
  populations are largely the **same underlying wires** viewed through
  two different reconciliation lenses (WireSemanticResolution fully
  absent vs. both endpoints geometric-kind) — not two independent defect
  classes.
- 54 of the 62 `SEPARATE_NET_RESOLUTION_GAP` endpoints (category 5's
  "independent" subset) belong to those same 27 fully-unresolved wires.
  This population is therefore **not as independent from category 6/7 as
  AP-DIAG-018 characterized it** — the majority (87%) is the same
  underlying object (an unresolved wire) counted a third time from the
  net-membership angle. AP-DIAG-018's structural classification
  (`SEPARATE_NET_RESOLUTION_GAP` vs. `FOLLOWS_FROM_ZERO_WIRE`) remains
  correct as a description of *which endpoints have a wire at all*; this
  AP adds that a further correlation exists *within* the "has a wire"
  side of that split.
- The remaining 8 endpoints (4 wires) are genuinely independent of
  category 6/7 — they involve properly-resolved `component_terminal`
  endpoints — but are explained by the **exact same net-construction skip
  condition** as the other 54, just applied to already-resolved endpoints
  rather than unresolved ones. So even the "genuinely independent"
  remainder shares Part C's single root cause with the majority; there is
  no second, different net-resolution mechanism to find here.

**Tested and NOT confirmed as a common cause:** connector false positives
(Part A) and junction-dot false positives (Part B) do **share a
structural meta-pattern** — both are geometric/contour-based shape
detectors promoting to a domain object (`ConnectorCandidate`,
`ComponentCandidate`) with no downstream semantic veto for the specific
kind of incidental ink or negative space that triggers the false
positive — but they are **not the same code path or the same detector**,
and Part C is architecturally the opposite kind of issue (an existing,
deliberate *conservatism* in net construction, not an over-eager
promotion). The three populations do not share one single root cause;
they share a general engineering pattern (geometric tests without
semantic gates) across two of the three, plus one intentional design
scope limit in the third.

**Geometric endpoint warnings (28) correlation**: already covered above —
27 of 28 are the same wires as category 6. The 28th
(`wire-6352c2500ffa1edf`) is also one of Part C's "independent" 4 wires.
So all three of categories 6, 7, and the "independent" slice of category
5 trace back to a tight cluster of the same ~31 wires, analyzed from three
different angles, not ~117 independent defects.

## 9. Evidence strength for each conclusion

| conclusion | evidence type | strength |
|---|---|---|
| Connector notch-axis discriminator (Part A) | direct execution, full population (7/7) | HIGH |
| Junction-dot `bounded_by_crossing_lines` gap (Part B) | direct execution, full population (21/21 + 5/5 contrast) | HIGH |
| Net-construction two-terminal/no-anchor skip (Part C) | direct source trace, full population (62/62) | HIGH |
| 27-wire ⊂ 28-wire exact overlap | exact ID join against same-run audit | HIGH |
| 54-of-62 tied to the 27 fully-unresolved wires | exact ID join against same-run audit | HIGH |
| Upstream cause of "zero semantic resolution on any field" for the 27/28 wires | not traced to a specific code location | UNDETERMINED |
| A/B "common meta-pattern" (geometric test, no semantic veto) | qualitative code-structure observation across 2 files | MEDIUM |

## 10. Explicit UNKNOWN / UNDETERMINED cases

- **ROOT CAUSE: UNDETERMINED** — why the 27 (of 28) geometric-endpoint-
  warning wires' endpoints never received *any* semantic role (component,
  connector, ground) at all, upstream of both the "fully unresolved wire"
  and "no net" symptoms. Tracing this fully would require examining
  `EndpointSemanticReconstructor` and its upstream terminal/connector
  association inputs, which this AP did not do.
- `connector-713147ddb5e260d6`'s visual read remains LOW confidence
  (dense switch-matrix staircase routing) even though its measured notch
  axis matches the other 4 false positives exactly.
- Whether every real Wire *should* eventually receive some electrical-net
  identity (widening `DistributionDecomposer`'s scope) versus the current
  design being intentionally correct as-is is a product decision, not
  something this AP resolves.

## 11. Recommended next AP

Do not open automatically. Three narrowly-scoped candidates, in order of
leverage (largest affected population per line of actual code change):

1. **AP-DIAG-020 (proposed): connector notch-axis discriminator.** Scope:
   require the opposing-notch test to match the axis the real TRX300
   connector family actually uses (left/right), rather than accepting
   either axis — bounded strictly to `ConnectorGeometryDetector`'s
   `opposing_notches` computation. Affects up to 5 of 7 current connector
   candidates.
2. **AP-DIAG-021 (proposed): grid-gap probe-distance correction.** Scope:
   make `bounded_by_crossing_lines`'s probe distance adaptive to local
   grid spacing (or otherwise correctly reject tightly-spaced crossing
   gaps), bounded strictly to that one function in `shape_detector.cpp`.
   Affects up to 21 of 34 current component candidates.
3. **AP-DIAG-022 (proposed, lower priority): upstream semantic-role
   tracing.** Scope: determine why the 27/28-wire cluster's endpoints
   never receive any semantic role, tracing `EndpointSemanticReconstructor`
   and its inputs. Purely diagnostic until a specific defect is found;
   do not tune thresholds speculatively.

Widening `DistributionDecomposer`'s net-construction scope (Part C) is
explicitly **not** recommended as a tuning target from this AP alone — it
is documented, intentional design, and changing it is a scope/policy
decision, not a defect fix.

## 12. Detector-change authorization decision

**NOT AUTHORIZED / NOT NEEDED in this AP.** AP-DIAG-019 found specific,
precisely-located implementation conditions for Parts A and B, and
confirmed Part C as intentional design — but per the AP's own charter,
no detector, threshold, morphology, endpoint, connector, topology, net
resolution, or component-promotion code was changed here. Any change is
deferred to the narrowly-scoped follow-up APs in §11.

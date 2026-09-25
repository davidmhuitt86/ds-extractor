# AP-DIAG-FIX-001 — Exclude Component Boundary Ink From Conductor Extraction

## 1. Original defect

AP-DIAG-AUDIT-001 confirmed that 5 of 40 reported physical Wires
(12.5%) were fabrications of a component's own body/housing outline —
not conductors — with both endpoints of each wire visually confirmed to
be two corners of the same component's own drawn boundary.

## 2. Five confirmed fabricated wires

All five were re-confirmed against a fresh `samples/trx300ODG.png`
extraction on the pre-fix baseline before any code was changed:

| Wire ID | Endpoints | Owning component | Component bounds | Source object |
|---|---|---|---|---|
| `wire-33f921558320cea8` | (229,127)–(284,166) | `...16df4d7df3b94a27` | (216,128,68,39) | CDI Unit housing, top+right edge |
| `wire-549cb3ad15309b45` | (243,167)–(215,128) | `...16df4d7df3b94a27` | (216,128,68,39) | CDI Unit housing, bottom+left edge |
| `wire-430f59213afaef3a` | (161.5,157)–(161.5,182) | `...845947b0afd7451a` | (145,158,18,24) | Indicator-lamp base rectangle, right edge |
| `wire-69a0a0492bd97a07` | (144.5,182)–(144.5,157) | `...845947b0afd7451a` | (145,158,18,24) | Indicator-lamp base rectangle, left edge |
| `wire-b945dc3bfb9c54d2` | (869,544)–(922,545) | `...0771bb11fe6c347d` | (870,517,52,29) | Battery (12V12AH) case, bottom edge (3-segment path also covering top+left+right) |

In every case both endpoints of the wire shared the same
`component_id`, both were classified `component_terminal` with
`confidence: high`, and the wire's own `conductor_segments` traced that
same component's own bounding-box perimeter to within 1–2 pixels — the
gap between the shape detector's own measured bounds and where the
drawn stroke actually sits.

## 3. Root cause

The pipeline already contains a purpose-built interception point for
exactly this class of problem: `GeometryOwnershipClassifier`
(AP-GEOMETRY-006/007, `src/image/geometry_ownership_classifier.cpp`),
which decides whether line-like geometry detected by
`MorphologyWireDetector` is "owned" by a component/connector/text region
(and therefore not a conductor) before it reaches
`ConductorNormalizer`/`TopologyReconstructor`.

Tracing the real pipeline against the real image (a standalone harness
built from the production `.cpp` files, not test fixtures) showed why
this classifier let the housing outlines through:

- `both_endpoints_strictly_inside()` requires both ends of a candidate
  segment to sit **inside** a component's bounds, deliberately excluding
  boundary contact — by design, "a conductor that terminates at an
  object's boundary is exactly the interaction we need to preserve." A
  housing's own outline runs **along** the boundary, not inside it, so
  it never satisfies this test.
- `overlap_fraction()` (used to pick which component "owns" a segment at
  all) is computed via `segment_inside_length()`, which clips the
  segment against the box's interior. A housing outline stroke commonly
  sits 1px outside the shape detector's own measured `bounds` (confirmed
  empirically: the CDI Unit's top edge is drawn at y=127, the shape
  detector measured the box top at y=128), so the outline segment often
  has **zero** overlap with its own owning box under this metric and is
  never even considered for exclusion.
- `ShapeDetector`'s own exclusion mask (`shape_detector.cpp`, built via
  `cv::rectangle(mask, bounds, 255, FILLED)` for `Enclosure`/`Exclusion`-
  role shapes) fills only `[x, x+width) × [y, y+height)` — the half-open
  interval leaves the box's own far edge (column `x+width`, row
  `y+height`) unfilled, compounding the same 1px gap from the other
  direction. Confirmed directly: `exclusion_mask` at the CDI Unit box's
  measured top-left corner (216,128) is `255` (excluded), but at its
  measured right edge (284, mid-height) it is `0` (not excluded).
- Smaller shapes (the indicator-lamp base, `18×24px`) are classified
  `ShapeRole::Primitive` rather than `Enclosure` (their width falls below
  `rectangle_min_exclusion_width = 25`), and Primitive-role shapes are
  **deliberately** excluded from the exclusion mask entirely — "Primitive
  symbols remain visible in SHAPES but do not erase the conductor field"
  — because a real conductor lead commonly terminates exactly at a small
  primitive's own boundary, and masking the whole region would destroy
  that evidence. This is the same policy that made the CDI Unit case
  possible for a different reason, correctly applied for its own stated
  purpose, but with no narrower signal available to catch just the
  primitive's own outline stroke.

In short: every existing "is this owned by the component" test in the
pipeline is anchored on containment (**inside** the box) or on masking
the box's **area**, and a housing/primitive's own outline is neither —
it is coincident with the box's **perimeter**, a geometric relationship
none of the existing checks tested for.

## 4. Earliest defensible fix stage

`GeometryOwnershipClassifier::classify()`, immediately after its
existing `component_owned`/`text_owned` checks and before a candidate is
accepted into `result.conductor_candidates`. This is the pipeline's own
stated purpose for this stage (`extraction_pipeline.cpp`:
"AP-GEOMETRY-006: determine whether line-like geometry is actually owned
by a graphical object before it can enter conductor topology"), it runs
before `ConductorNormalizer`/`TopologyReconstructor`/
`PhysicalWireIdentityReconstructor` ever see the geometry, and it already
has direct access to each component's authoritative `bounds` — no new
data flow or circular dependency was needed.

## 5. Implementation

`include/eke_dx_wire/image/geometry_ownership_classifier.hpp`: added one
config field, `component_boundary_tolerance_px = 2.0` (sized to the
observed ~1px measurement/rendering gap plus stroke width, not a generic
proximity threshold).

`src/image/geometry_ownership_classifier.cpp`: added
`segment_traces_component_boundary()` and
`component_whose_boundary_is_traced()`, and one new check in
`classify()`, applied to every candidate that survives the existing
"strictly inside" tests:

A candidate is rejected as `RejectedGeometryClass::ComponentAssociated`
(or `ConnectorAssociated`, via the existing
`rejected_class_for_component()` helper, unchanged) with reason
`graphical_object_ownership_component_boundary` when:

1. it is axis-aligned (near-horizontal or near-vertical, within the same
   tolerance — a diagonal segment is never a rectilinear housing-outline
   stroke), **and**
2. its coordinate on the perpendicular axis lands within tolerance of one
   specific edge (top, bottom, left, or right) of some component's own
   established `bounds`, **and**
3. both of its endpoints, projected onto that edge's axis, fall within
   that edge's span (with the same tolerance).

Ties (a segment that could match more than one component, e.g. two
flush-adjacent housings sharing a line) are resolved by lowest
component id, for determinism, matching the tie-break style already
used elsewhere in this file (`nearest_component`/`nearest_text`).

No other file was modified. `RejectedGeometryClass`, its switch-based
consumers (`topology_exporter.cpp`, `terminal_location_detector.cpp`,
`rejected_geometry_classifier.cpp`), `PhysicalWireIdentityReconstructor`,
`ConductorBoundaryResolver`, `ElectricalNetResolver`,
`DistributionDecomposer`, and the Wire model were not touched.

## 6. Why the exclusion is evidence-based

The test never uses proximity alone, color, thickness, or "looks like a
rectangle." It uses exactly one piece of pre-existing, authoritative
evidence — a `ComponentCandidate`'s own `bounds`, already established by
`ShapeDetector`/`ComponentCandidateClassifier` earlier in the same
pipeline run — and asks a narrow geometric question: does this segment
run along that specific, already-established boundary line, for its
whole length, within a tolerance sized to the measured rendering gap?
It is not "everything inside a component's box is not a wire" (interior
geometry is untouched — see §7), not "all lines touching a component are
component geometry" (a perpendicular lead only touches the boundary at
one point and is never flagged — see Test B/C below), and not a shape
classifier of any kind (it takes a component's classification as given
and never re-derives or second-guesses it).

## 7. Why legitimate conductors remain detectable

- A lead entering or exiting a component perpendicular to its boundary
  (the task's own R1 example) has one endpoint on the boundary line and
  the other far off-axis; it is never near-horizontal *and* near-vertical
  in the same test, and it is never coincident with the boundary line for
  its own length, so it never matches. Confirmed by TEST B/C below.
- A conductor crossing a component's box entirely, or entering and
  terminating strictly inside it, is unaffected — this is exactly the
  pre-existing `component-crossing`/`component-entering` behavior in
  `test_geometry_ownership_classifier.cpp`, unchanged by this fix (still
  asserted, still passing).
- A real conductor running close to, but not coincident with, a
  component's edge (beyond the 2px tolerance) remains detected — TEST E
  below uses a 6px offset specifically to demonstrate this.
- Two adjacent components' own boundaries are independently, correctly
  excluded without affecting a real conductor bridging the gap between
  them — TEST D below.
- The TRX300 delta audit (§10) independently confirms this empirically:
  all 35 remaining wires are byte-identical to their pre-fix records; the
  only wires that disappeared are the 5 audited fabrications.

## 8. Regression tests

Added to `tests/test_geometry_ownership_classifier.cpp` (existing file,
existing CTest target `dx-wire-test-geometry-ownership-classifier`; the
pre-existing 8-candidate scenario and its assertions are untouched):

- **TEST A** — all four sides of a housing's own outline are rejected as
  `ComponentAssociated` / `graphical_object_ownership_component_boundary`;
  zero become conductor candidates.
- **TEST B/C** — a lead entering from outside and ending at the top edge,
  and a lead starting at the bottom edge and exiting outward, both remain
  conductor candidates; nothing is rejected.
- **TEST D** — two flush-adjacent housings' own boundary segments are
  each independently rejected (attributed to `housing-a`/`housing-b`),
  while a real conductor bridging the gap between them remains a
  conductor candidate.
- **TEST E** — a real wire 6px from a housing's edge (outside the 2px
  tolerance) remains a conductor candidate; nothing is rejected.
- **TEST F (TRX300 regression)** — satisfied via the standard AP
  validation methodology (§9–§10) rather than a new CTest target: no
  existing test in this suite runs the full `ExtractionPipeline` against
  a real bundled image (the one test that touches
  `samples/trx300ODG.png`, `test_recognition_input_exporter.cpp`, only
  exercises image loading and file packaging, not pipeline reconstruction),
  and adding that category of test was judged out of scope for a minimal,
  focused diff. The five audited wires' disappearance and the absence of
  any other wire-level change was instead verified by an object-level
  diff of two full, real, fresh extractions (pre-fix vs. post-fix) — see
  §9.

## 9. Before/after metrics

Clean Release build (`rm -rf build` and reconfigure), 53/53 CTest
passing both before and after, assertions confirmed active via
`compile_commands.json` (`-DNDEBUG` only for `dx-extract`;
`-DNDEBUG -UNDEBUG` for all 53 test targets, unchanged from
AP-TEST-FIX-001).

Fresh `samples/trx300ODG.png` extraction, same invocation as every prior
AP measurement:

| Metric | Pre-fix | Post-fix | Delta |
|---|---|---|---|
| Conductor segments (normalized) | 294 | 285 | −9 |
| Topology nodes | 692 | 678 | −14 |
| — conductor_end | 210 | 200 | −10 |
| — continuation | 139 | 135 | −4 |
| — splice | 106 | 106 | 0 |
| — crossing | 237 | 237 | 0 |
| — junction | 0 | 0 | 0 |
| Topology edges | 877 | 868 | −9 |
| Endpoint candidates | 210 | 200 | −10 |
| — geometric | 172 | 172 | 0 |
| — component_terminal | 26 | 16 | −10 |
| — ground | 12 | 12 | 0 |
| — connector_terminal | 0 | 0 | 0 |
| Physical wires | 40 | **35** | −5 |
| — resolved | 40 | 35 | −5 |
| — conflicted | 0 | 0 | 0 |
| — unresolved | 0 | 0 | 0 |
| Electrical nets | 10 | 10 | 0 |
| — ground role | 4 | 4 | 0 |
| — unresolved role | 6 | 6 | 0 |
| Validation errors | 0 | 0 | 0 |
| Validation warnings | 30 | 30 | 0 |
| — NET-ROLE-UNRESOLVED | 6 | 6 | 0 |
| — WIRE-GEOMETRIC-ENDPOINTS | 24 | 24 | 0 |
| Component candidates / shape_kinds | 85 (unchanged breakdown) | 85 (unchanged breakdown) | 0 |
| `symbol_primitives` | 39 | 40 | +1 (see §10) |

Every reduction is exactly and only attributable to removing 5 wires'
worth of geometry: 10 fewer endpoints (2 per wire), 10 fewer
`component_terminal`-kind endpoints (exactly matching, since all 10
removed endpoints were that kind), 9 fewer conductor segments and edges
(the CDI Unit path used 2 segments per wire × 2 wires = 4, the lamp base
used 1 segment per wire × 2 wires = 2, the battery used 3 segments for
its 1 wire = 3; 4+2+3=9). `geometric`-kind and `ground`-kind endpoints,
splice/crossing/junction node counts, all 10 electrical nets, and both
warning codes' counts are completely unaffected — nothing here was
collaterally disturbed.

## 10. Changed topology/net effects

**Wire-level delta** (object-by-object comparison, not just counts):

- **REMOVED**: exactly the 5 audited wires listed in §2 — confirmed by
  set difference over wire ids between the pre-fix and post-fix
  `topology.json`.
- **ADDED**: 0.
- **MODIFIED**: 0 — all 35 wires present in both extractions are
  byte-identical records (same id, endpoints, `identity_status`,
  `identity_evidence_ids`, `conductor_segments`, `topology_edges`).

**Electrical Net delta**: 0 nets added, removed, or modified — all 10
net records are byte-identical before and after. None of the 5 removed
wires' endpoints were ever electrical-net members, so removing them had
zero net-level effect. (The one CRITICAL AP-DIAG-005 finding — a
Ground-role net anchored on a misclassified diode symbol — is a
ChassisGround-classification issue, explicitly out of this AP's scope,
and is confirmed unchanged: same net id, same anchor, same role, same
confidence, before and after.)

**One incidental, non-wire, non-net side effect**: `symbol_primitives`
gained one new entry, `symbol-primitive-c9bca4105e248446`
(`component-candidate-shape-region-c98f6566b0672535`, kind
`terminal_lead`, 3×8px, confidence `low`). This component is a third
indicator-lamp base adjacent to the ones in §2, whose own internal-
primitive scan (`SymbolGeometryExtractor`, not modified by this AP)
receives `normalized_segments` as an input to know what ink is already
explained as a conductor; with 9 fewer conductor segments now present,
a few residual pixels near this component's own edge are attributed to
it as low-confidence internal content instead. No Wire, topology node/
edge, or electrical net is affected by this; it is confined entirely to
`SymbolGeometryExtractor`'s own, pre-existing, unmodified output for one
component.

## 11. Remaining limitations

- The 2px tolerance is a fixed constant tuned to the one measured gap
  (~1px) observed on this fixture; a diagram rendered at a different
  scale or with much thicker strokes could need a different value. It is
  a `GeometryOwnershipConfig` field, not hard-coded, so it can be tuned
  per-source if ever needed without another code change.
- This fix only catches boundary ink that is itself rectilinear
  (horizontal/vertical) and coincident with an axis-aligned bounding box
  edge. A component drawn with a non-rectangular or rotated outline would
  not be caught by this mechanism.
- AP-DIAG-AUDIT-001's SUSPECT-003 (30 wires not individually
  raster-verified) is narrowed but not eliminated by this fix's own
  narrow structural signal (both endpoints on the same component's
  perimeter) — a spurious wire touching only *one* component corner plus
  one otherwise-unremarkable point would not have been caught by either
  the audit's screen or this fix.
- The AP-DIAG-004/005 ChassisGround misclassification (a diode symbol
  feeding a false Ground-role net) is untouched, as required by this
  AP's scope, and remains open for its own follow-on AP.
- No Connector taxonomy, OCR/vision recognition, or gap-tuning change was
  made, per scope.

## 12. Follow-on APs

- **AP-DIAG-FIX-002** (proposed, not started): the AP-DIAG-004/005
  ChassisGround misclassification (diode/text/small-connector-housing
  false positives feeding a false Ground-role electrical net).
- **AP-DIAG-FIX-003** (proposed, not started): the AP-DIAG-002 connector-
  taxonomy gap (`ComponentCandidateKind` has no `Connector` value).
- **AP-DIAG-FIX-004** (proposed, not started): whether the established
  baseline invocation should include `--recognition`/`--vision-
  recognition` (AP-DIAG-006/007), a deliberate invocation decision, not a
  code defect.
- A follow-on completion of AP-DIAG-AUDIT-001's SUSPECT-001/002/003/005
  items, ideally with automated (not manual-crop) tooling, now that this
  fix has removed the specific pattern SUSPECT-003 was most worried
  about among the 5 originally-flagged wires.

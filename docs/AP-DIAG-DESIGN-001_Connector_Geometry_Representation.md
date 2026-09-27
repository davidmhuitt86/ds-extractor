# AP-DIAG-DESIGN-001: Connector Geometry & Representation Architecture

**Status:** DESIGN ONLY. No production code, model, or CMake change is authorized or made by this AP.

## 1. Executive Summary

AP-DIAG-AUDIT-010/011/012 established, with direct forensic evidence, that
the sample diagram (`samples/trx300ODG.png`) contains 12 real connector
symbols (33 visible pins, 33 conductor entries, 33 conductor exits, 0
ambiguous), that the existing `ShapeDetector` (exactly three kinds:
`Rectangle`, `Circle`, `ChassisGround`) structurally cannot classify 11 of
them, and that the one connector that does reach a shape
(`CONN-001` → `ShapeKind::Circle`) is architecturally blocked from ever
becoming a `ConnectorCandidate`/`ConnectorTerminal`, because
`TerminalCandidateKind::ConnectorBoundary` is only assigned when the
owning `ComponentCandidateKind` is `PrimitiveSymbol`, itself only
reachable from `ShapeKind::Rectangle` with a non-`Enclosure` `ShapeRole`.

This design proposes the smallest architecture that closes that gap
**without** forcing connector geometry through `Rectangle`/`Circle`/
`Enclosure` semantics it does not fit, and without disturbing any of the
existing, already-correct Wire/Net/Ground/Component invariants this
engagement has spent nine prior APs establishing. The core proposal:

1. One new `ShapeKind::ConnectorBody` value (geometry-only, no electrical
   claim) produced by a new, narrowly-scoped detection stage that sits
   **after** `ShapeDetector`, not inside it — a hybrid geometry+semantic
   stage (Design Question 3, Option D), because the forensic evidence
   shows connector recognition needs contour evidence (vertex-count
   irregularity, notch/interlock shape) **and** conductor-interaction
   evidence (pass-through wires, pin proximity) that neither `ShapeDetector`
   alone nor a purely-semantic post-processor alone can evaluate.
2. Reuse of the **existing** `ConnectorCandidate` and `ConnectorTerminal`
   structs (already defined in `model.hpp`, already wired into
   `ConnectorTerminalModelBuilder`, `ConductorBoundaryResolver`,
   `WireSemanticResolver`, and `ElectricalComponentResolver`'s
   `ConnectorInterface` rejection path) — these types are currently
   dormant (0 instances in production) but architecturally complete for
   this purpose. No new top-level candidate type is introduced.
3. One new small model addition: a `ConnectorPin` record type
   (REQUIRED — justified in §17) to represent a visible pin location
   *before* it has earned Wire-endpoint status, closing the gap between
   "a connector body was recognized" and "a `TerminalCandidate` of kind
   `ConnectorBoundary` exists" — today there is no model object between
   those two states, and pins must be representable as Unresolved without
   fabricating an endpoint.
4. Zero changes to `ShapeDetector`'s three existing detectors, zero
   changes to ground-detector thresholds (the evidence already shows 0
   false positives; nothing to fix), zero changes to `Wire`'s
   endpoint-to-endpoint definition, and zero changes to
   `ElectricalNetResolver`'s semantics.

## 2. Source-Grounded Requirements

From AP-DIAG-AUDIT-010/011/012, treated as fixed constraints on this
design, not re-derived here:

- 12 connector locations, 33 visible pins, 33 conductor entries, 33
  conductor exits, 0 ambiguous pins. (The invalid "8 connectors / 46
  terminals" figures are not used anywhere in this design.)
- 1 partial representation (CONN-001, via the ordinary `ComponentBoundary`
  path, not connector-specific), 11 absent, 0 complete.
- Two confirmed geometry sub-variants: simple single-notch (10 instances)
  and interlocking multi-step staircase (2 instances, CONN-007/008), plus
  oval/elongated variants (CONN-009/010) and housing-adjacent variants
  (CONN-003/005/009 where a separate housing box sits near the notch).
- Pin counts vary 2–4 per connector — any pin/terminal model must support
  variable arity.
- Interlocking notches are encoded as **excess `approxPolyDP` vertices
  (5–7) on a single contour**, never as a separate or nested contour.
  Contour hierarchy (`RETR_TREE` parent/child) is confirmed **not** a
  usable primary grouping mechanism — all 12 connectors' fragments are
  siblings under one universal root contour.
- CONN-011 has a genuine child contour (interior void) at exactly the
  location responsible for its `internal_line_density` near-miss
  (0.0809 vs 0.08) and its `edge_support` near-miss (0.417 vs 0.65) —
  this cannot be assumed to be a crossing wire.
- 6 of 12 connectors produce ≥3-bar ground-like patterns; all 9 such runs
  are already correctly rejected by the existing `ground_bar_width_sequence`
  / spacing checks; 0 false `ChassisGround` candidates exist today.
- `ConnectorCandidate` and `ConnectorTerminal` already exist as structs in
  `model.hpp` (§4 below) and are already consumed by three downstream
  resolvers; they are simply never populated today because nothing
  upstream ever emits a `TerminalCandidateKind::ConnectorBoundary`
  candidate for a `PrimitiveSymbol`-kind component that doesn't exist for
  any of the 12 locations.

## 3. Connector Object Semantics

Answering Design Question 1. Eight distinct concepts, deliberately kept
separate (none collapsed):

| Concept | Model status today | Proposed status |
|---|---|---|
| **Physical connector body** | No representation | New: a `ShapeRegion` of `ShapeKind::ConnectorBody` (geometry only — no pin count, no identity, no electrical claim) |
| **Connector boundary** (the body's recognized, classified footprint) | `ConnectorCandidate` struct exists, unused | Reused: one `ConnectorCandidate` per recognized connector body |
| **Connector pin** (a visible pin location on the body, before any wire evidence) | No representation | New: `ConnectorPin` (§17) — geometric location + ordinal, no endpoint reference yet |
| **Connector terminal** (a pin with resolved wire/label evidence) | `ConnectorTerminal` struct exists, unused | Reused, unchanged |
| **Conductor entry** (a wire visually crossing into the connector boundary) | No representation | New, but only as a field on evidence records (§10) — never a standalone model object |
| **Conductor exit** (symmetric to entry) | No representation | Same as entry |
| **Physical Wire endpoint** | `EndpointCandidate` of `EndpointKind::ConnectorTerminal`, referenced by `Wire.start_endpoint`/`end_endpoint` | Unchanged. A pin becomes eligible for this status only after passing through the full existing chain (§9) |
| **Electrical Net membership** | `ElectricalNet.endpoint_ids` | Unchanged — membership is decided entirely by existing net-resolution logic once an endpoint exists; connector recognition does not touch net resolution directly |

The critical ordering, stated once here and enforced throughout this
design: **body → boundary → pin → terminal → (conditionally) Wire
endpoint → (conditionally) net member.** Each arrow requires independent
evidence; none is automatic.

## 4. Geometry Model

Answering Design Question 2. **Recommendation: boundary + conductor-context
evidence**, not primitive geometry, not pure contour-derived geometry, and
not a composite-of-primitives geometry. Reasoning, directly from the
forensic record:

- **Not primitive geometry** (a single closed shape akin to `Rectangle`/
  `Circle`): rejected because the forensic evidence shows no single
  primitive-shape test (vertex count, circularity, aspect ratio) is
  satisfied by more than 1 of 12 connectors, and forcing a new primitive
  category to match "5–7 vertices" or "circularity between X and Y" would
  just be curve-fitting thresholds to this one diagram's 12 instances —
  exactly what AP-DIAG-AUDIT-012 warned against ("do not create one
  geometry class per connector").
- **Not pure contour-derived geometry** (classify from `approxPolyDP`
  output alone): rejected because AP-DIAG-AUDIT-012 §9/§11 showed vertex
  count alone cannot distinguish "genuine interlocking connector notch"
  from "noisy/irregular non-connector blob" — by the time
  `polygon.size() != 4` is evaluated, the vertex arrangement itself
  (regular staircase vs. random noise) is already lost. Contour shape
  alone is necessary evidence but not sufficient evidence.
- **Not composite geometry** (a fixed assembly of named sub-parts, e.g.
  "housing + 3 pin-tabs"): rejected because pin count varies 2–4 and the
  two confirmed sub-variants (simple notch vs. interlocking staircase)
  have structurally different pin-tab geometry; a fixed composite schema
  would need a variant enum per sub-variant, reintroducing the
  per-connector proliferation this AP must avoid.
- **Chosen: boundary + conductor-context evidence.** A `ConnectorBody`
  geometry record carries (a) the recognized boundary polygon/contour
  (whatever `approxPolyDP` vertex count it actually has — 4, 5, 6, or 7,
  unconstrained), (b) a small, closed set of **geometric evidence
  fields** (vertex count, convexity, aspect ratio, fill ratio — the same
  measurements `ShapeDetector` already computes for `Rectangle`/`Circle`,
  reused rather than reinvented), and (c) a small set of
  **conductor-context evidence fields**: how many distinct conductor
  segments cross the boundary polygon's perimeter, and at how many
  distinct crossing points. This directly matches the two-family
  breakdown AP-DIAG-AUDIT-012 confirmed (interlocking-vertex-excess
  families vs. oval/elongated-aspect families vs. housing-adjacent
  fragmented families) without hardcoding any of them: a connector
  candidate is one whose evidence combination is *consistent with*
  "notched pass-through body," evaluated as a scored evidence set, not as
  a single pass/fail geometric test.

This directly satisfies Design Gate A: one generalized evidence-based
model, not 12 per-connector rules, and it satisfies AP-DIAG-AUDIT-012 §19
architectural constraint (i): a future detector "must operate on vertex
count/shape irregularity directly" rather than forcing a fixed vertex
count.

## 5. Detection Architecture

Answering Design Question 3.

| Option | Verdict | Why |
|---|---|---|
| A. Inside `ShapeDetector` | **Rejected** | `ShapeDetector`'s three detectors are pure, single-pass, single-image-channel geometric tests with no access to conductor-segment data (conductor extraction is a separate, later pipeline stage — `GeometryOwnershipClassifier` operates on `ConductorSegment`s that don't exist yet when `ShapeDetector::detect()` runs). Connector recognition needs conductor-crossing evidence (§4), which is structurally unavailable at `ShapeDetector`'s point in the pipeline. Adding it here would require either reordering the whole pipeline (out of scope, high blast radius) or evaluating conductor-crossing on incomplete data (unsound). |
| B. Dedicated `ConnectorGeometryDetector` (image-stage, standalone) | **Partially adopted** | Correct for the *geometry-only* half of the evidence (boundary polygon, vertex count, aspect ratio) — these are legitimately image-stage concerns, identical in kind to what `ShapeDetector` already does. But a detector that only sees the image cannot evaluate conductor-crossing evidence, so on its own this option is incomplete. |
| C. Post-`ShapeDetector` semantic recognizer | **Partially adopted** | Correct for the *conductor-context* half (crossing count, crossing points) and for combining that with `ShapeRegion` output, once conductor segments exist. But a purely semantic stage operating only on already-finalized shapes cannot re-examine raw contour geometry that `ShapeDetector` discarded (e.g. a rejected `Rectangle` candidate's raw vertex list is not persisted anywhere once rejected). |
| D. **Hybrid geometry + semantic stage** | **Recommended** | Combines B and C as two cooperating passes rather than one option chosen over the other: (1) a small, new, standalone image-stage component — not added inside `ShapeDetector`, not touching its thresholds or detectors — that runs the *same* `findContours`/`approxPolyDP` machinery already proven in AP-DIAG-AUDIT-012 to characterize connector contours, and emits `ShapeKind::ConnectorBody` regions directly into `ShapeDetectionArtifacts.regions` (same output type `ShapeDetector` already produces, so every downstream consumer of `ShapeRegion` needs no new code path); (2) a second, later semantic stage — positioned exactly where `TerminalLocationDetector`/`TerminalRecognizer` already run, consuming `ConductorSegment`s (which exist by then) — that evaluates conductor-crossing evidence against `ConnectorBody` regions and decides pin/terminal candidacy. |

**Recommendation: Option D**, implemented as two new, narrow components:
a `ConnectorGeometryDetector` (image stage, geometry-only, emits
`ShapeRegion`s of kind `ConnectorBody`) and connector-aware extensions to
the existing terminal-recognition stage (semantic stage, conductor-aware,
emits `ConnectorPin`/`TerminalCandidate` records) — never a single
monolithic detector, and never inside `ShapeDetector` itself. This is
justified strictly by the forensic evidence that connector recognition
needs both image-only and conductor-aware evidence, which live at
different points in the existing pipeline's data availability.

## 6. Candidate Representation

Answering Design Question 4. **`ConnectorCandidate` (existing struct) is
reused as-is; no new top-level candidate type is introduced.**

Evaluated against the existing architecture:

- `ComponentCandidateKind` — evaluated and rejected as the vehicle for
  connectors. Every existing value (`Enclosure`, `CircularSymbol`,
  `ChassisGround`, `PrimitiveSymbol`, `DiagramFurniture`, `Unknown`)
  already carries specific downstream semantics (e.g.
  `ElectricalComponentResolver` treats `ChassisGround`/`DiagramFurniture`
  as automatic rejections, and `PrimitiveSymbol` is the sole gateway to
  `TerminalCandidateKind::ConnectorBoundary` today). Adding a
  `ComponentCandidateKind::Connector` value would require every existing
  consumer of that enum to add a new case, and would conflate "this is a
  component" with "this is a connector" — exactly the semantic distortion
  this AP must avoid. **Not used.**
- `PrimitiveSymbol` (the `ComponentCandidateKind` value) — already proven
  architecturally incompatible (AP-DIAG-AUDIT-010/011): it is only
  reachable from `ShapeKind::Rectangle` with non-`Enclosure` role, which
  11 of 12 connectors never produce and which, even for CONN-001, is the
  wrong shape family (`Circle`, not `Rectangle`). **Not used as the path
  to connector representation.**
- `SymbolPrimitive`/`SymbolPrimitiveKind` — this is a *different* existing
  concept (internal geometric primitives found *inside* an
  already-classified component's boundary, e.g. `TerminalLead`). It
  operates one level below `ComponentCandidate`, not as a peer to it, and
  is explicitly documented to never produce `EndpointCandidate`s itself.
  Not a fit for representing the connector body itself. **Not used.**
- `ConnectorCandidate` — **already exists**, already has the right shape
  (`id`, `component_candidate_id` — see §17 for why this field name is
  kept but re-scoped, `bounds`, `confidence`, `semantic_labels`), and is
  already the type `ConnectorTerminalModelBuilder`,
  `ElectricalComponentResolver` (`ConnectorInterface` rejection reason),
  and `ConductorBoundaryResolver` all already consume. **Reused
  unmodified in field layout.**

This directly answers Design Gate G for this section: no new type is
introduced merely for conceptual cleanliness — `ConnectorCandidate`
already exists and already fits.

## 7. Shape Representation

Answering Design Question 5. **Yes — one new `ShapeKind::ConnectorBody`
value is proposed**, because the existing three values are each
demonstrably wrong for this geometry, not merely inconvenient:

- **`CircularSymbol` is not appropriate**: CONN-001's circle-path
  acceptance (circularity 0.686, aspect 1.235) is a coincidental
  geometric resemblance, not evidence of connector identity — `Circle`
  correctly means "this ink forms a circularity/aspect/edge-support
  profile consistent with a drawn round symbol," which says nothing about
  pins, pass-through conductors, or notch structure. Continuing to route
  CONN-001 through `CircularSymbol` (as it is today) leaves it
  permanently unable to reach `ConnectorBoundary`, because
  `ComponentCandidateKind::CircularSymbol` is not `PrimitiveSymbol` and
  nothing proposes changing that mapping (§17 rejects doing so).
- **`Enclosure` is not appropriate**: `Rectangle`+`Enclosure` specifically
  means "a clean, closed 4-vertex boundary with disconnected interior
  content and low internal-line density" — the opposite of what a
  notched, wire-crossed, variable-vertex connector body is. Every housing
  box adjacent to a connector (CONN-003, CONN-005, CONN-009) that *does*
  pass the `Enclosure` gauntlet is a **different feature** from the
  connector notch itself (confirmed in AP-DIAG-AUDIT-011/012); using
  `Enclosure` for the connector body would either misclassify the
  housing as the connector or require weakening `Enclosure`'s own
  semantics, both rejected.

**Semantic meaning of `ShapeKind::ConnectorBody`:** "this raster region's
boundary is geometrically consistent with a drawn connector housing —
irregular polygon (not required to be a clean quadrilateral), notch or
staircase edge structure permitted, aspect ratio unconstrained — and nothing
more." It carries **no** implication about pin count, wire identity,
electrical connectivity, or resolution status; those are established only
by the later semantic stage (§5, §12).

**Evidence requirements** (all measured the same way `ShapeDetector`
already measures its existing evidence, via the same `contourArea`/
`arcLength`/`approxPolyDP`/morphology primitives — no new image-processing
primitive is introduced):
- Contour area ≥ a minimum (reuse `rectangle_min_area`-scale reasoning,
  not a new magic number invented in this design).
- `approxPolyDP` vertex count in a **range**, not a fixed value (the
  forensic data shows real connector-adjacent candidates at 4, 5, 6, and
  7 vertices — CONN-011's own accepted-shape candidate is 4 vertices,
  CONN-001/002 are 5, CONN-007/008 are 5–7).
- At least one conductor segment (from `GeometryOwnershipClassifier`'s
  already-classified, already-owned conductor geometry) crossing the
  boundary polygon at two or more points on its perimeter (pass-through
  evidence) — this is the discriminating evidence a pure image-stage
  detector cannot supply, and is exactly why this is a hybrid stage
  (§5).

**What it must NOT imply:** `ShapeKind::ConnectorBody` must not, by
itself, cause `ShapeDetector`'s existing `exclusion_mask` behavior to
change for any other shape, must not retroactively reclassify any
existing `Rectangle`/`Circle`/`ChassisGround` region, and must not by
itself create any `TerminalCandidate`, `EndpointCandidate`, or `Wire` —
those all require the independent evidence chain in §9.

## 8. Connector Terminal Model

Answering Design Question 6. **The existing `ConnectorTerminal` struct
already has the required fields and needs no modification**:

```cpp
struct ConnectorTerminal {
    std::string id;
    std::string connector_id;
    std::string endpoint_id;
    Point2D position {};
    std::string terminal_name;
    std::string function_label;
    std::string wire_color;
    TerminalRole role = TerminalRole::ConnectorTerminal;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    ConnectorTerminalStatus status = ConnectorTerminalStatus::Unresolved;
};
```

The relationship tree required by this design question:

```
ConnectorCandidate (the body)
   |
   +-- ConnectorPin (NEW, §17) — one per visually-observed pin, geometry only
   |      |
   |      +-- (evidence accumulates: label match, wire-color continuity,
   |      |    conductor-crossing alignment)
   |      |
   |      +-- TerminalCandidate{kind=ConnectorBoundary} — existing type,
   |             emitted only when pin evidence clears the existing
   |             TerminalLocationDetector/TerminalRecognizer proximity
   |             logic, exactly as it already does for ComponentBoundary
   |             candidates today
   |             |
   |             +-- ConnectorTerminal — existing type, built by the
   |                    existing (unmodified) ConnectorTerminalModelBuilder
   |                    |
   |                    +-- (only if ConnectorTerminalStatus::Resolved)
   |                           adopted by ConductorBoundaryResolver as
   |                           connector_status = Resolved
   |                           |
   |                           +-- (only if PhysicalWireIdentityReconstructor
   |                                  independently confirms a Wire boundary
   |                                  belongs here) Wire.start_endpoint /
   |                                  end_endpoint
```

No arrow in this chain is automatic. `ConnectorPin` existing does not
imply a `TerminalCandidate` exists; a `TerminalCandidate` existing does
not imply a `ConnectorTerminal` reaches `Resolved`; a `Resolved`
`ConnectorTerminal` does not imply a `Wire` endpoint exists there (a
`Wire` endpoint requires the independent conductor-topology evidence
`PhysicalWireIdentityReconstructor` already demands of every endpoint,
connector or not).

## 9. Pin/Wire Boundary Semantics

Answering Design Question 7 — the AP explicitly marks this critical.

**A connector pin is never automatically a Wire endpoint.** The exact
state progression, each transition gated by independent evidence:

1. **Connector pin** (new `ConnectorPin`, §17): a geometric location on a
   recognized `ConnectorBody`/`ConnectorCandidate`'s boundary where visual
   evidence (a small tab, a labeled notch segment, or a conductor
   touching the boundary at that point) suggests a pin exists. This
   record can exist with **no** `EndpointCandidate` reference at all —
   it is pure geometric observation, exactly analogous to how
   `SymbolPrimitive` records exist without implying an `EndpointCandidate`
   (per the existing `TerminalLead` precedent, which explicitly documents
   "not an EndpointCandidate and must not be treated as one").
2. **Connector terminal** (existing `TerminalCandidateKind::ConnectorBoundary`
   → `ConnectorTerminal`): becomes eligible only when the *existing*
   `TerminalLocationDetector`/`TerminalRecognizer` proximity logic — run
   unmodified — independently finds an `EndpointCandidate` positioned at
   or near the `ConnectorPin`'s location. This is the same evidentiary
   bar every `ComponentBoundary` terminal already has to clear today; no
   new, weaker bar is introduced for connectors.
3. **Conductor boundary** (existing `ConductorBoundaryResolution`):
   unchanged — `ConductorBoundaryResolver` already adopts a `Resolved`
   `ConnectorTerminal`'s identity "directly, never re-decided," and
   already marks `Conflicted` if multiple connectors claim the same
   endpoint. This design adds no new logic here; it only supplies
   `ConnectorTerminal`s for the resolver to potentially find, where today
   it finds none.
4. **Wire endpoint** (existing `Wire.start_endpoint`/`end_endpoint`):
   unchanged. `PhysicalWireIdentityReconstructor` (AP-WIRE-031) decides
   Wire boundaries from topology-node degree and `ConductorSegment`
   evidence, **not** from connector recognition. A `ConnectorPin` or even
   a `Resolved` `ConnectorTerminal` existing at a location is necessary
   context for `WireSemanticResolver`'s later semantic *labeling* of an
   already-decided Wire endpoint (populating `start_connector_id`/
   `start_connector_terminal_name` — this already exists, unmodified) —
   it is never sufficient cause to *create* a Wire boundary that topology
   evidence does not independently support. This directly satisfies
   AP-WIRE-029/030/031's standing invariants (this design changes none of
   them) and the AP's explicit instruction: "do not create Wire
   boundaries merely because a connector symbol exists."

## 10. Pass-Through Conductor Semantics

Answering the AP's Part 8 (pass-through conductors). The model must
represent, without inferring electrical connectivity from geometry alone:

- **Conductor enters/exits a connector body**: represented as a field on
  the (new, §17) connector-recognition evidence record — a list of
  `ConductorSegment` ids whose geometry crosses the `ConnectorCandidate`'s
  boundary polygon, each tagged with its crossing point (`Point2D`) and
  which side of the boundary it approaches from. This is **evidence**,
  not a claim of electrical connection — it says "a conductor's ink
  crosses this boundary here," nothing more.
- **One conductor through**: the common case — one `ConductorSegment`
  enters, the same or a topologically-continued segment exits on the
  opposite/adjacent side. Represented as two independent crossing-evidence
  entries; whether they represent the *same* electrical path through the
  connector is a question for `PhysicalWireIdentityReconstructor`'s
  existing topology-continuity logic, not for connector recognition to
  assert.
- **Multiple independent conductors**: represented as multiple,
  independent crossing-evidence entries with no forced pairing between
  them. The design explicitly does **not** pair entry N with exit N by
  positional convention (e.g. "first entry pairs with first exit") —
  doing so would be inferring connectivity from geometry alone, which the
  AP explicitly forbids. Pairing, if ever asserted, must come from
  independent evidence (e.g. matching wire color continuity, matching
  `function_label` text) via the existing `WireSemanticResolver`
  machinery, not from this design's evidence records.
- **Multiple pins sharing a visible conductor section**: represented as
  multiple `ConnectorPin` records whose crossing-evidence entries
  reference the same `ConductorSegment` id — this is recorded as observed
  fact (shared segment reference), not resolved into a single merged pin;
  resolution (if any) is deferred to a future AP with its own evidence
  requirements, not decided here.
- **Ambiguous continuity**: represented by leaving the relevant
  `ConnectorPin`'s terminal-candidacy status at `Unresolved` (never
  guessing `Resolved`) — consistent with §11's evidence model and the
  AP's explicit "never guess" instruction.

## 11. Electrical Net Semantics

Answering Design Question 10. **No change to `ElectricalNetResolver` or
to `ElectricalNet`'s fields.** A connector terminal participates in an
`ElectricalNet` in exactly the same way any other endpoint does today:
by having its `endpoint_id` appear in some `ElectricalNet.endpoint_ids`
list, which is decided entirely by the existing topology/distribution
decomposition logic operating on `TopologyNode`/`TopologyEdge`/
`ConductorSegment` evidence — logic this design does not touch. This
preserves both standing invariants unmodified:

- **ONE NET != ONE WIRE**: a `Wire` remains a single endpoint-to-endpoint
  physical identity; an `ElectricalNet` remains a possibly-larger group of
  endpoints sharing distribution-level connectivity (e.g. a shared ground
  bus). Nothing in this design merges these concepts for connectors.
- **ONE WIRE != ONE NET**: unchanged; a Wire's membership in a net is
  still decided after the fact by `ElectricalNetResolver`, never asserted
  by connector recognition.

No visual adjacency to a connector body is ever, by itself, treated as
evidence for net membership — net membership evidence remains exactly
what it is today (topology/distribution structure), and connector
recognition supplies no new net-membership evidence path.

## 12. Evidence Model

Answering Design Question 11. Evidence categories evaluated against
what the forensic record shows is actually available:

| Evidence type | Available? | Used for |
|---|---|---|
| Connector geometry (boundary polygon, vertex count, aspect) | Yes — `ShapeDetector`-style contour measurement | `ConnectorBody` shape recognition (§7) |
| Contour evidence (raw `approxPolyDP` output) | Yes | Same as above |
| Conductor interaction (crossing count/points) | Yes, once `ConductorSegment`s exist | `ConnectorBody` recognition (discriminates from ordinary non-crossed rectangles) and pin candidacy (§10) |
| Notch/interlock evidence (vertex irregularity pattern) | Yes, but **not sufficient alone** (AP-DIAG-AUDIT-012 §9/§11) | Contributing evidence only, combined with conductor interaction |
| Repeated symbol convention (same footprint appears N times in one diagram) | **Not established as available** — this AP's forensic record never measured cross-instance geometric similarity | **Not required** in the evidence model; if a future AP establishes this is measurable and reliable, it may be added as an additional, optional evidence source, but this design does not assume it exists |
| Pin evidence (tab/lead geometry at boundary) | Partially — `SymbolPrimitive::TerminalLead` already exists as a concept for exactly this kind of geometry, though never yet linked to connector bodies | Contributing evidence for `ConnectorPin` candidacy |
| Label evidence (OCR text near the connector, e.g. "(B)(MINI)", "[MINI](G)") | Yes — `TextRegion`/`text_recognition_evidence` already exists in the pipeline | Contributing evidence for `ConnectorCandidate.semantic_labels` (a field the struct already has) |
| Component relationship (is this shape owned by/adjacent to a known component) | Yes — `GeometryOwnershipClassifier` precedent | Used to distinguish a connector body from an adjacent housing (the CONN-003/005/009 pattern) |
| Source-context evidence (diagram-wide conventions, e.g. two connectors always drawn as twins) | **Not established as available** — no existing pipeline stage measures this | **Not required**; same treatment as "repeated symbol convention" above |

Per the AP's own instruction ("do not require every evidence type if the
forensic evidence establishes some are unavailable"), this design
requires only: connector geometry, conductor interaction, and (as
available) label/component-relationship evidence — combined into a
resolution status:

- **Resolved**: `ConnectorBody` geometry evidence is present AND at least
  one conductor-crossing evidence entry exists AND at least one
  `EndpointCandidate` is independently found by the unmodified
  `TerminalLocationDetector`/`TerminalRecognizer` proximity logic at a
  `ConnectorPin` location.
- **Unresolved**: `ConnectorBody` geometry evidence is present but
  conductor-crossing or endpoint evidence is missing or insufficient —
  this is the expected status for most of the 11 currently-absent
  connectors even after this design is implemented, since geometry
  recognition alone does not manufacture endpoints.
- **Conflicted**: two or more `ConnectorCandidate`s claim overlapping
  geometry, or an endpoint is claimed by more than one connector's pin
  set (mirroring `ConductorBoundaryResolver`'s existing `Conflicted`
  precedent for exactly this shape of disagreement — never resolved by
  picking a winner).

## 13. False-Positive Controls

Answering Design Question 12. The design relies on the **combination** of
geometric and conductor-interaction evidence, never shape resemblance
alone, specifically to exclude each listed false-positive class:

- **Component enclosures**: excluded because a real `Enclosure` (per
  `ShapeDetector`'s existing, unmodified logic) has low
  `internal_line_density` and disconnected interior content — the
  opposite of the conductor-crossing evidence a `ConnectorBody` requires.
  A shape cannot simultaneously satisfy both `Enclosure`'s
  low-internal-line-density gate and `ConnectorBody`'s
  conductor-crossing requirement; they are evidentially exclusive by
  construction, not by an arbitrary rule added to distinguish them.
- **Switches/relays**: excluded the same way general "ordinary rectangles"
  are (below) — a switch/relay symbol without conductor pass-through
  evidence never accumulates the required evidence combination.
- **Circular symbols**: excluded because `ConnectorBody` recognition does
  not compete with `detect_circles()` — it is evaluated as a separate,
  additional geometry family (§7), and CONN-001 specifically is expected
  to eventually accumulate evidence for **both** `Circle` (already true
  today) **and** `ConnectorBody` (new); this design does not require
  choosing one over the other for a given raster region (see §16 for how
  `ComponentCandidateClassifier` must handle a region carrying evidence
  for two `ShapeKind`s — resolved there as a design change, not silently
  here).
- **Ground symbols**: no interaction required (§14/Design Question 13) —
  the existing ground detector's width-sequence/spacing checks already
  reject every connector-generated bar pattern; this design adds no
  connector-vs-ground disambiguation logic because none is needed.
- **Diagram furniture**: excluded by the same conductor-crossing
  requirement — furniture (tables, legends, grid boxes) does not have
  live conductors crossing its boundary in the same way a connector does;
  where ambiguity remains, `DiagramFurnitureClassifier`'s existing
  grid-arrangement logic (unmodified) still runs and can still re-tag a
  falsely-accepted `ConnectorBody` region's owning `ComponentCandidate`
  as `DiagramFurniture` exactly as it already does for other kinds today.
- **Text glyphs**: excluded by the existing minimum-area/vertex-count
  floor inherited from `ShapeDetector`'s established thresholds (reused,
  not reinvented) — a text glyph does not have the contour area or
  conductor-crossing evidence a connector body needs.
- **Wire crossings / wire labels**: excluded because a bare wire crossing
  (two lines meeting) does not produce a closed boundary polygon at all —
  `ConnectorBody` requires a genuine closed (or near-closed) contour, not
  merely intersecting line segments; wire labels are text, excluded as
  above.
- **Ordinary rectangles**: excluded because an ordinary rectangle with no
  conductor crossing its boundary (the vast majority of rectangles in any
  diagram) never accumulates the conductor-interaction evidence
  `ConnectorBody` requires — this is the single most important
  discriminator in the whole design, and it is exactly the evidence axis
  (conductor interaction) that a pure-geometry detector (Option B alone,
  §5) could never evaluate, which is why Option D (hybrid) was chosen.
- **Arbitrary notched geometry** (a stray notch that isn't a connector at
  all): excluded by requiring the conductor-crossing evidence in addition
  to notch/vertex-irregularity evidence — a notch shape with no
  conductor crossing it stays `Unresolved`, never promoted to `Resolved`
  connector status.

## 14. Interior-Void Handling

Answering Design Question 14 / the AP's explicit CONN-011 requirement.
The design introduces **no generic "low ink density = wire" rule** — the
AP explicitly forbids this, and AP-DIAG-AUDIT-012 already showed this
assumption is unsafe for CONN-011.

Distinguished cases, using contour hierarchy as *observational* evidence
(never as a grouping mechanism, per §4/AP-DIAG-AUDIT-012's finding that
hierarchy is not usable for connector association — it is still usable
here as a **local, single-contour** fact, which is a different claim):

- **Connector interior void**: a child contour exists (via `RETR_TREE`
  parent/child, exactly as AP-DIAG-AUDIT-012 found for CONN-011's
  `idx=698` inside `idx=697`) whose geometry is consistent with a small,
  closed, symbol-scale feature (bounded area, reasonable aspect ratio,
  fully enclosed within the parent's interior region) — recorded as
  `has_interior_void = true` on the connector-recognition evidence
  record, contributing *positively* toward `ConnectorBody` candidacy
  (an interior void is itself a plausible piece of connector-notch
  structure), not negatively as "wire interference."
  the evidence record, contributing toward candidacy.
- **Conductor absence** (no ink at all in a region): recorded separately
  as `interior_density` (the existing measurement `ShapeDetector` already
  computes) — low density alone, with **no** corresponding child contour,
  is not treated as void evidence at all; it is simply low density,
  exactly as it is for any other shape today.
- **Conductor interruption** (a wire that visually stops at the
  boundary rather than passing through): represented as a
  crossing-evidence entry with a `crosses_fully = false` flag — the
  segment touches the boundary but does not have a corresponding
  exit-side segment. This is *evidence toward* pin/terminal candidacy
  (a wire terminating at a connector is exactly the semantic pattern this
  design must eventually recognize), not evidence toward or against
  `ConnectorBody` shape recognition itself.
- **Symbol background** (ordinary non-void empty space inside a
  recognized boundary, no child contour, no crossing evidence): the
  default/neutral case — contributes nothing either way.
- **Actual wire crossing**: represented as a crossing-evidence entry
  referencing a real `ConductorSegment` id from
  `GeometryOwnershipClassifier`'s already-classified, already-owned
  conductor geometry — this is the **only** case allowed to be labeled
  "wire," and only because it is backed by an actual, independently
  classified `ConductorSegment`, never inferred from density alone. This
  directly resolves the AP-DIAG-AUDIT-011 ambiguity: CONN-011's near-miss
  ink is now classified as "child contour, no corresponding
  `ConductorSegment` reference" — i.e., interior-void evidence, not wire
  evidence — unless a future run's actual conductor-segment data says
  otherwise, in which case the crossing-evidence record (not a density
  heuristic) is what decides it.

## 15. Geometry-Family Mapping

Answering Design Question 15, using the exact families from
AP-DIAG-AUDIT-012 §9 (A–J taxonomy). All families map onto the **one**
`ConnectorBody`/evidence-record model from §4/§7 via variant *evidence
values*, not variant *detector logic* or variant *geometry classes*:

| AP-DIAG-AUDIT-012 family | Connectors | Represented via |
|---|---|---|
| A (single closed contour) | CONN-001 | High vertex-regularity evidence, may also carry `Circle` shape evidence concurrently (§13) |
| B (outer+inner contour) | CONN-011 | `has_interior_void = true` (§14) |
| D (multiple sibling contours) | all 12 (universal, non-discriminating) | Not itself evidence for or against `ConnectorBody` — recorded as a structural fact only, never scored |
| E (open contour fragments) | CONN-004, CONN-006, CONN-012 | Insufficient contour-area/vertex evidence — these remain `Unresolved` under this design too; the design does not claim to recover connectors with no coherent boundary ink at all, and §21 states this explicitly as a non-goal |
| F (conductor-interrupted) | CONN-011 (confirmed), possibly others (unconfirmed, §17 AP-DIAG-AUDIT-012 limitation) | Crossing-evidence entries with `crosses_fully=false`/`has_interior_void=true` combination (§14) |
| G (notch as indentation in one contour) | CONN-007, CONN-008 | Vertex-count-range evidence (§7), no separate contour needed |
| I (interlocking/staircase) | CONN-007, CONN-008 | Same as G — this design treats G and I as the same underlying evidence shape (vertex-count irregularity on one contour), consistent with AP-DIAG-AUDIT-012's own finding that these overlap for CONN-007/008 |
| J (mixed) | CONN-003 | Represented by two independent, non-conflicting records: the housing continues to be classified as `Enclosure`/`ComponentCandidate` exactly as today (unchanged), while the notch region is separately evaluated for `ConnectorBody` evidence — the design does not attempt to merge these into one object |

No detector-per-connector and no geometry-class-per-connector is
introduced anywhere in this mapping — every family reduces to a
combination of the same handful of evidence fields (vertex count range,
aspect ratio, interior-void flag, conductor-crossing count/points),
directly satisfying Design Gate A.

## 16. Existing Architecture Integration

Answering Design Question 16.

| Component | Status | Why |
|---|---|---|
| `ShapeDetector` | **Unchanged** | Its three existing detectors, thresholds, and `ShapeKind`/`ShapeRole` handling are untouched. `ShapeKind::ConnectorBody` is a new enum value it does not itself produce. |
| `ConnectorGeometryDetector` (new) | **New (extended architecture)** | A new, standalone image-stage component (§5), consuming the same normalized/binary raster `ShapeDetector` uses, emitting `ShapeRegion`s of the new `ShapeKind::ConnectorBody` into the same `ShapeDetectionArtifacts` type — extends the set of shape producers, does not modify the existing one. |
| `GeometryOwnershipClassifier` | **Unchanged** | Continues to classify conductor-segment ownership exactly as today; its output (`ConductorSegment`s) is *consumed* by the new connector semantic stage (§5) as an input, but this classifier's own logic is not modified. |
| `SymbolGeometryExtractor` | **Unchanged** | Continues to extract internal primitives (including `TerminalLead`) for non-`DiagramFurniture` components exactly as today. A future implementation AP may choose to also run it against `ConnectorBody`-kind regions (since `TerminalLead` geometry is exactly the kind of pin evidence §12 wants), but that is an **extension** of its inputs, not a modification of its logic — left to the implementation AP to decide, not decided here. |
| `ComponentCandidate` / `ComponentCandidateClassifier` | **Extended** | `classify_kind()`'s switch statement needs one new case: a `ShapeKind::ConnectorBody` region must not silently fall to `default: Unknown`. The smallest correct extension is a new `ComponentCandidateKind::Unknown`-sibling path that does **not** reuse `PrimitiveSymbol`/`Enclosure`/`CircularSymbol` (§6) — see §17 for whether this requires a new `ComponentCandidateKind` value (REQUIRED, justified there) or whether `ConnectorBody` regions bypass `ComponentCandidate` entirely (the design's actual recommendation, §17). |
| `TerminalCandidate` / `TerminalLocationDetector` / `TerminalRecognizer` | **Extended** | Both already assign `TerminalCandidateKind::ConnectorBoundary` in their existing enum/logic; today nothing upstream ever gives them a `PrimitiveSymbol`-kind connector component to attach it to. The extension is: allow these two stages' existing proximity logic to also consider `ConnectorPin` locations (new, §17) as candidate endpoint-attachment points, using the **same** distance-threshold logic already used for `ComponentBoundary` — no new proximity algorithm. |
| `ConnectorBoundary` (concept) | **Unchanged** | Remains what it already is: the `TerminalCandidateKind::ConnectorBoundary` enum tag. No new struct is introduced for it (§6 confirms `ConnectorCandidate` already serves the "connector body" role this enum tag's owner needs). |
| `ConnectorTerminalModelBuilder` | **Unchanged** | Its existing `build()` logic (gate on `kind == ConnectorBoundary && component_candidate_id non-empty`, look up owning candidate, look up endpoint) is reused verbatim. It will simply start receiving non-empty input for the first time once the upstream stages above are extended. |
| `ConductorBoundaryResolver` | **Unchanged** | Its existing "adopt a Resolved ConnectorTerminal directly, never re-decide, Conflicted on multi-claim" logic already does exactly what this design needs; it requires no new code to handle real connector data once it exists. |
| `PhysicalWireIdentityReconstructor` | **Unchanged** | Wire-boundary decisions remain purely topology/`ConductorSegment`-evidence-based, per §9. |
| `ElectricalNetResolver` | **Unchanged** | Per §11. |
| `WireSemanticResolver` | **Unchanged** | Its existing `find_resolved_connector_terminal()` logic already does exactly what this design needs once real, `Resolved` `ConnectorTerminal`s exist to find. |
| `ElectricalComponentResolver` | **Unchanged** | Its existing `ConnectorInterface` rejection reason already fires correctly once `ConnectorCandidate.component_candidate_id` references are populated for the first time — no new logic required, it was already built for this. |
| `extraction_pipeline` | **Extended** | The new `ConnectorGeometryDetector` (image stage) is inserted alongside `ShapeDetector`'s existing call site; the new pin/terminal evidence logic is inserted alongside the existing `TerminalLocationDetector`/`TerminalRecognizer` call sites. No stage is reordered, removed, or bypassed. |

No component in this list is marked **Replaced** or **Bypassed** — every
integration is additive (new components alongside existing ones) or a
minimal, additive extension of an existing component's input surface.

## 17. Model Change Gate

Every proposed change, classified per the AP's explicit gate.

| Change | Classification | Justification |
|---|---|---|
| New `ShapeKind::ConnectorBody` enum value | **REQUIRED** | Existing limitation: `ShapeKind` has exactly `{Rectangle, Circle, ChassisGround}` (confirmed, `shape_detector.hpp`); none can represent a notched, variable-vertex, conductor-crossed boundary without semantic distortion (§7). Smallest possible change: one new enum value, no field changes to `ShapeRegion` itself (it already has `id`, `kind`, `role`, `bounds`, `confidence` — sufficient). |
| New `ConnectorGeometryDetector` component | **REQUIRED** | Existing limitation: no existing component evaluates connector geometry at all (§5); this is not a modification of an existing limitation, it is the absence of a component. |
| New `ConnectorPin` model type | **REQUIRED** | Existing limitation: there is no model object between "a connector body was recognized" and "a `TerminalCandidate` exists." Every existing candidate type (`ComponentCandidate`, `ConnectorCandidate`, `TerminalCandidate`, `EndpointCandidate`) either represents a whole body or an already-endpoint-attached terminal — none represents "a visually observed pin location, not yet evidenced as a terminal." Without this type, the design would be forced to either (a) fabricate an `EndpointCandidate` prematurely (explicitly forbidden by §9/the AP), or (b) skip straight from body-recognition to terminal-recognition with no intermediate Unresolved state to report — violating §12's requirement that Unresolved be representable, not just Resolved. Smallest possible change: `struct ConnectorPin { std::string id; std::string connector_id; Point2D position{}; int ordinal = 0; std::vector<std::string> conductor_crossing_evidence_ids; ConfidenceClass confidence = ConfidenceClass::Unresolved; };` — five fields, no changes to any existing struct. |
| `ComponentCandidateClassifier::classify_kind()` extension | **REQUIRED, but scoped to non-mapping** | Existing limitation: a `ShapeKind::ConnectorBody` region reaching the existing `classify_kind()` switch would silently fall to `default: Unknown` without an explicit case. The smallest correct change is **not** a new `ComponentCandidateKind::Connector` value (rejected in §6 — would require every existing `ComponentCandidateKind` consumer to add a case and would conflate component/connector semantics); instead, `ShapeKind::ConnectorBody` regions are recommended to **bypass `ComponentCandidate` entirely** and feed the new `ConnectorCandidate` pipeline directly (§6 already established `ConnectorCandidate` as the correct type). This means `classify_kind()` itself needs no change if the calling code routes `ConnectorBody`-kind `ShapeRegion`s to a separate candidate-construction path rather than through `ComponentCandidateClassifier` at all — an integration/wiring decision for the implementation AP, not a model change. Recorded here as REQUIRED design guidance, not a struct/enum change. |
| `ConnectorCandidate.component_candidate_id` field re-scoping | **NOT REQUIRED (no rename)** | The AP explicitly forbids renaming/removing existing fields. This field's *name* refers to "component" for historical reasons but its *type* (`std::string`, an id reference) is unconstrained — it can continue to hold a reference to whatever upstream candidate owns the connector body. If connector bodies bypass `ComponentCandidate` (previous row), this field would need to reference something else; the smallest non-breaking approach is to populate it with the `ConnectorGeometryDetector`'s own region-derived id (still a valid, stable, non-empty string, satisfying `ConnectorTerminalModelBuilder`'s existing non-empty check) rather than renaming the field. This is a **usage clarification**, not a schema change. |
| `TerminalCandidateKind`, `EndpointKind`, `ConnectorTerminalStatus`, `WireIdentityStatus`, `ElectricalComponentRejectionReason` enums | **NOT REQUIRED** | All already have the exact values needed (`ConnectorBoundary`, `ConnectorTerminal`, `Resolved/Unresolved/Conflicted`, `ConnectorInterface`) confirmed present and already wired to consumers. |
| `Wire` struct | **NOT REQUIRED** | Already endpoint-to-endpoint; no connector-specific field is needed (§9). |
| `ElectricalNet` struct | **NOT REQUIRED** | No connector-specific field needed (§11). |
| Renaming `ConnectorBoundary` from an enum tag to a struct | **REJECTED** | Would be a larger, higher-blast-radius change than this design needs; `ConnectorCandidate` already fills the "connector body" role a `ConnectorBoundary` struct would have filled (§6). Not pursued. |
| Weakening `ShapeDetector`'s `Circle`/`Rectangle`/`ChassisGround` thresholds to catch more connectors | **REJECTED** | Explicitly forbidden by the AP and unnecessary — §7/§13 show a new, additive shape kind with its own evidence is the correct mechanism, not threshold loosening on existing kinds. |

## 18. Serialization Design

New semantic information introduced by this design, and how it is
serialized, respecting existing JSON/determinism/stable-ID conventions
(no serialization code is written in this AP):

- `ShapeKind::ConnectorBody` — serializes exactly like the existing three
  `ShapeKind` values already do (as a named enum string in whatever JSON
  writer already handles `ShapeRegion`); no new serialization pathway,
  only one new valid enum string value.
- `ConnectorPin` — a new top-level collection on `WireModel` (e.g.
  `connector_pins`, following the existing plural-collection-name
  convention visible in the field list: `connector_candidates`,
  `connector_terminals`, `terminal_candidates`, etc.), serialized as an
  array of objects with the same flat-field style every other model
  struct already uses. IDs generated via the existing `stable_id()`
  helper (`core/ids.hpp`), **not** the ad hoc string-concatenation
  convention `ConnectorTerminalModelBuilder`/`ComponentCandidateClassifier`
  currently use — this design recommends new code prefer `stable_id()`
  going forward (noted as a pre-existing inconsistency in the codebase,
  not something this design is authorized to retroactively fix
  elsewhere).
- Crossing-evidence records (§10/§14) — serialized as a new, small
  evidence-record collection (e.g. `connector_conductor_crossing_evidence`),
  following the existing `*_evidence`-collection naming convention already
  used elsewhere in `WireModel` (`conductor_boundary_evidence`,
  `component_identity_evidence`, etc.).
- Determinism: all new collections must be sorted before serialization by
  their own `.id` field, exactly as `ConnectorTerminalModelBuilder`
  already does today for `connectors`/`terminals` — this is a
  continuation of an existing pattern, not a new one.
- No existing field is renamed, removed, or repurposed; no existing
  collection's element type changes shape. Backward compatibility is
  fully preserved — old consumers reading a `WireModel` JSON that now
  additionally contains populated `connector_candidates`/
  `connector_terminals`/(new) `connector_pins` see only additional,
  previously-always-empty collections now containing data, plus one new
  valid `ShapeKind` enum string they were already required to handle
  generically (or ignore) as an unrecognized-but-valid case.

## 19. Validation Invariants

For the future implementation AP, verbatim from the AP's own list plus
this design's additions:

- **Connector count**: exactly 12 source-grounded connector locations in
  the canonical sample; the implementation must not report more or fewer
  `ConnectorCandidate`s than the census unless it can independently
  justify a discrepancy with evidence (never silently drift from 12).
- **Visible pin count**: 33 across all 12 connectors; the same
  non-silent-drift requirement applies to `ConnectorPin` counts.
- **Physical Wire**: no duplicate `Wire` records — unchanged, enforced by
  existing `PhysicalWireIdentityReconstructor` logic, not touched by this
  design.
- **Topology**: connector recognition must create zero new `Junction`,
  `Splice`, or `Crossing` topology nodes — connector recognition operates
  entirely on already-existing `ConductorSegment`/`TopologyNode` data as
  read-only evidence (§9), never mutating topology.
- **Ground**: no connector may become `ChassisGround` merely from
  connector-bar geometry — already true today (§13/Design Question 13,
  0 false positives, no change proposed), and this design adds no new
  interaction that could change that outcome.
- **Terminals**: no `TerminalCandidate` (of any kind, `ComponentBoundary`
  or `ConnectorBoundary`) or `ConnectorTerminal` may be manufactured
  without the same independent `EndpointCandidate`-proximity evidence
  every terminal already requires today (§9) — connector pins do not get
  a lower evidentiary bar.
- **Determinism**: repeated extraction must be byte-identical except
  documented volatile fields — the new stages must follow the same
  deterministic-iteration/deterministic-sort discipline every existing
  stage in this codebase already follows (confirmed pattern across
  `ConnectorTerminalModelBuilder`, `ConductorBoundaryResolver`, etc.).
- **Regression**: all pre-existing non-connector Wires, Nets, Components,
  Ground symbols, and terminal semantics must remain byte-identical to
  today's baseline (37 wires, 12 nets, 81 `ComponentCandidate`s, 0
  resolved/28 unresolved/53 rejected, 6/6 chassis-ground references, 0
  validation errors, 34 runtime warnings) unless the implementation AP
  identifies and documents a specific, legitimate connector-related
  dependency change (e.g. CONN-001's eventual reclassification would be
  such a documented, legitimate change, not a regression).

## 20. Test Architecture

Future implementation test matrix (fixtures/expectations only; no tests
are written in this AP):

| Test area | Expected evidence (not just counts) |
|---|---|
| CONN-001 control | `ConnectorBody` region recognized at `(829,436,21,17)` **in addition to** the existing `Circle` region (both present, non-conflicting); crossing-evidence entries reference real `ConductorSegment`s at the two known wire approaches; resulting `ConnectorPin`/`ConnectorTerminal` status reported, not assumed `Resolved` |
| CONN-002 through CONN-012 | Each: `ConnectorBody` geometry evidence present with the specific vertex-count/aspect values AP-DIAG-AUDIT-012 measured for that location; explicit assertion of which evidence fields are populated vs. absent per connector (e.g. CONN-004/006/012 should assert *insufficient* geometry evidence, not force a false Resolved) |
| Oval connector (CONN-009/010) | Assert aspect-ratio evidence field reflects the measured 1.75–1.92 range; assert this does **not** cause `detect_circles()`'s own `circle_max_aspect_ratio` gate to change (regression guard) |
| Staircase/interlocking connector (CONN-007/008) | Assert vertex-count evidence in the 5–7 range is accepted by `ConnectorBody` recognition while continuing to be rejected by `detect_rectangles()`'s unmodified 4-vertex gate (regression guard) |
| Notched rectangle/oval | Covered by the general geometry-family test set (§15 mapping), one fixture per family, not per connector |
| Interior-void case (CONN-011) | Assert `has_interior_void=true` is recorded from the real child-contour evidence; assert this does **not** get mislabeled as a wire crossing without an actual matching `ConductorSegment` reference (§14) |
| Ground-bar interaction | Regression test: re-run the existing 9 count-gate-clearing ground-bar scenarios (AP-DIAG-AUDIT-012 §14 matrix) and assert 0 `ChassisGround` candidates still result, with the new connector stage active |
| Conductor pass-through | Assert multiple independent crossing-evidence entries are recorded without forced entry/exit pairing (§10) |
| Connector terminal | Assert a `ConnectorTerminal` only reaches `Resolved` when both geometry and independently-found endpoint evidence exist; assert `Unresolved` when only geometry exists |
| Connector boundary | Assert `TerminalCandidateKind::ConnectorBoundary` is only assigned via the same proximity logic already governing `ComponentBoundary`, with a `ConnectorPin` (not a `PrimitiveSymbol`) as the owning geometry |
| Physical Wire relationship | Regression test: assert no `Wire` boundary is created or moved by connector recognition alone, for all 12 connectors, absent independent topology evidence (§9) |
| Electrical Net relationship | Regression test: assert `ElectricalNetResolver`'s output is byte-identical with and without the new connector stages active, except for connector-endpoint semantic labels populated post-hoc by the unmodified `WireSemanticResolver` (§11) |
| False-positive enclosures | Regression test: assert CONN-003/005/009's housing boxes continue to classify as `Enclosure` exactly as today, unaffected by the new stage recognizing their adjacent connector notches |
| False-positive ground symbols | Regression test: assert all 9 known count-gate-clearing ground-bar runs remain rejected (duplicate of the ground-bar-interaction row, listed separately per the AP's explicit checklist) |
| Deterministic repeated extraction | Run extraction twice with the new stages active; assert byte-identical `topology.json` (or its future connector-inclusive equivalent), following this engagement's established SHA-256 comparison method |

## 21. Implementation Boundary

For the next (implementation) AP:

**Permitted production files (new):**
- `include/eke_dx_wire/image/connector_geometry_detector.hpp` (new)
- `src/image/connector_geometry_detector.cpp` (new)

**Permitted production files (extended, narrow diffs only):**
- `include/eke_dx_wire/image/shape_detector.hpp` — add
  `ShapeKind::ConnectorBody` enum value only; no threshold/struct changes.
- `src/image/component_candidate_classifier.cpp` — routing change only
  (bypass to the new connector path for `ConnectorBody`-kind regions, per
  §17); no change to existing `classify_kind()` cases.
- `include/eke_dx_wire/topology/terminal_location_detector.hpp` /
  `src/topology/terminal_location_detector.cpp` — extend input to accept
  `ConnectorPin` locations as additional candidate-attachment points,
  using existing distance-threshold logic.
- `include/eke_dx_wire/topology/terminal_recognizer.hpp` /
  `src/topology/terminal_recognizer.cpp` — same extension as above.
- `include/eke_dx_wire/pipeline/extraction_pipeline.hpp` /
  `src/pipeline/extraction_pipeline.cpp` — wire in the new detector/stage
  call sites; no reordering of existing stages.

**Permitted model files:**
- `include/eke_dx_wire/core/model.hpp` — add `ConnectorPin` struct and
  the crossing-evidence record struct only (§17/§18); no modification to
  any existing struct or enum, no field renames/removals.
- `src/core/ids.cpp` — only if a new `stable_id()` namespace constant is
  needed for the new types (additive only).

**Explicitly NOT permitted for the implementation AP without a further,
separate design review:**
- Any change to `ShapeDetector`'s existing `detect_rectangles()`,
  `detect_circles()`, or `detect_ground_symbols()` bodies, or any
  existing `ShapeDetectorConfig` threshold.
- Any change to `ConnectorTerminalModelBuilder`, `ConductorBoundaryResolver`,
  `PhysicalWireIdentityReconstructor`, `ElectricalNetResolver`, or
  `WireSemanticResolver`'s existing logic bodies.
- Any change to `Wire`, `ElectricalNet`, `TerminalCandidate`,
  `EndpointCandidate`, or `ComponentCandidate` struct fields.
- Any change to `ComponentCandidateKind`, `TerminalCandidateKind`,
  `EndpointKind`, `ShapeRole`, `ConnectorTerminalStatus`,
  `WireIdentityStatus`, or `ElectricalComponentRejectionReason` enum
  values.

**Permitted test files (new):**
- `tests/test_connector_geometry_detector.cpp` (new)
- Extensions (not rewrites) to `tests/test_terminal_location_detector.cpp`,
  `tests/test_terminal_recognizer.cpp`, `tests/test_connector_terminal_model.cpp`,
  `tests/test_shape_detector.cpp` (new cases only, no existing case
  deleted or weakened).

**Permitted fixture files:**
- New sample/test-fixture rasters or cropped regions under whatever
  existing fixture directory convention `tests/` already uses (to be
  confirmed by the implementation AP against the actual fixture layout;
  not enumerated here since this design did not survey the fixture
  directory).

## 22. Design Gates

- **Gate A** (does the geometry model represent all 12 families?): **PASS.**
  §15 maps every AP-DIAG-AUDIT-012 family (A, B, D, E, F, G, I, J) onto
  the single evidence-based `ConnectorBody` model from §4/§7 without a
  per-connector rule or per-family detector.
- **Gate B** (can all 33 pins be represented without conflating with Wire
  endpoints?): **PASS.** §17's new `ConnectorPin` type exists specifically
  to hold this intermediate, non-endpoint state; §9 enforces the
  four-stage gate (pin → terminal candidate → connector terminal → Wire
  endpoint) with independent evidence at each step.
- **Gate C** (can Wire identity semantics remain unchanged?): **PASS.**
  §9/§16/§19 — `Wire`'s struct, `PhysicalWireIdentityReconstructor`'s
  logic, and the endpoint-to-endpoint definition are all explicitly
  unmodified; connector pins never directly create Wire boundaries.
- **Gate D** (can Net resolution remain unchanged except for legitimate
  connector-terminal evidence?): **PASS.** §11/§16 — `ElectricalNetResolver`
  is unmodified; the only new net-adjacent behavior is
  `WireSemanticResolver`'s existing (unmodified) `find_resolved_connector_terminal()`
  finally having real data to find, which is a legitimate use of already-
  existing logic, not a new net-resolution mechanism.
- **Gate E** (can detection avoid the known ground false-positive
  patterns?): **PASS.** §13 (Design Question 13) — no ground-detector
  change is proposed; the existing width-sequence/spacing checks already
  produce 0 false positives against all 9 known connector-generated
  bar patterns, and this design introduces no new interaction with the
  ground detector at all.
- **Gate F** (can the design represent an unresolved connector without
  guessing?): **PASS.** §12's three-state evidence model
  (Resolved/Unresolved/Conflicted) explicitly allows — and expects, for
  most of the 11 currently-absent connectors — an `Unresolved` outcome
  where geometry evidence exists but endpoint/conductor evidence does
  not yet clear the bar; nothing in this design forces a guess.
- **Gate G** (is every required model change justified?): **PASS.** §17
  enumerates every proposed change with an explicit REQUIRED/OPTIONAL/
  NOT REQUIRED/REJECTED classification, each REQUIRED entry backed by a
  cited existing limitation and forensic evidence, and explicitly
  rejects the two most tempting but unjustified shortcuts (a new
  `ComponentCandidateKind::Connector` value, and weakening existing
  `ShapeDetector` thresholds).

All seven gates pass; per the AP's own rule, the design is complete.

## 23. Explicit Non-Goals

This design does **not**:

- Implement any of the proposed types, detectors, or stage extensions.
- Change any `ShapeDetector` threshold, kernel, or epsilon value.
- Add a `ConnectorBoundary` struct (the enum tag is retained; `ConnectorCandidate` fills its role).
- Resolve pairing between independent entry/exit conductors through a
  connector by positional convention (§10 — explicitly deferred to
  evidence-based resolution in a future AP).
- Attempt to recover connectors with no coherent boundary ink at all
  (the category-E connectors, CONN-004/006/012, are expected to remain
  `Unresolved` even after implementation, absent new evidence sources).
- Rely on "repeated symbol convention" or diagram-wide "source-context"
  evidence, since neither was established as measurable by the prior
  forensic APs (§12).
- Change `ElectricalNetResolver`, `PhysicalWireIdentityReconstructor`, or
  `ConductorBoundaryResolver` logic in any way.
- Change CMake, build configuration, or any test currently passing.
- Guess a Resolved status for any connector, pin, or terminal absent the
  evidence this design requires.

## 24. Recommended Implementation AP

**AP-DIAG-IMPL-001 — Connector Geometry Detector and Pin Model
(implementation of AP-DIAG-DESIGN-001).** Scope: implement exactly the
"Permitted" file list in §21, in this order: (1) `ShapeKind::ConnectorBody`
enum addition, (2) the new `ConnectorGeometryDetector` component
(geometry-only, no pipeline wiring yet, covered by its own unit tests
against the 12 known connector ROIs), (3) the `ConnectorPin` model
addition and its serialization, (4) the `TerminalLocationDetector`/
`TerminalRecognizer` extension to consider `ConnectorPin` locations, (5)
pipeline wiring, (6) full regression per §19's invariants and the test
matrix in §20. Recommend explicitly scoping AP-DIAG-IMPL-001 to stop at
"connectors reach `Unresolved`/`Resolved` status with correct evidence"
and deferring entry/exit pairing resolution (§10) and cross-connector
"repeated symbol convention" evidence (§12) to a later AP, consistent
with this design's non-goals (§23).

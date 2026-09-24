# AP-WIRE-026 — Unified Engineering Diagram Reconstruction

## Purpose

Establish `EngineeringDiagram`: the engineering-model boundary between
extraction (AP-WIRE-022 through AP-WIRE-025) and future rendering/export
stages (AP-WIRE-027+). It is a read-only, reference-preserving assembly
over an already-complete `WireModel` - never a new extraction algorithm,
never a second source of truth.

## Reference image: what it is and isn't

`samples/trx300_complete_diagram_view.png` is a rendering from OEP
Diagram Studio of the same 1988 Honda FourTrax TRX300 harness. It is a
**target representation, not extraction input and not a new evidence
source**. Studying it (full-resolution, not the source scan) surfaced:

- A completely different page **layout** than the source scan - components
  are arranged in a clean auto-routed grid, not the source diagram's
  original positions. Wires are drawn Manhattan-style (orthogonal
  right-angle routing), not as the source's physical trace.
- Every wire has a distinct, consistent color matching real wiring-color
  convention (black, red, green, blue, orange/brown, pink, gray, light
  green, etc.).
- Connector bodies are drawn as rounded light-blue rectangles with
  numbered pin dots inside; components are drawn as labeled boxes with an
  actual internal electrical symbol (lamp filament circle, switch lever,
  coil zigzag, motor "M" circle, diode arrow-bar, three-phase alternator
  winding, battery block, relay coil+switch).
- Splices are filled dots where same-colored wires visually join.
- Every pin/terminal carries a number or short code (`BAT2`, `IG1`, `TL`,
  etc.) and every component/connector carries a title-case label.

This directly confirms the task's framing: **the image is a presentation
of an engineering model**, not merely a drawing. It does **not** mean
AP-WIRE-026 should infer symbol families from visual similarity to this
picture, invent connector identity, or adopt its coordinate/layout
scheme - per the AP's explicit prohibition and the standing
evidence/status architecture. See the coverage table in
`docs/AP-WIRE-026_AAR.md` §10 for exactly what the current model can and
cannot yet represent toward reproducing it.

## Model

```
EngineeringDiagram
  |
  +-- components[]            (DiagramComponent, references ComponentCandidate.id)
  +-- connectors[]             (DiagramConnector, references ConnectorCandidate.id)
  +-- wires[]                  (DiagramWire, references Wire.id)
  +-- splices[]                (DiagramSplice, TopologyNode Splice/Junction + incident wires)
  +-- crossing_node_ids[]       (TopologyNode Crossing - visual only, no electrical significance)
  +-- labels[]                  (DiagramLabel, one per TextRegion, always - never discarded)
  +-- electrical_nets[]          (DiagramElectricalNet, references ElectricalNet.id + derived wire_ids)
  +-- validation                 (DiagramValidationReport)
```

Every object carries the **same deterministic ID** already established by
the AP that produced it. `EngineeringDiagram` never invents a new ID
scheme or a parallel identity for something that already has one - see
`include/eke_dx_wire/diagram/engineering_diagram.hpp` for the full field
list and rationale comments on every non-obvious field.

### Identity/geometry/semantics/evidence/status, kept distinct

Per the AP's explicit requirement, these are never collapsed:

- `DiagramComponent.symbol_geometry_id` is a *reference* to AP-WIRE-023
  geometry - it is evidence about what is drawn, never identity.
- `DiagramComponent.canonical_name` is populated **only** when
  `ComponentIdentityCanonicalizationStatus::Resolved`; a component label
  (`semantic_labels`) is kept as a separate field and is not treated as
  canonical identity.
- `DiagramComponent.terminal_candidate_ids` (geometric/associative
  evidence, AP-WIRE-024) is a different list from `endpoint_ids`
  (semantically *resolved* association, AP-WIRE-019) - a terminal
  candidate existing does not mean the association resolved.
- `DiagramWire` and `DiagramElectricalNet` are separate top-level
  collections; the only link between them is a derived, read-only
  `wire_ids` list on the net (wires are never "inside" a net, nets are
  never "inside" a wire).
- `DiagramObjectStatus` (`Resolved`/`Unresolved`/`Conflicted`) is kept as
  an explicit field everywhere a resolution exists - never collapsed to a
  boolean `valid`.

### Splices and shared conductors

A `DiagramSplice` is built from `TopologyNode` (type `Splice`/`Junction`)
plus a derived `incident_wire_ids` list (every `Wire` whose
`topology_edges` touch that node). This does not change wire identity in
any way - it is purely an additional, explicit, queryable view of the
already-legitimate "two wires share this splice" relationship that
AP-WIRE-022A's coverage diagnostics could already detect in aggregate
(`CONDUCTOR-SHARED`) but had no first-class object for. No wire is split
at a splice; no splice becomes a wire endpoint.

### Coordinate system

Every position/bounds value is in the same coordinate system as the
source `WireModel`: normalized source/page pixel coordinates, origin
top-left, no scaling, inversion, or translation
(`kEngineeringDiagramCoordinateSystem = "source_page_pixels"`, declared in
the header). This is the *only* coordinate system this AP establishes.
The reference image's auto-laid-out schematic canvas is a **different**
coordinate space that a future rendering stage would compute (an explicit
layout algorithm) - `EngineeringDiagram` does not attempt page-layout or
auto-routing, per the AP's own instruction that visual fidelity belongs
to rendering, not the model.

## Builder

`EngineeringDiagramBuilder::build(const WireModel&) -> EngineeringDiagram`
(`src/diagram/engineering_diagram_builder.cpp`) is a pure function: input
by `const&`, output by value, zero mutation of the input. It performs no
OCR, fuzzy matching, component recognition, topology inference, wire
reconstruction, terminal invention, or canonical-identity guessing -
every one of those is already done by an earlier AP; this only joins and
validates their output.

### Validation

Independently re-derives (does not trust any upstream summary) reference
integrity across 9 categories: wire→endpoint (start and end),
wire→topology edge, terminal→endpoint, terminal→component,
connector-terminal→connector, symbol-geometry→component,
primitive→component, electrical-net→endpoint, wire-semantic-resolution→wire.
Also checks for duplicate terminal/connector-terminal pairs and duplicate
wire-semantic-resolutions, and flags a narrower "orphan" case: a real
(non-furniture) component with zero terminals, zero endpoints, *and* zero
symbol geometry at all. Every failure becomes an explicit
`DiagramRelationshipIssue`, never a silently dropped reference.

## Export

`artifacts/engineering_diagram/engineering_diagram.json`
(`EngineeringDiagramExporter`, same hand-rolled JSON convention as every
other exporter in this codebase), written by
`ExtractionArtifactWriter::write()` **after** every other artifact, so
building the diagram cannot influence anything written earlier.

No new review-image layer was added: this AP is explicitly about model
capability, not visual fidelity ("Do not attempt to make the extraction
output visually identical to the reference image during AP-WIRE-026" -
task §"ADDITIONAL INSTRUCTION"). A rendering-focused review layer belongs
to AP-WIRE-027.

## Tests

`tests/test_engineering_diagram_builder.cpp`, 25 cases covering every
item in the AP's required regression list (see the AAR for the full
enumeration), including the two safety-critical cases: an AP-WIRE-024-
shaped conflicted endpoint is never fabricated into a component
association, and a splice with two branching wires is represented via
shared incidence, never duplicated geometry or mutated wire identity.

## Validation

See `docs/AP-WIRE-026_AAR.md`, including the required reference-image
coverage table and architecture assessment.

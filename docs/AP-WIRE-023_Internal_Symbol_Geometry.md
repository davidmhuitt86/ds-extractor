# AP-WIRE-023 — Internal Symbol Geometry Extraction

## Purpose

Establish the geometric layer AP-WIRE-022A's AAR found missing: an
explicit, inspectable model of what is actually drawn *inside* an
already-detected real `ComponentCandidate`'s bounding region. AP-WIRE-022A
found that 41 of 59 real components had no terminal/endpoint evidence;
the underlying cause was that the extractor detected component regions
but discarded (or never modeled) the geometry inside them. AP-WIRE-023
closes that gap in the model, not in the wire/terminal semantics.

## Scope

- Extract deterministic geometric primitives (`SymbolPrimitive`) from the
  interior of each real (non-`DiagramFurniture`) `ComponentCandidate`.
- Attach them to an explicit `ComponentSymbolGeometry` record per
  component.
- Preserve provenance, confidence, and parent-component relationship for
  every primitive.
- Export the new model through the existing artifact-writer architecture
  (`topology.json`, `extraction_audit.json`) and a new review layer.

## Non-goals

- **Not** terminal recognition or component-terminal association
  (AP-WIRE-024). A `TerminalLead` primitive is geometric shape evidence
  only; it is never turned into an `EndpointCandidate`.
- **Not** electrical-symbol semantic classification. No stage maps
  observed geometry to a symbol family ("relay", "motor", "switch") -
  that would violate the explicit prohibition in the AP spec (§5/§12).
- **Not** a wire, topology, or electrical-net repair. AP-WIRE-022A's
  baseline (294/692/877/210/40/10) is unchanged by this AP - see the AAR
  for the regression check.
- **Not** dependent on the Anthropic vision provider or any network
  access. `SymbolGeometryExtractor` operates only on the normalized page
  image and already-detected `ComponentCandidate` geometry.

## Domain model

```
ComponentCandidate
      |
      +-- ComponentSymbolGeometry   (1:1, id = "component-symbol-geometry-" + component.id)
                |
                +-- SymbolPrimitive
                +-- SymbolPrimitive
                +-- ...
```

`ComponentSymbolGeometry` (`include/eke_dx_wire/core/model.hpp`):

- `id`, `component_id`
- `primitive_ids` (ordered list of the component's `SymbolPrimitive` IDs)
- `confidence` - the strongest confidence among its primitives, or
  `Unresolved` when the component produced none (a legitimate outcome,
  not an error - see Limitations).

`SymbolPrimitive`:

- `id` (content-addressed, `stable_id("symbol-primitive", ...)`)
- `component_id` (parent reference)
- `kind` - see Primitive types
- `bounds` (`BoundingBox`, absolute source-image coordinates)
- `area`, `confidence`, `provenance`

Every `WireModel` now carries `component_symbol_geometries` and
`symbol_primitives` alongside the existing `component_symbol_recognitions`
(AP-WIRE-021, still the coarse outer-shape bucket - unchanged).

## Primitive types

Scoped to what the TRX300 fixture actually contains (AP spec §7/§21: do
not invent symbol families the fixture does not represent):

| Kind | Meaning |
|---|---|
| `Line` | Thin, elongated internal blob, fully clear of the component's boundary margin. |
| `Circle` | Compact, roughly square bounding box, high fill ratio, and contour circularity above threshold. |
| `Rectangle` | Filled blob with a bounded aspect ratio that is not circular. |
| `TerminalLead` | Elongated blob that touches the boundary margin - i.e. appears to reach toward/through the component's own outline. Geometric evidence only; never an `EndpointCandidate`. |
| `Unknown` | Anything that does not clearly match the above. Valid and expected - not forced. |

`Polyline`, `Arc`, `Contact`, `Plate`, `Coil`, `GroundPrimitive`,
`JunctionMarker` from the AP spec's illustrative list were **not**
implemented: the TRX300 inventory (see §"TRX300 inventory" below) did not
produce evidence distinguishing them from the five kinds above at the
achievable detection fidelity. Adding unused enumerators the pipeline
never produces would be exactly the "invented symbol family" the spec
warns against.

## Geometry ownership / exclusion

A component's own outer shape (circle/rectangle/chassis-ground bars) is
already modeled by `ShapeDetector`/`ComponentCandidateClassifier` as its
`ShapeRegion`/`ComponentCandidateKind`. Rediscovering that same outline as
an "internal primitive" would double-count it and pollute the primitive
population. `SymbolGeometryExtractor` avoids this by zeroing a
configurable `boundary_margin` (default 2px) around the cropped component
region before connected-component analysis - not by classifying and
discarding the outer contour, which would be less robust across shape
kinds. A blob that survives this margin and is still elongated enough is
classified `TerminalLead` rather than discarded, since a genuine lead
reaching the component's boundary is exactly the geometry AP-WIRE-024
needs.

`DiagramFurniture` candidates are filtered out before any of this runs
(`component.kind == ComponentCandidateKind::DiagramFurniture` short-
circuits per component) - see AP-WIRE-022A's finding that the
switch-continuity table was previously misclassified as component/
connector geometry. AP-WIRE-023 does not touch or second-guess
`DiagramFurnitureClassifier`.

## Confidence

Uses the existing `ConfidenceClass` (`High`/`Medium`/`Low`/`Unresolved`):

- `High`: unambiguous shape match (e.g. contour circularity ≥ 0.85, or a
  thin line ≤ 2px thick).
- `Medium`: shape match with some ambiguity (e.g. circularity 0.75-0.85,
  or a filled blob with fill ratio 0.75-0.90).
- `Low`: weak or partial evidence, including every `Unknown` primitive
  and every `TerminalLead` that does not clearly exceed the lead aspect
  threshold.
- `Unresolved`: a `ComponentSymbolGeometry` with zero primitives (the
  component's interior, after boundary exclusion, contained nothing
  above the noise floor).

No primitive is ever assigned artificial high confidence to make a count
look better - see the honest reporting in the AAR.

## Deterministic IDs and ordering

`SymbolPrimitive.id` is `stable_id("symbol-primitive", component_id +
":" + local_bounds + ":" + kind)` (`src/core/ids.cpp`'s existing FNV-1a
content hash - the same mechanism every other deterministic ID in this
codebase already uses). It depends only on the parent component and the
primitive's own geometry/classification, never on vector insertion order,
pointer values, or timestamps. `component_symbol_geometries` and
`symbol_primitives` are both sorted by `id` before being returned, so two
runs against the same image and configuration produce byte-identical
output (verified by a regression test - see Tests).

## Pipeline position

```
Shape Detection
      |
Component Candidate Classification
      |
Diagram Furniture Classification
      |
INTERNAL SYMBOL GEOMETRY   <- AP-WIRE-023 (this AP)
      |
(terminal/endpoint reconstruction, wire reconstruction, electrical-net
 resolution - all unchanged by this AP)
```

Implemented in `ExtractionPipeline::run` (`src/pipeline/extraction_pipeline.cpp`)
immediately after `ComponentSymbolRecognizer`, before terminal-location
detection, wire reconstruction, or net resolution - it neither reads nor
writes any of those structures.

## Export

- `topology.json` gains `symbol_primitives` and
  `component_symbol_geometries` arrays, written by the existing
  `TopologyExporter` (`src/export/topology_exporter.cpp`) using the same
  hand-rolled JSON conventions already used for every other array in that
  file. No second/competing export format was introduced.
- `extraction_audit.json` gains a `symbol_geometry` block: components
  with/without geometry, total primitive count, and a primitive-kind
  histogram (`src/export/artifact_writer.cpp`).
- `review_manifest.json`'s `layers` block gains a `symbol_geometry` count
  and `coverage_summary` gains `components_with_symbol_geometry` /
  `components_without_symbol_geometry`.

## Review rendering

New layer `artifacts/extraction_review/14_symbol_geometry.png`
(`render_symbol_geometry` in `src/export/review_artifact_writer.cpp`):
draws every real component's bounding box (thin purple, furniture
excluded) plus every extracted primitive, color-coded by kind (line =
cyan, circle = orange, rectangle = green, terminal lead = red, unknown =
gray). The renderer only reads `model.component_symbol_geometries` /
`model.symbol_primitives` - it does not independently re-run any
detection, so the layer is a true projection of the engineering model
per the AP spec's source-of-truth hierarchy.

## Tests

`tests/test_symbol_geometry_extractor.cpp` (synthetic, deterministic,
no dependency on the real fixture): single line, multiple lines, circle,
rectangle, contact-like paired-plate geometry, terminal-lead (boundary-
touching elongated blob), mixed primitives in one component, an empty
component region, an irregular/ambiguous blob (must never be reported
High confidence), `DiagramFurniture` exclusion, deterministic IDs/
ordering across two runs of the same input, confidence propagation from
primitive to geometry, and parent-component isolation across multiple
components in one image.

`tests/test_coverage_diagnostics.cpp` and the rest of the AP-WIRE-022A
suite are unaffected and continue to pass (this AP does not touch
coverage-diagnostic logic).

## TRX300 baseline

See `docs/AP-WIRE-023_AAR.md` for the full baseline, the failure/limits
analysis, and the AP-WIRE-024 correlation table
(`docs/AP-WIRE-023_component_symbol_geometry_correlation.csv`).

## Known limitations

- Detection is deterministic connected-component + shape-metric
  classification, not a learned or template-matched recognizer. It
  reliably finds compact internal marks (circles, rectangles, short
  leads) but will miss internal structure thinner or more irregular than
  a plain-geometry threshold can separate from noise.
- Many TRX300 `CircularSymbol` candidates are, on inspection, simple
  unlabeled ring/dot symbols with no internal geometry beyond their own
  outline - after boundary-margin exclusion, `ComponentSymbolGeometry`
  correctly reports `Unresolved` (zero primitives) for these. This is not
  a detection failure; it is an accurate reflection of what is drawn.
- `TerminalLead` is a boundary-touching, elongated-blob heuristic, not a
  detector of electrical leads per se. It should be read by AP-WIRE-024
  as "worth checking for a terminal here," not as confirmed terminal
  geometry.

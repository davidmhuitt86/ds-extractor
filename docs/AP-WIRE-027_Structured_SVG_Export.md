# AP-WIRE-027 — Structured Engineering SVG Export

## Purpose

Render an already-built `EngineeringDiagram` (AP-WIRE-026, itself a
read-only projection over `WireModel`) as real, structured SVG markup.
This is the extraction pipeline's first output stage that produces
something intended to look like an engineering diagram — but it is a
**renderer**, not an extractor: it consumes already-resolved evidence and
never derives, infers, or guesses new engineering meaning. Every rendering
decision branches on a status the pipeline already established
(`Resolved` / `Unresolved` / `Conflicted`), never on raw geometry.

## Boundary

```
EngineeringDiagram + WireModel  →  StructuredSvgExporter  →  output/wires.svg
```

- Input: `const EngineeringDiagram&` (component/connector/wire/splice/
  label/net views) plus `const WireModel&` (dereferenced for geometry,
  terminal positions, text-region bounds, wire-semantic and symbol-family
  resolution detail — never duplicated into `EngineeringDiagram` itself,
  consistent with that AP's reference-only design).
- Output: one deterministic SVG string / file. No raster `<image>`
  fallback; no source-image or OpenCV dependency in the exporter itself
  (verified: `grep -n "opencv\|cv::"` across `structured_svg_exporter.*`,
  `symbol_renderer.*`, `svg_structure_validator.*` returns nothing).
- Coordinate system: `source_page_pixels`, unchanged — the SVG `viewBox`
  matches `EngineeringDiagram::image_width/height` exactly, and every
  emitted coordinate is copied verbatim from `WireModel`/`EngineeringDiagram`
  geometry. No layout transform was introduced in this AP (the "minimal
  mode" the spec permits); a future auto-layout stage would live outside
  this renderer and would need to be explicit, deterministic, reversible,
  and non-destructive to source coordinates per the spec's rules.

## Architecture

- `SymbolRenderer` (`include/.../export/symbol_renderer.hpp`,
  `src/export/symbol_renderer.cpp`): an abstract, presentation-only glyph
  generator. One concrete renderer per `SymbolFamily`
  (Ground/Lamp/Switch/Relay/Motor/Diode/Alternator/Battery/Solenoid/Coil),
  plus explicit `unresolved_symbol_renderer()` and
  `conflicted_symbol_renderer()` singletons. A `SymbolRenderer` receives
  only a `BoundingBox` — it has no access to the source image, OCR text,
  or any recognition evidence, and cannot itself decide which family to
  use. It never performs recognition; `StructuredSvgExporter` is the only
  caller and it gates the choice strictly on `SymbolFamilyResolution.status`.
- `StructuredSvgExporter` (`include/.../export/structured_svg_exporter.hpp`,
  `src/export/structured_svg_exporter.cpp`): the renderer itself. Builds
  ID→object lookup maps from the `WireModel` once, then emits one `<svg>`
  document containing a single `<g id="engineering-diagram">` with these
  ordered subgroups: `components`, `connectors`, `wires`, `splices`,
  `crossings`, `terminals`, `grounds`, `labels`, `annotations`,
  `electrical-nets`, `metadata`.
- `SvgStructureValidator` (`include/.../export/svg_structure_validator.hpp`,
  `src/export/svg_structure_validator.cpp`): a lightweight, regex-based
  (no new XML/DOM library) post-hoc checker. Confirms a valid SVG root, no
  raster fallback, unique element `id=` values, every `data-*-id`
  reference resolving to a real `EngineeringDiagram`/`WireModel` object,
  and no symbol-family glyph rendered for a non-`Resolved`
  `SymbolFamilyResolutionStatus`.

## Rendering rules (by object)

- **Components**: `DiagramFurniture` is never rendered as a circuit
  component (legend/title-block content, per AP-WIRE-023). Every other
  component renders its bounds plus a symbol group gated on
  `symbol_family_resolution_id`: `Resolved` → `symbol_renderer_for(family)`;
  `Conflicted` → `conflicted_symbol_renderer()`; `Unresolved` (or no
  resolution at all) → `unresolved_symbol_renderer()`. `data-source-label`
  and `data-canonical-name` are both preserved as distinct attributes —
  neither replaces the other.
- **Connectors / terminals**: rendered as their own group; a
  `ConnectorTerminal` only carries `data-terminal-name` when
  `ConnectorTerminalStatus::Resolved`. `TerminalCandidate`s render as
  small markers traceable to their `endpoint_id`/`component_id`.
- **Wires**: one `<g>` per `Wire`, endpoint-to-endpoint (never split at a
  splice). Its `conductor_segment_ids` render as real `<line>` geometry —
  multiple segments render as one visual wire without inventing a merged
  polyline. Color is gated on `WireSemanticResolution.wire_color_status`:
  `Resolved` → raw text preserved in `data-wire-color` plus a best-effort
  display-color mapping (an unmapped resolved value still keeps its raw
  text — never dropped); `Unresolved` → neutral stroke,
  `data-wire-color-status="unresolved"`, no `data-wire-color` attribute
  at all; `Conflicted` → dashed stroke, explicit status, never one chosen
  winning color.
- **Splices vs. crossings**: a `Splice` topology node renders as a filled
  dot (`data-object-type="splice"`); a `Crossing` node renders as an
  unfilled ring in a separate `crossings` group
  (`data-object-type="crossing"`) — visually and structurally distinct, and
  a crossing never implies electrical continuity.
- **Grounds**: `EndpointKind::Ground` endpoints render a ground-symbol
  glyph in their own `grounds` group, distinct from a
  `SymbolFamily::Ground` (chassis-ground) *component* glyph — the two are
  different evidence sources and are kept visually and structurally
  separate.
- **Labels/annotations**: every `DiagramLabel` renders — a region with no
  recognized text still renders (as a dashed placeholder, never invented
  text) so "unknown" never means "missing". An `Unknown`-kind label
  renders in the `annotations` group; every other kind renders in
  `labels`.
- **Electrical nets**: rendered as pure metadata
  (`electrical-nets` group) — never merged into wire geometry, consistent
  with the standing physical-layer/electrical-layer distinction.

## Provenance / metadata scheme

Every rendered element that corresponds to an engineering object carries
`data-object-type` plus the relevant `data-*-id` reference(s)
(`data-component-id`, `data-wire-id`, `data-terminal-id`,
`data-endpoint-id`, `data-node-id`, `data-net-id`,
`data-conductor-segment-id`, `data-text-region-id`, …), and status/
confidence attributes where applicable (`data-symbol-family-status`,
`data-wire-color-status`, `data-status`, `data-*-confidence`). This is the
exact schema `SvgStructureValidator` checks references against.

## Canonical artifact location

`StructuredSvgExporter::export_svg()` replaces the old raw-line-dump
`SvgExporter` (retired and deleted in this AP) at the same call site in
`ExtractionArtifactWriter::write()`, writing to the same canonical
`output/wires.svg` path — no second, competing SVG artifact tree was
introduced.

## What this AP deliberately does not do

- Does not increase symbol-family recognition coverage. The TRX300
  baseline stays 10 Resolved / 49 Unresolved / 0 Conflicted — the
  renderer draws whatever `SymbolFamilyRecognizer` (AP-WIRE-026A) already
  decided, nothing more.
- Does not introduce any geometric heuristic ("circular + two terminals =
  lamp") in `SymbolRenderer` or `StructuredSvgExporter`. If recognition
  coverage needs to improve, that is a future recognition AP's job.
- Does not introduce an auto-layout engine. Source coordinates are used
  as-is.
- Does not redesign the GUI. The output is structured and provenance-
  tagged so a future Diagram Studio consumer can use it, but no GUI work
  was done here.

See `docs/AP-WIRE-027_AAR.md` for full validation results.

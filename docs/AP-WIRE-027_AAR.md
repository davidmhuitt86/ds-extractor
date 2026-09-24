# AP-WIRE-027 — After-Action Report

## 1. Baseline commit

`d951811` — "AP-WIRE-026A: design doc, AAR, and status sync" (last commit
before this AP's work began; AP-WIRE-022 through AP-WIRE-026A complete
and validated).

## 2. Implementation commits

- `437c2db` — partial commit (an intermediate `git add` invocation
  referenced an already-deleted path and failed atomically, so this
  commit landed only the `SvgExporter` deletion half of the intended
  change; left in history rather than rewritten, per the project's
  no-history-rewriting rule).
- `571c42c` — "AP-WIRE-027: structured engineering SVG renderer" — the
  full implementation: `SymbolRenderer`, `StructuredSvgExporter`,
  `SvgStructureValidator`, `ExtractionArtifactWriter` wiring, old
  `SvgExporter` retirement.
- `b14005d` — "AP-WIRE-027: add structured SVG exporter and validator
  tests" — the 36-case + 10-case test suites.

## 3. Final validation commit

This AAR and the accompanying design doc
(`docs/AP-WIRE-027_Structured_SVG_Export.md`) are committed together as
the closing commit for this AP.

## 4. Build environment

- Linux sandbox, `g++ 13.3.0`, `cmake 3.28.3`.
- OpenCV **5.1.0**, built from source, installed at `/usr/local`
  (`-DOpenCV_DIR=/usr/local/lib/cmake/opencv5`) — required because the
  project uses `opencv2/geometry/2d.hpp` (OpenCV 5-only API); apt only
  offers 4.6, so it is never used.
- `libcurl` linked for the existing vision-recognition provider
  (unrelated to this AP; unused by the new renderer).
- Full clean rebuild performed from a deleted `build/` directory
  immediately before this AAR was written: `EXIT:0`, zero errors.

## 5. Complete CTest results

```
100% tests passed, 0 tests failed out of 50
Total Test time (real) =   0.26 sec
```

50 = 48 pre-existing tests (unchanged) + `dx-wire-test-structured-svg-exporter`
(36 cases) + `dx-wire-test-svg-structure-validator` (10 cases), both new
in this AP.

## 6. Warnings

The clean `-Wall -Wextra -Wpedantic` build produces zero warnings from
any file this AP touched or added
(`symbol_renderer.*`, `structured_svg_exporter.*`,
`svg_structure_validator.*`, `artifact_writer.cpp`, `main.cpp`,
`CMakeLists.txt`, both new test files). Seven pre-existing warnings from
unrelated files (`rejected_geometry_classifier.cpp`,
`geometry_ownership_classifier.cpp`, `shape_detector.cpp`, two dead
helper functions in `artifact_writer.cpp` predating this AP, and
`[[nodiscard]]`-ignoring lines in two pre-existing tests) were confirmed
present in the `d951811` baseline before this AP began and are out of
this AP's scope.

## 7. SVG rendering architecture

`SymbolRenderer` is a pure presentation boundary: one concrete class per
`SymbolFamily` (Ground/Lamp/Switch/Relay/Motor/Diode/Alternator/Battery/
Solenoid/Coil) plus explicit `unresolved_symbol_renderer()` and
`conflicted_symbol_renderer()` singletons, each taking only a
`BoundingBox` and returning a local-coordinate SVG glyph fragment. No
`SymbolRenderer` has access to the source image, OCR text, or recognition
evidence, and none decides which family applies — that decision was
already made upstream by AP-WIRE-026A's `SymbolFamilyRecognizer`.
`StructuredSvgExporter` is the only caller, and it gates the choice
strictly on `SymbolFamilyResolution.status`:

```cpp
if (resolution && resolution->status == SymbolFamilyResolutionStatus::Resolved) {
    // symbol_renderer_for(resolution->family)
} else if (resolution && resolution->status == SymbolFamilyResolutionStatus::Conflicted) {
    // conflicted_symbol_renderer()
} else {
    // unresolved_symbol_renderer()
}
```

No geometric heuristic (e.g. "circular + two terminals = lamp") exists
anywhere in `src/export/symbol_renderer.cpp` or
`src/export/structured_svg_exporter.cpp` — confirmed by full read of both
files.

## 8. Group hierarchy produced

```
<svg>
  <g id="engineering-diagram">
    <g id="components">      <!-- DiagramFurniture excluded -->
    <g id="connectors">
    <g id="wires">
    <g id="splices">
    <g id="crossings">
    <g id="terminals">
    <g id="grounds">         <!-- EndpointKind::Ground markers -->
    <g id="labels">          <!-- non-Unknown-kind DiagramLabel -->
    <g id="annotations">     <!-- Unknown-kind DiagramLabel -->
    <g id="electrical-nets"> <!-- metadata only, no geometry -->
    <g id="metadata">
```

Verified present, in this order, in the real TRX300 output (test #28 in
`test_structured_svg_exporter.cpp` also asserts this ordering on a
synthetic empty diagram).

## 9. Provenance / metadata scheme

Every rendered engineering object carries `data-object-type` plus the
relevant `data-*-id` reference (`data-component-id`, `data-wire-id`,
`data-terminal-id`, `data-endpoint-id`, `data-node-id`, `data-net-id`,
`data-conductor-segment-id`, `data-text-region-id`, `data-start-endpoint`,
`data-end-endpoint`), plus status/confidence attributes where applicable
(`data-symbol-family-status`, `data-wire-color-status`, `data-status`,
`data-*-confidence`, `data-identity-status`). This is exactly the schema
`SvgStructureValidator` resolves references against.

## 10. Unresolved/conflicted rendering (acceptance criterion)

Verified by direct inspection of the TRX300 output and by tests #3-#5,
#11, #15, #18, #21, #25 in `test_structured_svg_exporter.cpp`:

- Unresolved symbol family → `unresolved_symbol_renderer()` glyph,
  `data-symbol-family-status="unresolved"`, **no** `data-symbol-family`
  attribute at all (never a guessed family name).
- Conflicted symbol family → `conflicted_symbol_renderer()` glyph,
  status explicit, never a chosen winning family.
- Unresolved wire color → neutral stroke, `data-wire-color-status=
  "unresolved"`, no `data-wire-color` attribute.
- Conflicted wire color → dashed stroke, status explicit, never one
  chosen color.
- Unresolved `ConnectorTerminal` → no `data-terminal-name` fabricated.
- Unresolved label (no recognized text) → dashed placeholder rectangle,
  region still present with `data-status="unresolved"` — never dropped,
  never invented text.

No code path in `structured_svg_exporter.cpp` writes a `data-symbol-
family`, `data-wire-color`, or `data-terminal-name` attribute except from
inside the `Resolved`-gated branch for that field.

## 11. TRX300 extraction metrics (fresh run, this AP)

```
extracted conductor segments: 294
topology nodes: 692
topology edges: 877
reconstructed wires: 40
electrical nets: 10
endpoint candidates: 210
validation errors: 0
validation warnings: 30
```

Identical to the AP-WIRE-026A baseline — the renderer is read-only over
the model and never touches extraction. Terminal candidates
(109 source, per `.coverage.components.real_with_terminal_evidence` +
`real_without_terminal_evidence` = 20 + 39 = 59 real components' worth of
terminal evidence bookkeeping, consistent with the pre-existing audit
categories) and the 59-component / 10-resolved-family split are unchanged
from AP-WIRE-026A.

## 12. Symbol-family rendering result (the key acceptance criterion)

```
data-symbol-family-status="resolved":   10
data-symbol-family-status="unresolved": 49
data-symbol-family-status="conflicted":  0
```

**Exactly the AP-WIRE-026A baseline, unmodified.** No rule in the
renderer was loosened, and no fallback heuristic was added to inflate
this count, per the explicit instruction not to increase 10/59
artificially.

## 13. Object coverage report (source / rendered / intentionally-non-rendered)

| Category | Source count | Rendered count | Non-rendered | Reason |
|---|---|---|---|---|
| Components | 109 | 59 | 50 | `DiagramFurniture` (legend/title-block/continuity-chart content per AP-WIRE-023) is never rendered as a circuit component |
| Connectors | 0 | 0 | 0 | TRX300 baseline has no resolved `ConnectorCandidate` — a pre-existing extraction-coverage gap, not introduced or masked by this AP |
| Wires | 40 | 40 | 0 | all rendered, endpoint-to-endpoint |
| Splices | 106 | 106 | 0 | all rendered as filled junction markers |
| Crossings | 237 | 237 | 0 | all rendered as unfilled markers, distinct from splices |
| Labels (text regions) | 115 | 115 | 0 | every region renders (as `label` or `annotation`), including unresolved ones as placeholders |
| Electrical nets | 10 | 10 | 0 | rendered as metadata only, per the physical/electrical-layer separation rule |
| Symbol-family resolutions | 59 (1 per non-furniture component) | 59 (10 glyph + 49 placeholder + 0 conflicted-placeholder) | 0 | every resolution renders something, gated on status |
| Terminal candidates referenced by rendered components | 56 | 56 | 0 | 1:1, sum of `terminal_candidate_ids` across all rendered components |
| Connector terminals | 0 | 0 | 0 | none exist in this baseline (no connectors) |

## 14. Determinism results

Two independent extraction runs against `samples/trx300ODG.png` (fresh
temp output directories) produce **byte-identical** `output/wires.svg`
(`diff` reports no differences). `StructuredSvgExporter::render()` is a
pure function of its two const-reference inputs with no timestamps,
random IDs, memory addresses, or unordered-container iteration exposed to
output — confirmed by full read of `structured_svg_exporter.cpp` (all
container iteration is either over `EngineeringDiagram`'s already-sorted
vectors or a single deterministic pass over `WireModel` vectors in their
stored order).

## 15. SVG structural validation results

`SvgStructureValidator::validate()` run against the real TRX300
`output/wires.svg` (built via a small standalone runner linking
`libeke_dx_wire.a` directly against the real `EngineeringDiagram`/
`WireModel` from a fresh extraction):

```
element_id_count=642
issues=0
```

- 642/642 element `id=` values unique (verified independently via a raw
  Python regex pass too, matching exactly).
- Zero `SVG-DANGLING-REFERENCE` issues — every `data-*-id` attribute
  resolves to a real component/connector/wire/node/terminal/endpoint/
  text-region/net/conductor-segment id.
- Zero `SVG-RASTER-FALLBACK` — `grep -c "<image" output/wires.svg` = 0.
- Zero `SVG-UNGUESSED-SYMBOL-FAMILY-VIOLATION`.

## 16. AP-WIRE-024 known-conflict preservation (6th consecutive AP check)

The 4 endpoint-semantic-reconstruction conflicts AP-WIRE-024 introduced
are still present, unchanged, in the fresh extraction:

```
conflicted endpoint reconstructions: 4
  endpoint-candidate-cde07f8718a2b9cd  (component_id: "", status: conflicted)
  endpoint-candidate-b0e3d6bb622a227c  (component_id: "", status: conflicted)
  endpoint-candidate-c033140e251b7b86  (component_id: "", status: conflicted)
  endpoint-candidate-1fd584e37a5c72b5  (component_id: "", status: conflicted)
```

Checked in the SVG: for `endpoint-candidate-b0e3d6bb622a227c`, the wire
touching it renders with `data-wire-color-status="unresolved"` (no
fabricated color), and **both** competing `TerminalCandidate`s for that
endpoint render as separate markers (one referencing
`component-candidate-shape-region-845947b0afd7451a`, the other
`component-candidate-shape-region-c98f6566b0672535`) — the renderer shows
the genuine ambiguity rather than silently picking one association.

## 17. Structural/mutation confirmation

`StructuredSvgExporter::render()`/`export_svg()` take `const
EngineeringDiagram&` and `const WireModel&` — no mutable reference exists
anywhere in the call chain from `ExtractionArtifactWriter::write()`. The
TRX300 structural invariants (§11) are byte-for-byte identical to the
AP-WIRE-026A baseline, confirming the renderer performed no extraction,
topology, or semantic mutation.

## 18. No source-image / OpenCV dependency (acceptance criterion)

```
$ grep -n "opencv\|cv::" src/export/structured_svg_exporter.cpp \
    src/export/symbol_renderer.cpp src/export/svg_structure_validator.cpp \
    include/eke_dx_wire/export/structured_svg_exporter.hpp \
    include/eke_dx_wire/export/symbol_renderer.hpp \
    include/eke_dx_wire/export/svg_structure_validator.hpp
(no output)
```

None of the three new translation units include any OpenCV header or
reference any `cv::` type. Test #33 additionally exercises rendering a
`WireModel` with no associated image path to confirm this at the
behavioral level, not just by grep.

## 19. Reference-image comparison (visual/conceptual, not pixel-matching)

Against `samples/trx300_complete_diagram_view.png` (design-reference
target only, never extraction input, per every prior AP's treatment of
this file):

- **Represented and correct**: distinct wire lines, splice junctions
  (filled) vs. crossings (unfilled), ground symbols, component bounding
  boxes with labels, connector regions — the reference's basic visual
  vocabulary (lines/dots/boxes/text) is present and structurally correct
  in the output.
- **Generic due to unresolved status**: 49/59 components render the
  dashed-outline unresolved placeholder rather than the reference's
  specific symbol glyphs (lamp bulb, switch blade, motor circle-with-M,
  etc.) — this is the correct, honest behavior given AP-WIRE-026A's
  current recognition coverage, not a rendering defect.
- **Unavailable due to missing evidence**: 0 rendered connectors (the
  reference shows several multi-pin connector housings) because the
  TRX300 baseline currently resolves 0 `ConnectorCandidate`s — an
  upstream extraction-coverage gap, not something this renderer can or
  should invent.
- **Deferred to layout**: the reference's clean auto-routed, non-
  overlapping schematic layout is not reproduced — this AP intentionally
  did not add an auto-layout engine (explicitly out of scope per the
  spec unless clearly required), so the output uses raw
  `source_page_pixels` coordinates as extracted.
- No pixel-tracing, image-diffing, or visual-similarity scoring against
  the reference was performed or attempted, consistent with the spec's
  explicit prohibition on treating the reference as ground truth.

## 20. Remaining limitations / known gaps

- 50/109 components are `DiagramFurniture` and never render as circuit
  content — correct per AP-WIRE-023, but means the rendered diagram is
  visually sparser than the full source page.
- 0 connectors render because 0 resolve upstream; the renderer has no
  fallback for this and should not gain one — a future connector-
  recognition improvement is out of scope here.
- No auto-layout: overlapping/dense regions in the source render as
  overlapping/dense regions in the SVG. This was an explicit, spec-
  sanctioned scope decision, not an oversight.
- `SvgStructureValidator` is intentionally lightweight (regex-based, no
  DOM); it validates the specific acceptance criteria the AP-WIRE-027
  spec names, not general XML well-formedness beyond a root/close check.

## 21. Architecture assessment / verdict

`StructuredSvgExporter` sits cleanly downstream of `EngineeringDiagram`
and never re-touches the source image, extraction, topology, or semantic
layers. Every uncertainty the pipeline has already recorded (unresolved
terminal, unresolved wire color, unresolved/conflicted symbol family,
unresolved label text, the AP-WIRE-024 endpoint conflicts) remains
visible and distinguishable in the rendered SVG rather than being
resolved away by rendering-time guesswork. The symbol-family count stays
exactly 10/49/0 as instructed. The renderer is suitable as a foundation
for a future Diagram Studio consumer (structured, provenance-tagged,
group-organized) without any GUI-specific engineering objects having been
introduced. AP-WIRE-027 is complete and validated; **AP-WIRE-028 is not
started**, per the stop condition.

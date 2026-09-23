# AP-GUI-001 — Simple Desktop GUI

Status: IMPLEMENTED
Version: 0.1

## Purpose

Provide a minimal visual instrument for exercising the existing EKE-DX-WIRE
extraction library without duplicating extraction logic.

The GUI is intentionally a thin client over libeke_dx_wire.

## v0.1 capabilities

- Open a raster wiring diagram using the Windows file picker.
- Display the source diagram.
- Run the existing extraction pipeline.
- Overlay detected conductor segments.
- Toggle source, conductor, and topology visibility.
- Display segment/node/edge counts.
- Write the same canonical artifact set used by the CLI.
- Accept an image path as the first command-line argument.
- Exit with Esc or Q.

## Architecture

    dx-extractor-gui
          |
          v
    libeke_dx_wire
          |
          +-- image loading
          +-- normalization
          +-- morphology
          +-- topology
          |
          v
    WireModel

The GUI contains no extraction algorithms. Its diagnostic calibration view may
run early detector stages for visualization, but artifact generation is always
performed by the complete ExtractionPipeline and the canonical
ExtractionArtifactWriter.

## Implementation

The first implementation uses OpenCV HighGUI for the visual surface and the
native Windows file picker for image selection. This avoids introducing a
second GUI framework or additional runtime dependency during extraction-engine
development.

This is a development instrument, not the final OEP/EKE user interface.

## Interaction

Toolbar:
- OPEN — select an image.
- EXTRACT — run the current extraction pipeline.
- SOURCE — toggle source image.
- CONDUCTORS — toggle conductor overlay.
- TOPOLOGY — toggle topology nodes.

Keyboard:
- O — open.
- E — extract.
- Q / Esc — quit.

Topology rendering:
- crossings are orange;
- electrically connective nodes are green.

The visualization is diagnostic only. It does not imply that every detected
node is a true engineering endpoint.

## Future increments

- project loading/saving.
- artifact browser.
- object inspector.
- zoom/pan controls.
- endpoint and wire visualization.
- validation overlays.
- PDF source rendering.
- cross-platform Qt-based shell when the core model stabilizes.

## Canonical artifact boundary

CLI and GUI extraction results use the same project artifact layout:

```text
<project-root>/
├── project.json
├── artifacts/
│   ├── audit/extraction_audit.json
│   ├── topology/topology.json
│   └── recognition/
└── output/wires.svg
```

The GUI resolves the repository/project root from its executable location when
running from the development tree. It does not create a separate `extraction/`
artifact tree.

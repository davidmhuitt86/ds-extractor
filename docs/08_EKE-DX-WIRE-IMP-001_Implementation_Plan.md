# EKE-DX-WIRE-IMP-001
# Implementation Plan

---

## Phase 0 — Repository skeleton

Create:

```text
CMake project
domain library
engine library
CLI application
test framework
fixture structure
```

Deliverable:

```text
dx-extract --help
```

---

## Phase 1 — Image foundation

Implement:

- image loading;
- PDF raster input;
- normalization;
- thresholding;
- diagram-field configuration;
- diagnostic image output.

Deliverables:

```text
normalized.png
threshold.png
```

---

## Phase 2 — Wire masks

Implement:

- horizontal morphology;
- vertical morphology;
- connected components;
- centerline extraction.

Deliverables:

```text
horizontal-mask.png
vertical-mask.png
segments.json
```

Acceptance:

- no systematic double-edge representation.

---

## Phase 3 — Non-wire masking

Implement:

- manual region provider;
- rectangle detector;
- clipping.

TRX300 may initially use a checked-in region configuration.

Deliverables:

```text
non-wire-mask.png
clipped-segments.json
```

---

## Phase 4 — Segment reconstruction

Implement:

- collinear merge;
- duplicate suppression;
- endpoint normalization;
- spatial index.

Deliverable:

```text
merged-segments.json
```

---

## Phase 5 — Topology

Implement:

- node creation;
- endpoint/interior relationships;
- junction classification;
- graph;
- path chaining.

Deliverables:

```text
topology.json
wire-paths.json
```

---

## Phase 6 — Cable classification

Implement:

- local thickness;
- distance transform;
- configurable classification.

Deliverable:

```text
wire-model.json
```

---

## Phase 7 — Validation

Implement:

- geometry validator;
- topology validator;
- residual analysis;
- overlay renderer;
- regression metrics.

Deliverable:

```text
validation-report.json
overlay.png
```

---

## Phase 8 — SVG export

Implement:

- SVG line output;
- SVG path output;
- metadata;
- stable IDs;
- wire/heavy-cable layers.

Deliverable:

```text
wires.svg
```

---

## Phase 9 — Standalone application

Implement:

- project management;
- source preview;
- extraction controls;
- overlay viewer;
- object inspection;
- manual exclusion editing;
- rerun selected stages;
- export.

---

## Phase 10 — Generalization

Replace TRX300-specific assumptions with:

- automatic diagram-field detection;
- automatic non-wire region detection;
- adaptive morphology parameters;
- source-resolution calibration;
- multi-document regression suite.

---

## Phase 11 — EKE integration

Expose the engine as an EKE service/library.

Attach:

```text
wire
 -> terminal
 -> connector
 -> component
 -> electrical relationship
```

without modifying the underlying observed wire geometry.

---

## Recommended first coding target

Do not begin with the GUI.

Build:

```text
libeke_dx_wire
        +
dx-extract CLI
        +
TRX300 golden fixture
```

until the complete wire pipeline is deterministic and validated.

The GUI should consume a proven engine rather than become the place where the extraction algorithm is developed.

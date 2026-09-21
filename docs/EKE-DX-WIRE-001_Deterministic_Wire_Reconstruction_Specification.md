# EKE-DX-WIRE-001
# Deterministic Wiring Diagram Reconstruction Specification

**Document Type:** Engineering Specification  
**System:** Electrical Knowledge Engine (EKE) — Diagram Extractor  
**Subsystem:** Wire Reconstruction Pipeline  
**Status:** Proposed  
**Primary Reference:** 1988 Honda TRX300 FourTrax wiring diagram  
**Revision:** 0.1

---

## 1. Purpose

Define a deterministic computer-vision and geometry pipeline for reconstructing the **wire layer** of a wiring diagram from a raster image or scanned PDF.

The output is not intended to be a pixel trace. It shall be a structured geometric representation in which each conductor is represented as an editable engineering object with explicit geometry and topology.

The wire reconstruction shall be suitable as an intermediate representation for the EKE knowledge graph and for downstream SVG/CAD-like editing.

### Governing principle

> **The uploaded image is not the product. The graph is the product.**

The raster image is evidence. The reconstructed wire graph is the engineering artifact.

---

# 2. Scope

## 2.1 Included

The wire-reconstruction stage shall identify and reconstruct:

- ordinary wires;
- heavy cables;
- straight wire segments;
- orthogonal bends;
- continuous routed conductors;
- wire endpoints;
- wire-to-wire continuity;
- T-junctions;
- multi-wire junctions;
- crossings that are not electrically connected;
- conductor centerlines;
- conductor thickness estimates;
- wire continuity groups;
- topology graph edges and nodes.

## 2.2 Excluded from this stage

The following shall not be semantically reconstructed by the wire extractor:

- component identities;
- connector identities;
- connector pin identities;
- terminal identities;
- labels;
- text;
- annotations;
- switch-state tables;
- component symbols;
- electrical function;
- source/destination semantics;
- wire color semantics unless reliably recoverable from the source;
- inferred circuit meaning.

These may be processed by subsequent EKE extraction stages.

## 2.3 Reference overlay

A raster reference layer may be generated during development and validation.

It shall not be required in the production wire artifact.

---

# 3. Design Principles

## 3.1 Evidence before semantics

The extractor shall reconstruct observable graphical geometry before assigning electrical meaning.

## 3.2 Centerline representation

A printed conductor is normally a finite-width dark region. The extractor shall represent the conductor by its **centerline**, not by its two visible edges.

Incorrect:

```text
edge ───────────────── edge
```

Correct:

```text
        ───────────────
          centerline
```

The resulting SVG shall therefore normally contain a single `<line>` or `<path>` representing the conductor.

## 3.3 Orthogonal geometry

The primary diagram model shall assume horizontal and vertical routing unless non-orthogonal geometry is actually detected.

Diagonal geometry shall not be invented.

## 3.4 Graph before SVG

SVG shall be considered a presentation/editing representation of the reconstructed wire model.

The internal model shall contain geometry and topology independently of SVG.

## 3.5 No automatic electrical assumptions

A crossing shall not become a junction merely because two geometric objects intersect.

A junction shall require observable graphical evidence or a sufficiently explicit geometric relationship.

## 3.6 Deterministic first

The wire pipeline shall be implementable without an AI model.

Machine-learning or AI-assisted interpretation may later be added as a separate confidence/review layer, but the deterministic extractor shall remain independently executable.

---

# 4. Pipeline Architecture

The required processing sequence is:

```text
SOURCE IMAGE / PDF
        |
        v
[1] Source Inspection
        |
        v
[2] Orientation Normalization
        |
        v
[3] Diagram Field Segmentation
        |
        v
[4] Image Preprocessing
        |
        v
[5] Horizontal/Vertical Wire Mask Extraction
        |
        v
[6] Centerline Segment Extraction
        |
        v
[7] Non-Wire Region Detection
        |
        v
[8] Segment Clipping
        |
        v
[9] Collinear Segment Merging
        |
        v
[10] Endpoint Normalization / Snapping
        |
        v
[11] Intersection and Junction Analysis
        |
        v
[12] Topology Graph Construction
        |
        v
[13] Wire Path Chaining
        |
        v
[14] Heavy Cable Classification
        |
        v
[15] Geometry Validation
        |
        v
[16] Overlay Validation
        |
        v
[17] Structured Wire Object Export
        |
        v
SVG / EKE Wire Graph
```

---

# 5. Stage 1 — Source Inspection

Before image processing, determine:

- page count;
- page dimensions;
- image resolution;
- raster versus vector content;
- available embedded images;
- text-layer availability;
- orientation;
- approximate diagram bounds.

For scanned diagrams, rasterize the relevant page at sufficient resolution.

The source inspection process demonstrated in the reference methodology uses PDF diagnostics followed by raster inspection when the diagram is graphical rather than text-centric.

The extractor shall preserve the original source dimensions and maintain a coordinate transform between source pixels and normalized diagram coordinates.

---

# 6. Stage 2 — Orientation Normalization

The source shall be normalized to the intended readable orientation before geometric extraction.

The orientation transform shall be recorded in metadata.

Example:

```json
{
  "source_rotation_degrees": 180
}
```

All subsequent geometry shall operate in normalized coordinates.

---

# 7. Stage 3 — Diagram Field Segmentation

The extractor shall isolate the principal wiring field from:

- page borders;
- margins;
- title blocks;
- legends;
- switch-continuity tables;
- unrelated annotations;
- scan artifacts.

The initial implementation may use configurable bounding regions.

The generalized implementation should derive the wiring field from:

- line density;
- connected geometric structure;
- component/symbol density;
- text density;
- page geometry.

### Important rule

The crop shall remove unrelated regions without cropping actual conductor geometry.

A configurable safety margin shall be applied around the detected diagram field.

---

# 8. Stage 4 — Image Preprocessing

Recommended initial pipeline:

1. grayscale conversion;
2. moderate Gaussian blur;
3. upscaling;
4. adaptive thresholding;
5. binary cleanup.

Reference implementation parameters may begin with:

```text
Upscale factor:       3x
Gaussian blur:        sigma ≈ 1.5
Adaptive threshold:   Gaussian
Block size:           51
Constant:             14
```

These are starting parameters, not immutable constants.

The implementation shall expose them as configuration parameters.

---

# 9. Stage 5 — Orthogonal Wire Mask Extraction

The reference methodology demonstrated that morphological opening with long horizontal and vertical kernels is effective for isolating wiring geometry.

For horizontal extraction:

```text
horizontal kernel = (L, 1)
```

For vertical extraction:

```text
vertical kernel = (1, L)
```

The initial implementation used a minimum line-length parameter around 16–24 source pixels after normalization.

The system shall allow the minimum structural length to be configured.

Two independent masks shall be generated:

```text
HORIZONTAL_WIRE_MASK
VERTICAL_WIRE_MASK
```

The masks may overlap at corners and junctions.

---

# 10. Stage 6 — Centerline Segment Extraction

Connected components shall be extracted from the horizontal and vertical masks.

For each component:

```text
orientation
center coordinate
start coordinate
end coordinate
observed thickness
pixel area
```

shall be calculated.

For a horizontal component:

```text
x_start = component left
x_end   = component right
y       = weighted/mean component centerline
```

For a vertical component:

```text
y_start = component top
y_end   = component bottom
x       = weighted/mean component centerline
```

### Critical requirement

The extractor shall derive **one centerline per physical conductor**.

It shall not use edge detection as the primary wire representation.

Canny/Hough/LSD-style edge detection may be used as supplemental evidence, but must not directly produce the final wire geometry without centerline consolidation.

---

# 11. Stage 7 — Non-Wire Region Detection

This stage prevents component outlines, connector boxes, symbols, and text from being interpreted as wires.

The system shall maintain a `NON_WIRE_MASK`.

Initial implementation may use explicit exclusion regions.

Example:

```text
component/connector bounding region
        |
        v
NON_WIRE_MASK
```

The reference implementation manually established exclusion rectangles for:

- switches;
- rectifier;
- tail light;
- headlight;
- ignition coil;
- fuse box;
- relay;
- battery;
- pulse generator;
- alternator;
- connectors;
- other component bodies.

The generalized EKE implementation shall eventually replace manual exclusion rectangles with automatically detected object regions.

### Automatic non-wire region candidates

Candidate regions may be derived from:

- closed rectangles;
- symbol contours;
- text bounding boxes;
- connector geometry;
- component detector output;
- high-density non-orthogonal geometry;
- enclosed shapes;
- known diagram symbol templates.

Each region shall have a confidence value.

---

# 12. Stage 8 — Segment Clipping

Wire candidates intersecting non-wire regions shall be clipped.

Given:

```text
WIRE_SEGMENT
NON_WIRE_REGION
```

the output shall be:

```text
WIRE_SEGMENT_A
NON-WIRE REGION
WIRE_SEGMENT_B
```

rather than allowing the wire candidate to pass through the component body.

This preserves the fact that the conductor approaches a component/connector without incorrectly interpreting the component's internal graphical geometry as conductor geometry.

Clipping shall not delete the geometric endpoint relationship needed for later terminal reconstruction.

---

# 13. Stage 9 — Collinear Segment Merging

Segments with the same orientation shall be merged when they satisfy configurable tolerances.

Initial reference values:

```text
centerline tolerance: approximately 1.6 source pixels
endpoint gap:         approximately 5 source pixels
```

The merge algorithm shall require:

1. same orientation;
2. centerlines sufficiently close;
3. overlapping or nearly adjacent extents;
4. no intervening non-wire region;
5. no evidence of distinct parallel conductors.

The merged object shall retain provenance metadata identifying the source segments from which it was constructed.

---

# 14. Stage 10 — Endpoint Normalization / Snapping

Wire endpoints shall be normalized against nearby perpendicular wire geometry.

For each endpoint:

```text
endpoint
   |
   +--> search nearby perpendicular segments
   |
   +--> if within tolerance
             |
             v
        snap endpoint
```

Initial reference tolerance:

```text
TOL ≈ 5 source pixels
```

Snapping shall be performed only when a valid perpendicular conductor exists.

Endpoints shall not be arbitrarily moved merely to produce visually cleaner geometry.

The original and snapped coordinates shall be retained where required for auditability.

---

# 15. Stage 11 — Intersection and Junction Analysis

Three primary geometric cases shall be distinguished.

## 15.1 Continuation

```text
──────────────
```

Two collinear segments form one continuous conductor.

## 15.2 Crossing without connection

```text
──────────────
      │
      │
```

or an equivalent crossing geometry where the source provides no junction evidence.

The wires remain separate graph edges.

## 15.3 Junction

```text
──────────────┬────
              │
              │
```

or an equivalent T/multi-way connection.

A junction candidate shall be generated when:

- multiple wire endpoints coincide;
- a wire endpoint touches the interior of another wire;
- the source contains an explicit junction dot;
- geometry otherwise establishes a connection.

The implementation shall distinguish:

```text
observed junction
inferred geometric junction
unresolved intersection
```

---

# 16. Stage 12 — Topology Graph Construction

The wire model shall be represented as a graph.

```text
G = (V, E)
```

Where:

### Vertices

Vertices represent:

- conductor endpoints;
- junctions;
- topology transition points;
- connector/component boundaries when later stages are attached.

### Edges

Edges represent conductor geometry between vertices.

Each edge shall contain:

```json
{
  "id": "wire-edge-001",
  "geometry": "...",
  "orientation": "horizontal",
  "thickness": 1.8,
  "confidence": 0.97
}
```

### Incidence model

The system shall maintain endpoint incidence so that each topology node can determine:

- number of connected segments;
- segment orientations;
- whether another segment passes through the node;
- whether the node is a simple continuation;
- whether the node is a T-junction;
- whether the node is a multi-way junction.

---

# 17. Stage 13 — Wire Path Chaining

Graph edges shall be chained into editable conductor paths.

A chain may contain:

```text
M x1 y1
L x2 y2
L x2 y3
L x4 y3
```

representing one routed conductor.

Consecutive collinear points shall be removed.

Example:

```text
M 10 10
L 50 10
L 100 10
L 150 10
```

shall simplify to:

```text
M 10 10
L 150 10
```

provided no topology node exists between them.

A topology node shall never be removed merely because the geometry is collinear.

---

# 18. Stage 14 — Heavy Cable Classification

Wire thickness shall be measured during centerline extraction.

The system shall classify conductors using observed thickness rather than semantic assumptions.

Initial reference methodology:

```text
normal wire ≈ 2.2
heavy cable >= 4.2
```

These values shall be configurable and calibrated against source resolution.

Heavy cables shall be exported separately:

```xml
<g id="heavy-cables">
```

A conductor shall not be classified as a cable solely because of its electrical role.

The classification is graphical unless later semantic evidence overrides it.

---

# 19. Wire Object Model

Each reconstructed wire shall have a stable identity.

Minimum object:

```json
{
  "id": "wire-001",
  "object_type": "wire",
  "geometry": {
    "type": "orthogonal_path",
    "points": []
  },
  "orientation": "orthogonal",
  "thickness": 2.2,
  "source_evidence": [],
  "continuity_group": "continuity-001",
  "confidence": 0.98,
  "status": "resolved"
}
```

Optional fields:

```json
{
  "source_color": null,
  "source_region": {},
  "endpoint_a": "node-001",
  "endpoint_b": "node-002",
  "crossings": [],
  "junctions": [],
  "provenance": []
}
```

Unknown values shall be represented as unknown/null.

They shall not be fabricated.

---

# 20. SVG Output Requirements

The production SVG shall contain a dedicated wire layer:

```xml
<g id="diagram">
    <g id="wires">
        ...
    </g>

    <g id="heavy-cables">
        ...
    </g>
</g>
```

A normal straight conductor should be represented as:

```xml
<line
    id="wire-001"
    x1="100"
    y1="200"
    x2="300"
    y2="200"
    data-object-type="wire"/>
```

A routed conductor may be:

```xml
<path
    id="wire-002"
    d="M 100 200 L 300 200 L 300 350 L 500 350"
    data-object-type="wire"/>
```

### Prohibited representation

Do not represent a normal wire as:

```xml
<path d="...edge of wire..."/>
<path d="...other edge of wire..."/>
```

The physical stroke itself shall be rendered using SVG stroke width.

---

# 21. Provenance

Every generated wire shall retain enough provenance to permit debugging.

Recommended:

```xml
data-source-segments="17,18"
data-source-region="main-wiring-field"
data-centerline-method="morphological-component-centroid"
data-clipped="true"
data-snapped="true"
data-confidence="0.96"
```

Provenance shall not alter electrical semantics.

---

# 22. Confidence Model

Each wire object shall have a confidence score or categorical confidence state.

Suggested categories:

```text
HIGH
MEDIUM
LOW
UNRESOLVED
```

Confidence should consider:

- line-mask strength;
- component continuity;
- centerline stability;
- segment length;
- thickness consistency;
- endpoint proximity;
- topology consistency;
- exclusion-region uncertainty;
- crossing ambiguity;
- source image quality.

Confidence is an assessment of reconstruction reliability, not electrical certainty.

---

# 23. Validation

The extractor shall perform automated validation before export.

## 23.1 Geometry validation

Check for:

- zero-length segments;
- duplicate segments;
- near-duplicate parallel segments;
- invalid coordinates;
- excessive path complexity;
- unexpected diagonal segments;
- malformed SVG.

## 23.2 Topology validation

Check for:

- disconnected endpoints that should coincide;
- accidental endpoint connections;
- unexplained intersections;
- junctions with impossible geometry;
- duplicate junction nodes;
- orphan wire segments.

## 23.3 Visual validation

Generate a diagnostic overlay:

```text
SOURCE IMAGE
     +
EXTRACTED WIRES
     +
JUNCTION MARKERS
     +
NON-WIRE MASK
```

The overlay shall be inspectable by a human reviewer.

Recommended diagnostic colors:

```text
source              grayscale
horizontal wires    diagnostic color A
vertical wires      diagnostic color B
junctions           diagnostic color C
non-wire regions    translucent mask
unresolved          diagnostic color D
```

Diagnostic colors shall not be written into the production wire SVG unless explicitly requested.

---

# 24. Residual Analysis

After extracting candidate wires, the system shall calculate residual dark pixels:

```text
RESIDUAL = DARK_PIXELS - RECONSTRUCTED_WIRE_MASK
```

The residual image shall be used to identify:

- missed conductors;
- component geometry;
- text;
- symbols;
- scan artifacts.

Residual analysis is a validation mechanism and shall not automatically classify every residual as a missing wire.

---

# 25. Reference Overlay

During development, the extractor shall support:

```text
reference scan
      +
wire reconstruction
      =
visual comparison
```

The reference layer shall be optional and removable.

The production artifact shall contain no raster source image unless explicitly requested.

---

# 26. Generalization Beyond Manual Exclusion Boxes

The TRX300 implementation may use manually defined exclusion regions to establish a reliable baseline.

The production architecture shall provide an abstraction:

```text
NonWireRegionProvider
```

Possible implementations:

```text
ManualRegionProvider
SymbolDetectorRegionProvider
OCRRegionProvider
ConnectorDetectorRegionProvider
ComponentDetectorRegionProvider
CompositeRegionProvider
```

The wire extractor shall consume regions through this interface rather than directly depending on manually coded coordinates.

This permits the TRX300 implementation to evolve into a general diagram converter without rewriting the wire reconstruction algorithm.

---

# 27. Deterministic Algorithm Boundary

The following portions should be implementable without an AI model:

- PDF/image inspection;
- orientation correction;
- diagram-field segmentation;
- grayscale conversion;
- thresholding;
- morphological line extraction;
- connected-component analysis;
- centerline calculation;
- exclusion-mask application;
- segment clipping;
- collinear merging;
- endpoint snapping;
- intersection detection;
- topology graph construction;
- path chaining;
- geometric simplification;
- cable classification;
- SVG generation;
- SVG validation;
- residual analysis.

AI/ML may be introduced later for:

- component identification;
- connector identification;
- label interpretation;
- ambiguous junction interpretation;
- semantic wire-color interpretation;
- difficult exclusion-region detection;
- human-review prioritization.

The deterministic wire geometry shall remain independently usable.

---

# 28. Processing Artifacts

The extractor should optionally emit intermediate artifacts:

```text
01_source_normalized.png
02_diagram_field.png
03_threshold.png
04_horizontal_mask.png
05_vertical_mask.png
06_initial_segments.json
07_non_wire_mask.png
08_clipped_segments.json
09_merged_segments.json
10_topology_graph.json
11_wire_paths.json
12_residual.png
13_validation_overlay.png
14_final_wires.svg
```

This is important for engineering auditability and AAR/debugging.

A failed reconstruction should be diagnosable at the stage where the geometry was lost or incorrectly classified.

---

# 29. Deterministic Test Requirements

A wire extractor implementation shall be tested against fixtures containing:

### Test A — Straight wire

```text
────────────
```

Expected:

```text
1 wire
1 centerline
```

### Test B — Wire bend

```text
──────
      │
      │
```

Expected:

```text
1 wire path
2 orthogonal segments
1 continuity relationship
```

### Test C — Crossing

```text
────────
   │
   │
```

Expected:

```text
2 independent conductors
0 junctions
```

when no source junction evidence exists.

### Test D — T-junction

```text
────────┬────
        │
```

Expected:

```text
junction node
3 incident conductor edges
```

### Test E — Heavy cable

A visibly thicker conductor shall be classified separately from ordinary wires.

### Test F — Component enclosure

A rectangular component outline shall not become a wire.

### Test G — Parallel conductors

Two close parallel wires shall remain independent.

### Test H — Low-quality scan

Broken line pixels shall be recoverable when continuity evidence supports reconstruction, without joining unrelated nearby conductors.

---

# 30. Acceptance Criteria

The wire extraction implementation shall not be considered complete until:

1. Each normal conductor is represented by one centerline.
2. No systematic double-edge wire representation exists.
3. Components are not systematically converted into wires.
4. Parallel wires remain independent.
5. Collinear conductor segments are merged appropriately.
6. Orthogonal bends preserve geometry.
7. Junctions are represented independently from ordinary crossings.
8. Crossings are not automatically treated as electrical connections.
9. Heavy cables are distinguishable from ordinary wires.
10. Every wire has a stable identifier.
11. Topology can be reconstructed independently of SVG rendering.
12. The SVG contains editable vector geometry only.
13. The source raster is absent from the production artifact unless explicitly requested.
14. Validation overlays can demonstrate geometric correspondence to the source.
15. Intermediate artifacts permit reconstruction errors to be traced to a specific processing stage.

---

# 31. Initial TRX300 Implementation Strategy

The first implementation shall intentionally optimize for correctness on the 1988 Honda TRX300 reference diagram.

Phase 1:

```text
manual diagram-field bounds
+
manual component exclusion regions
+
deterministic morphology
+
centerline extraction
+
topology graph
```

Phase 2:

```text
automatic component-region detection
+
automatic text exclusion
+
automatic diagram-field detection
```

Phase 3:

```text
multi-document calibration
+
vehicle-independent extraction rules
+
confidence-driven human review
```

Phase 4:

```text
EKE knowledge graph integration
+
semantic component/connector attachment
+
electrical validation
```

The TRX300 should therefore be treated as the **calibration fixture and reference dataset**, not as a collection of one-off coordinate hacks.

---

# 32. Relationship to EKE Knowledge Graph

The wire extractor shall produce a graph-ready representation.

Conceptually:

```text
Image Evidence
      |
      v
Geometric Wire Object
      |
      v
Topology Node / Edge Graph
      |
      v
Connector / Terminal Attachment
      |
      v
Component Attachment
      |
      v
Electrical Relationship Graph
```

The wire extractor must stop at the boundary between **observable conductor geometry** and **semantic electrical interpretation**.

This boundary is deliberate.

It allows later EKE stages to replace or revise semantic interpretations without corrupting the underlying observed geometry.

---

# 33. Summary

The required wire-conversion architecture is:

```text
                 RASTER SOURCE
                       |
                       v
              NORMALIZE / CROP
                       |
                       v
              ADAPTIVE THRESHOLD
                       |
          +------------+------------+
          |                         |
          v                         v
    HORIZONTAL MASK          VERTICAL MASK
          |                         |
          +------------+------------+
                       |
                       v
              CENTERLINE SEGMENTS
                       |
                       v
                NON-WIRE MASK
                       |
                       v
                  CLIPPING
                       |
                       v
               COLLINEAR MERGE
                       |
                       v
                 ENDPOINT SNAP
                       |
                       v
              INTERSECTION ANALYSIS
                       |
                       v
                TOPOLOGY GRAPH
                       |
                       v
                 PATH CHAINING
                       |
             +---------+---------+
             |                   |
             v                   v
        NORMAL WIRES        HEAVY CABLES
             |                   |
             +---------+---------+
                       |
                       v
                  VALIDATION
                       |
             +---------+---------+
             |                   |
             v                   v
       OVERLAY REVIEW       RESIDUAL ANALYSIS
             |                   |
             +---------+---------+
                       |
                       v
             STRUCTURED WIRE MODEL
                       |
                       v
                  SVG EXPORT
```

The central engineering change from the earlier extraction attempts is:

> **Detect the physical wire region, derive its centerline, reconstruct its topology, and only then generate SVG geometry.**

This avoids the double-edge problem, makes the output graph-oriented, and establishes a deterministic foundation that can later be generalized beyond the TRX300.

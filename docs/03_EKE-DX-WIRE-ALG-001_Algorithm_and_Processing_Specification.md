# EKE-DX-WIRE-ALG-001
# Algorithm & Processing Specification

---

## 1. Objective

Define the concrete deterministic algorithms used by the wire reconstruction engine.

---

## 2. Image normalization

Inputs:

```text
Raster image
```

Outputs:

```text
Normalized image
Coordinate transform
```

Operations:

1. determine orientation;
2. rotate;
3. crop/segment diagram field;
4. normalize coordinate space.

The transform must be retained.

---

## 3. Thresholding

Create a binary dark-pixel representation.

Recommended initial algorithm:

```text
grayscale
 -> Gaussian blur
 -> adaptive Gaussian threshold
```

Alternative thresholding algorithms may be selected based on source quality.

---

## 4. Morphological extraction

Create:

```text
H = morphology_open(binary, horizontal_kernel)
V = morphology_open(binary, vertical_kernel)
```

The kernel length shall exceed typical character stroke length while remaining below expected conductor length.

The engine should automatically estimate a candidate kernel length from:

- image resolution;
- median long-line length;
- text stroke distribution.

---

## 5. Connected components

For each mask:

```text
connectedComponents(H)
connectedComponents(V)
```

Each component becomes a candidate segment.

Discard candidates below minimum geometric length.

---

## 6. Centerline calculation

For horizontal components:

```text
x1 = min X
x2 = max X
y  = weighted median/mean Y
```

For vertical:

```text
y1 = min Y
y2 = max Y
x  = weighted median/mean X
```

Weighted centerline calculation should be based on dark-pixel mass rather than blindly selecting a bounding-box midpoint when the stroke is asymmetric.

---

## 7. Edge-pair rejection

The engine must prevent the original double-edge failure.

If an edge detector is used for supplemental evidence, parallel detections shall be clustered before object creation.

Two parallel detections are candidate edges of one conductor when:

```text
same orientation
distance within configured stroke-width range
longitudinal overlap >= configured threshold
similar intensity/geometry
```

The resulting centerline is the midpoint.

The final object count shall represent physical conductors, not visible boundaries.

---

## 8. Non-wire region detection

Initial deterministic providers:

### Manual region provider

Used for calibration fixture development.

### Rectangle detector

Detect closed rectangular geometry.

### Text detector

Use OCR or connected-component characteristics to create exclusion regions.

### Symbol detector

Use shape templates or later object-classification services.

### Composite provider

Union all region providers.

---

## 9. Segment clipping

For each wire candidate:

```text
candidate - nonWireMask
```

Split the candidate at mask intersections.

Preserve the pre-clipping source reference.

---

## 10. Collinear merging

Two segments may merge when:

```text
orientation equal
abs(centerline_distance) <= tolerance
gap <= endpoint_tolerance
no non-wire region between
no topology node requiring separation
```

Do not merge parallel conductors.

Do not merge across a junction merely because they are collinear if the junction must remain a graph node.

---

## 11. Endpoint snapping

For every endpoint:

1. query spatial index;
2. locate nearby perpendicular segments;
3. calculate projection;
4. determine whether projection lies within the segment;
5. if distance <= tolerance, create a candidate connection;
6. preserve the original coordinate in provenance;
7. snap only after topology validation.

A spatial index should be used rather than an O(n²) full comparison once the implementation scales beyond small diagrams.

---

## 12. Junction classification

### Endpoint-endpoint

Likely direct continuity/junction depending on degree.

### Endpoint-interior

Potential T-junction.

### Interior-interior

Potential crossing.

Interior-interior crossings require additional evidence before being treated as connected.

Evidence sources:

- junction dot;
- stroke continuity pattern;
- topology context;
- source symbol conventions;
- later semantic review.

---

## 13. Topology graph

Build:

```text
Node = normalized geometric topology point
Edge = conductor segment/path
```

Node degree provides an initial structural classification:

```text
degree 1 -> endpoint
degree 2 -> continuation/bend
degree 3 -> junction
degree >3 -> multi-way junction
```

Degree alone shall not determine electrical semantics.

---

## 14. Path chaining

Walk the graph from:

- degree-1 nodes;
- junction nodes;
- branch points.

Stop at:

- another topology node;
- branch point;
- unresolved intersection;
- component boundary.

Simplify consecutive collinear points.

---

## 15. Heavy cable classification

Calculate effective stroke width from:

- morphology component width;
- distance transform;
- local perpendicular thickness.

Classify using calibrated thresholds.

Store measured width, not merely the class.

---

## 16. Residual analysis

After wire reconstruction:

```text
residual = source_dark_pixels - reconstructed_wire_pixels
```

Residual clusters are categorized as candidate:

- components;
- connectors;
- labels;
- symbols;
- missed wires;
- artifacts.

Residual analysis is diagnostic first.

---

## 17. Validation metrics

Useful geometric metrics:

### Coverage

```text
wire_source_overlap / expected_wire_area
```

### False extraction

```text
reconstructed_wire_area outside expected wire regions
```

### Centerline error

Distance between reconstruction and source stroke center.

### Endpoint error

Distance between reconstructed endpoint and visually supported source endpoint.

### Topology error

Difference between expected and reconstructed node/edge relationships.

---

## 18. Performance architecture

Use spatial indexing for:

- endpoint search;
- segment intersection;
- region lookup.

Candidate implementation:

```text
R-tree / uniform grid / spatial hash
```

For the first implementation, a uniform spatial grid is sufficient and deterministic.

Parallelize independent image-mask operations where beneficial.

Do not parallelize graph mutation without explicit ownership rules.

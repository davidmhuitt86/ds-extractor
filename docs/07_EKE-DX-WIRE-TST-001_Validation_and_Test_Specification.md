# EKE-DX-WIRE-TST-001
# Validation & Test Specification

---

## 1. Purpose

Ensure the extraction engine produces geometrically and topologically reliable wire reconstructions.

---

## 2. Test levels

```text
Unit
Integration
Fixture
Regression
Visual
Performance
Schema
```

---

## 3. Unit tests

Test:

- point distance;
- collinearity;
- segment intersection;
- projection;
- clipping;
- snapping;
- path simplification;
- duplicate detection;
- centerline calculation;
- thickness calculation.

---

## 4. Topology tests

Fixtures shall include:

```text
straight
bend
T-junction
4-way junction
crossing
parallel wires
endpoint/interior hit
near miss
```

Expected graph structures shall be explicitly encoded.

---

## 5. Regression fixture

The 1988 Honda TRX300 diagram shall become the initial golden fixture.

Store:

```text
source image
normalized image
wire mask
expected wire objects
expected topology
expected validation tolerances
```

The fixture must not be treated as a hard-coded algorithm.

---

## 6. Quantitative metrics

### Geometry

- centerline deviation;
- endpoint deviation;
- path Hausdorff distance;
- source-wire coverage;
- false wire area.

### Topology

- node count deviation;
- edge count deviation;
- missed junctions;
- false junctions;
- incorrect connectivity.

### Stability

Repeated extraction with identical inputs/configuration must produce identical serialized output.

---

## 7. Determinism test

Run:

```text
same input
same configuration
N repetitions
```

Require:

```text
identical object IDs
identical coordinates
identical topology
identical serialization
```

except for explicitly nondeterministic diagnostic metadata such as timestamps.

---

## 8. Visual regression

Generate a standardized overlay.

Store reference render.

Compare:

```text
reference overlay
current overlay
difference image
```

A tolerance shall be defined rather than relying solely on pixel-perfect comparison.

---

## 9. Human review criteria

A reviewer shall inspect:

- missed conductors;
- false conductors;
- double-edge artifacts;
- incorrect bends;
- false junctions;
- missed junctions;
- component outlines interpreted as wire;
- incorrect cable classification.

---

## 10. Acceptance gates

No release candidate should pass unless:

1. no systematic double-edge extraction exists;
2. no systematic component-outline extraction exists;
3. deterministic replay passes;
4. topology fixture tests pass;
5. SVG schema validation passes;
6. TRX300 visual regression remains within tolerance;
7. all low-confidence objects are explicitly marked.


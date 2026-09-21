# SDD-001 — EKE-DX-WIRE Software Design Description

## 1. Objective

Define the coding-level architecture for a standalone deterministic electrical
diagram wire reconstruction engine.

The implementation must preserve the architectural separation established by
the extraction specification:

```text
Source Evidence
      |
      v
Image / Page Representation
      |
      v
Wire Detection
      |
      v
Geometric Reconstruction
      |
      v
Non-Wire Exclusion / Clipping
      |
      v
Endpoint Normalization
      |
      v
Topology
      |
      v
Wire Paths
      |
      v
Validation
      |
      v
Export
```

## 2. Dependency direction

```text
app
 |
 v
pipeline
 |
 +--> image
 +--> geometry/domain
 +--> topology
 +--> validation
 +--> export
 |
 v
infrastructure
```

Domain objects must not depend on OpenCV.

OpenCV is an infrastructure/algorithm dependency used by image-processing
stages.

## 3. Domain model

The domain model contains:

- `Point2D`
- `Segment2D`
- `BoundingBox`
- `WireSegment`
- `WirePath`
- `TopologyNode`
- `TopologyEdge`
- `WireModel`
- provenance and confidence types.

The domain model is deliberately independent of SVG.

## 4. Stage contracts

### Stage A — Source inspection

Input:
- source file

Output:
- source metadata
- image/page representation

### Stage B — Normalization

Input:
- source image

Output:
- grayscale normalized image

Requirements:
- deterministic
- no geometry interpretation

### Stage C — Morphological detection

Input:
- normalized image

Output:
- binary mask
- horizontal mask
- vertical mask
- candidate centerline segments

The primary detector is morphological extraction, not Canny/Hough edge tracing.

### Stage D — Region exclusion

Input:
- candidate segments
- non-wire regions

Output:
- clipped segments

Regions are first-class data because component and connector geometry can
otherwise create false conductor continuity.

### Stage E — Geometric normalization

Input:
- clipped segments

Output:
- merged collinear segments
- normalized endpoints

### Stage F — Topology

Input:
- normalized segments

Output:
- topology graph

The topology layer distinguishes:

- endpoint
- continuation
- junction
- crossing
- component boundary
- unresolved.

A geometric intersection is not automatically an electrical junction.

### Stage G — Path reconstruction

Input:
- topology graph

Output:
- editable wire paths

### Stage H — Validation

Input:
- all stage artifacts

Output:
- deterministic metrics
- unresolved/ambiguous items
- overlay-ready diagnostics

### Stage I — Export

Input:
- validated model

Output:
- SVG
- JSON/IR
- future OEP package

## 5. Stable identity

Object IDs must be deterministic from canonical object content.

The current bootstrap implementation uses a dependency-free deterministic hash.
The production implementation shall replace it with the content-addressing
scheme specified by the project architecture without changing domain APIs.

## 6. Error model

Stages should distinguish:

- invalid source
- unsupported source format
- malformed configuration
- processing failure
- insufficient evidence
- unresolved ambiguity

"Insufficient evidence" must not be converted into a fabricated geometry.

## 7. Determinism

For identical:

- source bytes
- configuration
- engine version

the extraction must produce identical geometry and stable identifiers, excluding
explicitly permitted diagnostic timestamps.

## 8. Threading

The first implementation is single-threaded.

Parallelism may be introduced at stage boundaries only when it preserves:

- deterministic ordering
- deterministic IDs
- deterministic serialization.

## 9. Current implementation boundary

v0.1.0 intentionally stops after morphological candidate generation.

This is not a failure of the architecture. It is the first executable vertical slice
that proves the domain/image/pipeline/export boundaries before topology logic is added.

# EKE-DX-WIRE Implementation Roadmap

## AP-01 — Executable foundation

Status: baseline

- C++23
- CMake
- OpenCV
- domain model
- deterministic IDs
- normalization
- morphology detector
- SVG candidate export
- CLI

## AP-02 — Non-wire region model

Implement:

- `NonWireRegion`
- region providers
- manual rectangle provider
- region intersection tests
- segment clipping

Initial calibration can use explicit region files.

## AP-03 — Geometry normalization

Implement:

- endpoint clustering
- collinear merge
- overlap merge
- gap tolerance
- orientation normalization
- canonical segment ordering

## AP-04 — Topology graph

Implement:

- spatial index
- endpoint-to-endpoint continuity
- endpoint-to-interior T-junction
- crossing detection
- crossing/non-junction classification
- component-boundary nodes
- unresolved intersections

## AP-05 — Wire path reconstruction

Implement:

- graph traversal
- path chaining
- collinear simplification
- branch preservation
- stable path IDs

## AP-06 — Heavy cable classification

Implement:

- measured thickness
- configurable thresholds
- confidence
- normal-wire/cable distinction

## AP-07 — Validation

Implement:

- source overlay
- missing-wire diagnostics
- false-positive diagnostics
- topology consistency checks
- deterministic regression metrics

## AP-08 — Project/package format

Implement:

```text
project.json
source/
config/
artifacts/
output/
review/
```

Add schema validation.

## AP-09 — TRX300 golden fixture

The fixture should contain:

- source image/PDF page
- configuration
- exclusion regions
- expected wire inventory
- expected topology invariants
- expected visual overlay tolerance
- expected stable IDs where appropriate

TRX300 is a calibration fixture, not a hard-coded architecture dependency.

## AP-10 — Review GUI

The GUI should consume the same core library.

Required review functions:

- source/wire overlay
- mask visibility
- object inspection
- topology inspection
- exclusion-region editing
- false-positive marking
- missed-wire marking
- rerun selected stage
- validation report

The GUI must not duplicate extraction algorithms.

# EKE-DX-WIRE-SW-001
# Software Architecture & Module Specification

**Proposed implementation:** C++23 + CMake  
**Primary computer-vision dependency:** OpenCV  
**Output:** Structured SVG + machine-readable intermediate representation

---

## 1. Repository layout

```text
diagram_extractor/
├── CMakeLists.txt
├── cmake/
├── include/
│   └── eke/
│       └── dx/
│           └── wire/
│               ├── api/
│               ├── domain/
│               ├── geometry/
│               ├── image/
│               ├── pipeline/
│               ├── topology/
│               ├── validation/
│               └── export/
├── src/
│   └── ...
├── apps/
│   └── dx-wire/
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── regression/
│   └── fixtures/
├── schemas/
├── configs/
├── docs/
└── tools/
```

---

## 2. Domain modules

### `domain`

Pure data types with minimal dependencies.

Examples:

```cpp
struct Point;
struct Rect;
struct Polyline;
struct BoundingBox;

struct Wire;
struct Cable;
struct Junction;
struct TopologyNode;
struct TopologyEdge;
struct NonWireRegion;

struct Provenance;
struct Confidence;
struct ValidationIssue;
```

Domain types should not depend on OpenCV.

---

## 3. Image module

Responsible for:

- raster loading;
- image normalization;
- thresholding;
- morphology;
- connected-component extraction.

Example interfaces:

```cpp
class IImageSource;
class ImageNormalizer;
class ThresholdProcessor;
class MorphologyProcessor;
```

OpenCV types should remain inside implementation boundaries where practical.

---

## 4. Geometry module

Responsible for:

- line fitting;
- centerline calculation;
- collinearity;
- intersection;
- snapping;
- clipping;
- simplification;
- distance calculations.

Example:

```cpp
class SegmentMerger;
class EndpointSnapper;
class SegmentClipper;
class PolylineSimplifier;
class IntersectionDetector;
```

---

## 5. Topology module

Responsible for converting geometry into a graph.

```cpp
class TopologyBuilder;
class JunctionDetector;
class PathChainer;
class GraphSimplifier;
```

The topology module must not perform OCR or component recognition.

---

## 6. Region module

Provides non-wire regions.

```cpp
class NonWireRegionProvider;
class ManualRegionProvider;
class RectangleRegionDetector;
class TextRegionProvider;
class SymbolRegionProvider;
class CompositeRegionProvider;
```

All region providers implement the same interface.

This allows the initial TRX300 implementation to use manually configured regions while later implementations become automatic.

---

## 7. Pipeline module

The pipeline module coordinates processing.

```cpp
class ExtractionPipeline;
class PipelineStage;
class PipelineContext;
class PipelineConfiguration;
class ArtifactStore;
```

Each stage follows:

```cpp
StageResult execute(PipelineContext&);
```

Stages shall be independently executable for debugging.

---

## 8. Validation module

Responsibilities:

- geometry validation;
- topology validation;
- duplicate detection;
- orphan detection;
- malformed-object detection;
- regression metrics;
- source-overlay comparison.

```cpp
class GeometryValidator;
class TopologyValidator;
class ReconstructionValidator;
class RegressionEvaluator;
```

---

## 9. Export module

Initial outputs:

```text
SVG
JSON
diagnostic images
validation report
```

Later:

```text
OEP package
CAD formats
graph exchange formats
```

SVG serialization should consume the domain model rather than reconstructing geometry from image data.

---

## 10. Configuration

Configuration shall be externalized.

Example:

```yaml
source:
  rotation: auto
  dpi: 300

segmentation:
  field_margin: 0.05

wire:
  minimum_length: 18
  orientation_tolerance_degrees: 3
  centerline_tolerance: 2
  endpoint_snap_tolerance: 5

cable:
  thickness_threshold: 4.2

validation:
  duplicate_tolerance: 2
```

No algorithm should require recompilation merely to change calibration parameters.

---

## 11. Dependency rules

```text
domain
  ↑
geometry
  ↑
image/topology/validation
  ↑
pipeline
  ↑
api
  ↑
applications
```

Domain code shall not depend on application code.

The UI shall not be a dependency of the extraction engine.

---

## 12. Error handling

The engine shall distinguish:

```text
fatal error
recoverable stage error
low-confidence reconstruction
unresolved object
validation warning
```

A low-confidence wire is not an exception.

It is valid engineering data with reduced confidence.

---

## 13. Logging

Structured logging shall record:

- job ID;
- stage;
- configuration;
- object counts;
- warnings;
- timing;
- failures;
- artifact locations.

Example:

```text
JOB 0042
STAGE wire.centerline
INPUT components=812
OUTPUT segments=428
WARNING parallel_candidates=12
```

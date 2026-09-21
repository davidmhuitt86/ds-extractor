# EKE-DX-WIRE-ARCH-001
# Extraction Engine System Architecture

**Status:** Proposed Architecture  
**Target:** EKE Diagram Extraction Engine  
**Initial domain:** Electrical wiring diagrams

---

## 1. System purpose

The Extraction Engine converts diagram imagery into a structured engineering representation.

It shall not treat SVG as the primary data model and shall not treat raster tracing as the final operation.

The system is divided into:

```text
SOURCE
  |
  v
INGESTION
  |
  v
NORMALIZATION
  |
  v
PERCEPTION
  |
  v
GEOMETRY RECONSTRUCTION
  |
  v
TOPOLOGY RECONSTRUCTION
  |
  v
OBJECT RECONSTRUCTION
  |
  v
SEMANTIC ENRICHMENT
  |
  v
VALIDATION
  |
  v
EXPORT
```

The first production vertical slice should implement the wire layer completely before expanding into components, connectors, terminals, and text.

---

## 2. System boundaries

### Inside the engine

- source image loading;
- PDF page rasterization;
- image normalization;
- diagram-field segmentation;
- wire-mask extraction;
- centerline extraction;
- non-wire masking;
- clipping;
- segment merging;
- endpoint normalization;
- topology graph construction;
- path chaining;
- cable classification;
- confidence/provenance;
- validation;
- intermediate artifact generation;
- SVG export.

### Later engine stages

- component detection;
- connector detection;
- terminal detection;
- text extraction;
- symbol classification;
- semantic electrical relationships;
- circuit validation.

### Outside the core

- user interface;
- project management;
- persistent asset library;
- cloud synchronization;
- AI services;
- external CAD integration.

These may consume the engine through a stable API.

---

## 3. Layered architecture

```text
+------------------------------------------------------------+
|                    Application Layer                       |
| CLI | Standalone GUI | EKE Integration | Batch Processing |
+------------------------------------------------------------+
|                     Orchestration Layer                    |
| Job Manager | Pipeline Controller | Configuration | Logs  |
+------------------------------------------------------------+
|                    Extraction Services                     |
| Ingest | Normalize | Segment | Reconstruct | Topology     |
| Objects | Semantic | Validate | Export                    |
+------------------------------------------------------------+
|                    Domain Model Layer                       |
| Geometry | Wire | Node | Graph | Region | Provenance      |
+------------------------------------------------------------+
|                     Algorithm Layer                        |
| Threshold | Morphology | Components | Clipping | Graph   |
| Simplification | Classification | Validation              |
+------------------------------------------------------------+
|                     Infrastructure                         |
| OpenCV | PDF Rasterizer | Filesystem | XML/SVG | Threads |
+------------------------------------------------------------+
```

---

## 4. Pipeline controller

The pipeline controller owns execution order but does not implement image algorithms.

Example:

```cpp
PipelineResult run(const ExtractionRequest& request);
```

Conceptual execution:

```text
LoadSource
 -> NormalizeSource
 -> DetectDiagramField
 -> BuildMasks
 -> ExtractSegments
 -> DetectNonWireRegions
 -> ClipSegments
 -> MergeSegments
 -> NormalizeEndpoints
 -> BuildTopology
 -> ChainPaths
 -> ClassifyCables
 -> Validate
 -> Export
```

Every stage consumes an explicit input model and produces an explicit output model.

---

## 5. Intermediate artifacts

Each major stage shall have a serializable result.

```text
SourceArtifact
NormalizedImage
DiagramField
MaskSet
RawSegmentSet
RegionSet
ClippedSegmentSet
MergedSegmentSet
TopologyGraph
WirePathSet
ValidationReport
ExportArtifact
```

This permits:

- deterministic replay;
- debugging;
- AAR analysis;
- stage-by-stage inspection;
- caching;
- regression testing.

---

## 6. Deterministic/AI boundary

The deterministic core shall not require an AI model.

AI can later be connected through explicit interfaces:

```text
IObjectClassifier
IRegionClassifier
ITextInterpreter
IAmbiguityResolver
```

AI results shall be advisory unless explicitly promoted into the engineering model by a validation/review process.

The deterministic geometry remains the underlying evidence.

---

## 7. Confidence architecture

Confidence shall exist at the object and relationship level.

```text
GeometryConfidence
TopologyConfidence
SemanticConfidence
```

These are independent.

Example:

```text
Wire geometry: HIGH
Topology: HIGH
Wire color: UNKNOWN
Terminal identity: NOT_PROCESSED
```

This prevents uncertainty in one attribute from contaminating otherwise reliable geometry.

---

## 8. Provenance

Every derived object should retain:

```text
source page
source region
processing stage
source primitives
transform history
parameters
confidence
```

A wire should be traceable backward:

```text
wire-001
  -> path-12
  -> merged segments 17, 18
  -> mask components 31, 42
  -> source image region
```

---

## 9. Standalone versus EKE deployment

The engine shall be packaged as a reusable library.

```text
libeke_dx_wire
```

Standalone application:

```text
dx-wire
```

EKE integration:

```text
EKE
 |
 +-- Diagram Extraction Service
       |
       +-- libeke_dx_wire
```

The standalone application must not fork or duplicate the extraction algorithms.

---

## 10. Extensibility

The architecture shall support additional extraction domains:

```text
WireExtractor
ComponentExtractor
ConnectorExtractor
TerminalExtractor
TextExtractor
SymbolExtractor
DimensionExtractor
AnnotationExtractor
```

All should operate against shared geometric/domain primitives.

---

## 11. Core architectural rule

The extraction engine shall never require the UI to understand the image-processing algorithms.

The UI receives:

```text
project
job
intermediate artifacts
objects
graph
validation results
```

The engine receives:

```text
source
configuration
pipeline request
```

This keeps the extraction engine reusable and testable.

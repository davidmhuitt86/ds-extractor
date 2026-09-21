# EKE-DX-WIRE-API-001
# CLI & Programmatic API Specification

---

## 1. API principle

The API shall expose the pipeline without exposing implementation details.

---

## 2. Core C++ API

Proposed:

```cpp
struct ExtractionRequest {
    SourceDocument source;
    ExtractionConfiguration configuration;
};

struct ExtractionResult {
    ProjectId project;
    WireModel wires;
    ValidationReport validation;
    ArtifactManifest artifacts;
};

class ExtractionEngine {
public:
    ExtractionResult extract(const ExtractionRequest&);
};
```

---

## 3. Stage execution

For debugging:

```cpp
engine.run_stage(Stage::Normalize);
engine.run_stage(Stage::WireMasks);
engine.run_stage(Stage::Segments);
engine.run_stage(Stage::Topology);
```

The normal production interface should still provide a complete pipeline call.

---

## 4. CLI

### Inspect

```text
dx-extract inspect source.pdf
```

Outputs:

- page count;
- resolution;
- orientation;
- raster/vector status;
- estimated diagram bounds.

### Extract wires

```text
dx-extract extract source.pdf \
  --stage wires \
  --config extraction.yaml \
  --output project/
```

### Validate

```text
dx-extract validate project/
```

### Overlay

```text
dx-extract overlay project/
```

### Export SVG

```text
dx-extract export project/ \
  --format svg \
  --output wires.svg
```

### Batch

```text
dx-extract batch manifest.json
```

---

## 5. Machine-readable API

JSON is the initial interchange format.

Example:

```json
{
  "schema": "eke.dx.wire.1",
  "source": {
    "file": "diagram.pdf",
    "page": 1
  },
  "objects": {
    "wires": [],
    "nodes": [],
    "edges": []
  }
}
```

---

## 6. Configuration

Configuration shall be external.

CLI overrides should be supported:

```text
--wire.minimum-length 20
--wire.snap-tolerance 5
```

The effective configuration shall be recorded in the result provenance.

---

## 7. Exit codes

```text
0  success
1  validation failure
2  input failure
3  configuration failure
4  processing failure
5  export failure
6  unresolved/interactive review required
```

---

## 8. API stability

The public API shall be versioned.

Internal algorithm implementations may change without changing the domain schema unless the schema itself changes.


# EKE-DX-WIRE-APP-001
# Standalone Diagram Extraction Application Architecture

---

## 1. Purpose

Define a standalone application that uses the extraction engine without requiring the full EKE platform.

Working name:

```text
EKE Diagram Extractor
```

Potential executable:

```text
dx-extract
```

or, for a future GUI:

```text
EKE Diagram Extractor
```

---

## 2. Product boundary

The standalone application is a host for the extraction engine.

It shall not contain a second implementation of extraction algorithms.

```text
+------------------------------------------------+
|              Standalone Application            |
|                                                |
|  Project UI / CLI / Preview / Review / Export  |
|                    |                           |
|                    v                           |
|             Extraction Engine                 |
|                    |                           |
|                    v                           |
|             Domain Model / Graph              |
+------------------------------------------------+
```

---

## 3. Initial application modes

### CLI mode

Primary development interface.

Example:

```text
dx-extract inspect diagram.pdf
dx-extract extract diagram.pdf --stage wires
dx-extract validate project.dxproj
dx-extract export project.dxproj --format svg
dx-extract overlay project.dxproj
```

### GUI mode

Later.

Primary views:

```text
Project
Source
Pipeline
Wire Layer
Topology
Diagnostics
Validation
Export
```

---

## 4. Project format

Proposed project:

```text
diagram-project/
├── project.json
├── source/
│   └── source.pdf
├── config/
│   └── extraction.yaml
├── artifacts/
│   ├── normalized/
│   ├── masks/
│   ├── segments/
│   ├── topology/
│   └── validation/
├── output/
│   └── diagram.svg
└── review/
```

The project should later be packable into `.oep`.

---

## 5. GUI architecture

If a native GUI is implemented later:

```text
Presentation
    |
Application/ViewModel
    |
Engine API
    |
Extraction Library
```

The GUI should never manipulate OpenCV objects directly.

---

## 6. Interactive review

The application should allow the user to:

- toggle source/reference;
- toggle extracted wires;
- toggle non-wire regions;
- inspect wire IDs;
- inspect continuity groups;
- inspect topology nodes;
- mark false positives;
- mark missed wires;
- modify exclusion regions;
- rerun selected pipeline stages;
- compare before/after extraction.

Review modifications should be stored as explicit project overrides rather than hidden algorithm changes.

---

## 7. Human-in-the-loop model

The application should distinguish:

```text
AUTOMATIC
REVIEWED
OVERRIDDEN
UNRESOLVED
```

A manual correction should not silently modify the deterministic algorithm.

Instead:

```text
base reconstruction
        +
review override
        =
reviewed reconstruction
```

---

## 8. Standalone/EKE compatibility

The standalone project should be importable into EKE.

The same:

- schema;
- object IDs;
- provenance;
- configuration;
- validation results

should remain usable.

This is important because the standalone app should become a development and authoring tool for the eventual EKE Diagram Studio rather than a disposable prototype.

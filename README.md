# EKE-DX-WIRE — Deterministic Wire Reconstruction Engine

Version: 0.1.0  
Language: C++23  
Build: CMake 3.24+  
Primary CV dependency: OpenCV 4.x

## Purpose

EKE-DX-WIRE reconstructs electrical diagram conductors as structured engineering
objects. The image is evidence; the reconstructed geometry and topology are the
engineering artifact.

This repository is the implementation baseline derived from:

- EKE-DX-WIRE-001 — Deterministic Wire Reconstruction Specification
- EKE-DX-WIRE-ARCH-001 — System Architecture
- EKE-DX-WIRE-SW-001 — Software Architecture and Modules
- EKE-DX-WIRE-ALG-001 — Algorithm and Processing Specification
- EKE-DX-WIRE-MDL-001 — Domain Model and Intermediate Representation

## Current baseline

Implemented in 0.1.0:

- C++23/CMake project structure.
- `dx-extract inspect` command.
- `dx-extract extract` command.
- deterministic image normalization.
- horizontal/vertical morphological wire-mask extraction.
- connected-component segment candidates.
- centerline segment generation for orthogonal candidates.
- SVG export of reconstructed candidate centerlines.
- artifact directory layout.
- deterministic serialization of the core wire model.
- unit tests for geometry and deterministic ID generation.

Not yet complete:

- automatic non-wire region detection.
- robust clipping against component/connector regions.
- endpoint snapping.
- junction/crossing classification.
- full topology graph construction.
- routed polyline chaining.
- heavy-cable classifier.
- GUI review application.
- PDF-native page/field analysis.
- final EKE/OEP package export.

The architecture intentionally keeps these as separate stages rather than hiding
them inside one monolithic "converter".

## Build

Requirements:

- C++23 compiler.
- CMake 3.24 or newer.
- OpenCV 4.x.

Example:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure
```

## CLI

```text
dx-extract inspect <image>
dx-extract extract <image> --output <project-directory>
```

Example:

```bash
dx-extract inspect trx300_page.png
dx-extract extract trx300_page.png --output trx300_project
```

The extraction command writes:

```text
project/
├── project.json
├── artifacts/
│   ├── normalized/
│   ├── masks/
│   ├── segments/
│   └── validation/
└── output/
    └── wires.svg
```

## Design rule

The SVG is not the source of truth.

The source of truth is the structured intermediate representation and, ultimately,
the topology graph. SVG is an editable/renderable projection of that model.

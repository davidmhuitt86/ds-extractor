# EKE Diagram Extraction Engine Architecture Package

This package defines the proposed architecture for a deterministic diagram extraction engine, beginning with the wire-reconstruction vertical slice.

The architecture is deliberately split into:

1. normative requirements;
2. system architecture;
3. software/module architecture;
4. algorithms;
5. domain model;
6. standalone application;
7. CLI/API;
8. testing and validation;
9. implementation plan;
10. architecture decisions;
11. requirements traceability.

The intended first implementation is a C++23/CMake library with a CLI host and a 1988 Honda TRX300 diagram as the initial golden fixture.

The standalone application and eventual EKE integration use the same extraction library.

**Important:** Items labeled/proposed by the architecture documents are design proposals. They are not claims that the current EKE repository already implements them.

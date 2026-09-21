# EKE Diagram Extraction Engine — Document Suite

**Suite:** EKE-DX-WIRE  
**Purpose:** Architecture and implementation foundation for a deterministic wiring-diagram extraction engine and a standalone engineering application.

## Document set

| ID | Document | Purpose |
|---|---|---|
| EKE-DX-WIRE-001 | Deterministic Wire Reconstruction Specification | Normative wire extraction requirements |
| EKE-DX-WIRE-ARCH-001 | Extraction Engine System Architecture | System boundaries, pipeline, services, data flow |
| EKE-DX-WIRE-SW-001 | Software Architecture & Module Specification | Code organization and module responsibilities |
| EKE-DX-WIRE-ALG-001 | Algorithm & Processing Specification | Concrete algorithms and processing stages |
| EKE-DX-WIRE-MDL-001 | Domain Model & Intermediate Representation | Engineering object model, graph model, provenance |
| EKE-DX-WIRE-APP-001 | Standalone Application Architecture | Standalone desktop application architecture |
| EKE-DX-WIRE-API-001 | CLI/API Specification | Programmatic interface and command model |
| EKE-DX-WIRE-TST-001 | Validation & Test Specification | Automated tests, fixtures, metrics, acceptance gates |
| EKE-DX-WIRE-IMP-001 | Implementation Plan | Build sequence and milestones |
| EKE-DX-WIRE-ADR-001 | Architecture Decision Record | Key architectural decisions and rationale |

## Architectural intent

The engine is designed around the principle:

> **The image is evidence. The reconstructed engineering graph is the product.**

SVG is an output representation, not the internal source of truth.

The initial implementation targets wiring diagrams because the TRX300 diagram provides a useful calibration fixture. The architecture must not permanently depend on the TRX300's coordinates or symbols.

## Source basis

The suite is grounded in:

- `EKE-DX-WIRE-001 — Deterministic Wiring Diagram Reconstruction Specification`
- the supplied Claude extraction log and its demonstrated extraction workflow
- the identified double-edge problem from the initial wire-only reconstruction

Items explicitly marked **PROPOSED** are architectural recommendations for implementation and are not claims about functionality already implemented in EKE.

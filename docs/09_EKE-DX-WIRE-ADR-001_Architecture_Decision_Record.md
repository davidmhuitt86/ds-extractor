# EKE-DX-WIRE-ADR-001
# Architecture Decision Record

---

## ADR-001 — Separate reconstruction from rendering

**Decision:** The internal engineering graph is the source of truth. SVG is an export/view representation.

**Reason:** SVG-first implementations tend to encode presentation decisions directly into reconstruction logic.

---

## ADR-002 — Use centerlines rather than edge traces

**Decision:** Physical conductors are represented by centerlines with stroke width.

**Reason:** Edge-based extraction produced two geometric lines for one physical wire.

---

## ADR-003 — Morphological line extraction is the primary wire detector

**Decision:** Use horizontal/vertical morphological masks as the initial deterministic wire detector.

**Reason:** The reference extraction demonstrated that this is better aligned with orthogonal wiring diagrams than raw Canny/Hough edge tracing.

---

## ADR-004 — Non-wire regions are a first-class input

**Decision:** Component/connector/text exclusion regions are represented through a `NonWireRegionProvider`.

**Reason:** A wiring diagram contains many straight graphical objects that are not wires.

---

## ADR-005 — Manual exclusions are permitted during calibration

**Decision:** The TRX300 implementation may use manual exclusion geometry.

**Reason:** It allows the deterministic reconstruction algorithm to be validated before automatic object detection is complete.

**Constraint:** Manual coordinates must remain configuration data, not algorithm code.

---

## ADR-006 — Topology is reconstructed independently of SVG

**Decision:** Build a graph before export.

**Reason:** Electrical continuity is a graph concept, not an SVG concept.

---

## ADR-007 — Crossings are not automatically junctions

**Decision:** A geometric intersection does not imply electrical connectivity.

**Reason:** Wiring diagrams routinely contain crossing conductors without connection.

---

## ADR-008 — Provenance is mandatory

**Decision:** Derived objects retain source and processing provenance.

**Reason:** Diagram extraction requires auditability and iterative correction.

---

## ADR-009 — Deterministic core first

**Decision:** The initial engine shall not require AI.

**Reason:** Deterministic geometry and topology provide a stable baseline and permit repeatable testing. AI can later address semantic ambiguity.

---

## ADR-010 — Standalone application uses the same engine as EKE

**Decision:** The standalone application shall link against the same extraction library intended for EKE.

**Reason:** Prevents prototype divergence and makes the standalone application a development/authoring surface for the eventual EKE workflow.

---

## ADR-011 — Intermediate artifacts are first-class

**Decision:** Each major pipeline stage should be serializable.

**Reason:** This makes failures diagnosable and permits stage-level reruns without repeating the entire pipeline.

---

## ADR-012 — TRX300 is the calibration fixture, not the architecture

**Decision:** The 1988 Honda TRX300 diagram is the first golden fixture but no production algorithm may depend permanently on its coordinates.

**Reason:** The goal is a general diagram extraction engine.


# AP-DIAG-IMPL-001-PRECHECK: Connector Implementation Boundary Audit

## Status
**BLOCKED FOR PRODUCTION IMPLEMENTATION — DESIGN CORRECTION REQUIRED**

This precheck was performed directly against `main` at `c4a88ba0a31995996b2e824b4928061de351b057`.

## Finding 1 — ConnectorCandidate construction is incompatible with the unchanged builder

The design requires ConnectorBody regions to bypass ComponentCandidate, ConnectorCandidate to be reused unchanged, ConnectorTerminalModelBuilder to remain unchanged, and ConnectorTerminal to remain produced through the existing connector model path.

The current ConnectorTerminalModelBuilder requires a ConnectorBoundary TerminalCandidate, a non-empty component_candidate_id, and a matching ComponentCandidate with that exact ID. It then creates the ConnectorCandidate from that ComponentCandidate.

Therefore a connector body that correctly bypasses ComponentCandidate cannot reach the existing builder. Assigning the ConnectorGeometryDetector region ID to ConnectorCandidate::component_candidate_id does not solve this because the builder performs an actual ComponentCandidate lookup. Creating a synthetic ComponentCandidate would violate the approved architecture and alter the component candidate population.

### Required correction

Option A — Extend ConnectorTerminalModelBuilder so it accepts already-created ConnectorCandidate / ConnectorPin objects and independently established endpoint evidence, while preserving its existing component-boundary path unchanged.

Option B — Introduce a separate connector-terminal materializer while leaving ConnectorTerminalModelBuilder unchanged for the legacy component-boundary path.

The current AP-DIAG-DESIGN-001 implementation boundary permits neither option.

## Finding 2 — TerminalLocationDetector cannot attach a ConnectorPin without an API extension

TerminalLocationDetector currently accepts ComponentCandidate, EndpointCandidate, rejected geometry, and symbol primitives. Its kind mapping assigns ConnectorBoundary only to ComponentCandidateKind::PrimitiveSymbol.

A ConnectorPin is intentionally not a ComponentCandidate, and the design rejects ComponentCandidateKind::Connector. Therefore the approved pin path requires a distinct input path that directly emits TerminalCandidateKind::ConnectorBoundary. It must not route the pin through kind_for_component().

## Finding 3 — TerminalRecognizer has the same boundary problem

TerminalRecognizer also operates on ComponentCandidate and derives terminal kind from ComponentCandidateKind. A connector pin therefore requires a dedicated connector-pin evidence path using the existing distance/alignment rules, rather than pretending the pin is a component.

## Finding 4 — ConnectorPin field-count documentation is inconsistent

The design calls ConnectorPin a five-field struct, but the specified declaration contains id, connector_id, position, ordinal, conductor_crossing_evidence_ids, and confidence: six fields.

This is documentation-level only but should be corrected before implementation.

## Finding 5 — Crossing evidence is named but not concretely specified

The design requires connector_conductor_crossing_evidence but does not define its exact record schema with the same precision as ConnectorPin.

A correction should specify at minimum: evidence ID, connector ID, conductor segment ID, crossing point, confidence, and provenance/source region. It should also state whether one record represents one conductor-segment crossing of one connector boundary.

## Conclusion

AP-DIAG-IMPL-001 is not yet safe to implement exactly as written. The geometry architecture is implementable. The blocker is the semantic materialization boundary: ConnectorBody -> ConnectorCandidate -> ConnectorPin -> TerminalCandidate -> ConnectorTerminal cannot be completed while simultaneously requiring ConnectorBody to bypass ComponentCandidate, ConnectorTerminalModelBuilder to remain unchanged, no synthetic ComponentCandidate, no ComponentCandidateKind::Connector, and ConnectorTerminal to remain the downstream representation.

The implementation should pause at this boundary rather than silently selecting a prohibited shortcut.

## Recommended next AP

AP-DIAG-DESIGN-002 — Connector Semantic Materialization Boundary.

Resolve only this implementation contradiction. The preferred correction is to extend the existing connector-terminal materialization boundary so a genuine ConnectorCandidate can be materialized directly from ConnectorBody + ConnectorPin + independently established endpoint evidence, without creating a synthetic ComponentCandidate.

No geometry thresholds, Wire identity, topology, electrical-net logic, or ground detection should change as part of that correction.

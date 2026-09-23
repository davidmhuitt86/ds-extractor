# AP-WIRE-020 — Connector & Terminal Model

## Purpose

AP-WIRE-020 materializes explicit connector and connector-terminal engineering objects from connector-boundary evidence already established by earlier pipeline stages.

The stage separates:

- connector object identity at the diagram-model boundary;
- individual connector terminals;
- endpoint geometry and topology;
- terminal semantic fields already resolved by AP-WIRE-019.

## Inputs

- ComponentCandidate
- TerminalCandidate
- EndpointCandidate

Only TerminalCandidateKind::ConnectorBoundary evidence can create connector objects.

## Outputs

### ConnectorCandidate

A connector candidate contains:

- deterministic connector ID;
- source component-candidate ID;
- connector bounds;
- source confidence;
- semantic labels already materialized onto the component candidate.

### ConnectorTerminal

A connector terminal contains:

- deterministic terminal ID;
- connector ID;
- endpoint ID;
- endpoint position;
- terminal name;
- function label;
- wire color;
- terminal role;
- confidence;
- explicit resolution status.

## Invariants

1. Connector objects are not created from arbitrary geometry.
2. Component identity is not inferred or canonicalized here.
3. Pin numbers are not invented.
4. Terminal names are copied only from existing endpoint semantic state.
5. Connector terminals retain their originating endpoint IDs.
6. Geometry and topology are not modified.
7. Wire identity is not modified.
8. Deterministic ordering and IDs are required.
9. Missing endpoint references are ignored rather than fabricated.
10. Non-connector terminal candidates do not enter the connector model.

## Pipeline Position

Image → Geometry → Topology → Endpoint Semantic Reconstruction (AP-WIRE-019) → Connector & Terminal Model (AP-WIRE-020) → Wire Reconstruction → Electrical Net Resolution

AP-WIRE-020 establishes the explicit connector/terminal object boundary needed by later component recognition and structured diagram reconstruction.

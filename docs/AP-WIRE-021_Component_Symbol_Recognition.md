# AP-WIRE-021 — Component Symbol Recognition

## Purpose

AP-WIRE-021 establishes an explicit engineering-model boundary for component symbol classification.

It converts the existing ComponentCandidate classification into a deterministic ComponentSymbolRecognition record while preserving the candidate as the source geometry object.

## Inputs

- ComponentCandidate

The candidate already carries:
- deterministic component ID;
- component candidate kind;
- source shape IDs;
- bounds;
- confidence.

## Outputs

ComponentSymbolRecognition contains:

- deterministic recognition ID;
- originating component ID;
- symbol classification;
- inherited confidence;
- explicit recognition status;
- originating shape IDs.

Supported symbol classifications are:

- Enclosure
- CircularSymbol
- ChassisGround
- PrimitiveSymbol
- Unknown

## Recognition Rule

This AP deliberately does not perform visual identity recognition.

The current implementation maps the already-established ComponentCandidateKind to the corresponding ComponentSymbolKind.

Therefore:

- no component identity is inferred;
- no OCR is performed;
- no fuzzy matching is performed;
- no registry lookup is performed;
- no terminal or pin is invented;
- no topology is changed;
- no wire identity is changed.

An Unknown candidate remains explicitly unresolved.

## Determinism

Recognition IDs are:

component-symbol-recognition-<component-id>

Results are sorted by recognition ID.

## Pipeline Position

Image → Geometry → Topology → Endpoint Semantic Reconstruction → Connector & Terminal Model → Component Symbol Recognition → Component Identity / Semantic Recognition → Structured Diagram Reconstruction

AP-WIRE-021 provides the explicit symbol-classification object required by later stages to distinguish diagram geometry from engineering-object identity.

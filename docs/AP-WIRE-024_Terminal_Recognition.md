# AP-WIRE-024 — Terminal Recognition & Component-Terminal Association

Status: validated. See `docs/AP-WIRE-024_AAR.md` for the full validation report, AP-WIRE-023-vs-024 comparison, coverage-diagnostic comparison, terminal-recognition breakdown, and a recommended (non-blocking) tightening of the boundary/alignment fallback before AP-WIRE-025.

## Purpose

Consume the geometric evidence established by AP-WIRE-023 and associate it with existing conductor endpoints without inventing new endpoints or altering wire/topology/electrical-net identity.

AP-WIRE-023 established 9 TerminalLead primitives across 6 components. AP-WIRE-024 treats those as geometric evidence only and attempts to attach them to already-existing EndpointCandidate objects. For components with no usable terminal-lead geometry, a conservative boundary/alignment fallback uses an existing endpoint and its incident topology direction.

## Boundaries

This AP:
- consumes ComponentSymbolGeometry and SymbolPrimitive from AP-WIRE-023;
- consumes existing EndpointCandidate, TopologyNode, and TopologyEdge objects;
- creates only TerminalCandidate associations;
- preserves deterministic IDs and confidence;
- preserves existing terminal associations without duplication;
- leaves EndpointCandidate creation to endpoint reconstruction;
- leaves endpoint semantic mutation to AP-WIRE-019;
- leaves connector construction to AP-WIRE-020.

This AP does not:
- create endpoints;
- create wires;
- split or merge wires;
- mutate topology;
- alter electrical-net membership;
- assign symbol-family identities;
- infer pin numbers or terminal names;
- use fuzzy component identity;
- treat a TerminalLead as an electrical terminal by itself;
- treat DiagramFurniture as an engineering component.

## Recognition paths

### 1. TerminalLead association

For each real component's TerminalLead primitive:
1. Find existing endpoints within the configured lead distance.
2. Select the unique nearest endpoint.
3. Reject an equidistant tie rather than selecting a winner.
4. Produce a TerminalCandidate using the existing endpoint position.
5. Combine primitive confidence with geometric distance confidence.
6. Never create an endpoint if no existing endpoint is sufficiently close.

Default lead distance: 6 px.

### 2. Boundary/alignment association

For components without a usable TerminalLead, an existing endpoint may be associated when:
- it is within the configured boundary-extension distance;
- its incident topology conductor points toward the component boundary;
- the directional cosine meets the configured minimum.

Defaults: maximum extension 16 px; minimum directional cosine 0.85.

This path exists specifically for low-resolution symbols whose electrical connection is represented by a conductor ending slightly short of the detected component boundary. Distance alone is insufficient.

## Ambiguity policy

No winner is invented when terminal evidence is ambiguous.

Examples:
- two endpoints equally close to one TerminalLead → no recognition;
- an endpoint's conductor points away from a component → no recognition;
- DiagramFurniture component → no recognition;
- unsupported component kind → no recognition.

If separate evidence associates one endpoint with multiple components, the candidates remain explicit and AP-WIRE-019's existing conflict handling determines the endpoint semantic result without selecting an arbitrary component.

## Determinism

Recognition IDs use the existing stable_id mechanism and depend only on endpoint/component/primitive identity and the recognition path. Results are sorted by ID.

## Pipeline position

ComponentCandidate → DiagramFurnitureClassifier → AP-WIRE-023 SymbolGeometryExtractor → EndpointReconstructor → TerminalLocationDetector → AP-WIRE-024 TerminalRecognizer → TerminalSemanticEvidenceBuilder → AP-WIRE-019 EndpointSemanticReconstructor → connector/wire/net stages.

The recognizer therefore feeds the established semantic endpoint boundary but does not enter the wire/topology reconstruction boundary.

## Regression coverage

tests/test_terminal_recognizer.cpp covers TerminalLead association, equal-distance ambiguity rejection, boundary/alignment fallback, conductor-direction rejection, preservation of existing associations, and DiagramFurniture exclusion. The test is registered as dx-wire-test-terminal-recognizer.

## Validation requirement

Before AP-WIRE-024 is declared complete, run Release build, full Release CTest, fresh TRX300 extraction, AP-WIRE-022A coverage diagnostics, and comparison against the AP-WIRE-023 baseline.

Critical measurements: real components with terminal evidence; endpoint zero-wire coverage; conductor segments owned by wires; topology edges owned by wires; connector population; electrical-net endpoint membership; validation warning histogram.

No increase in wire/topology/net counts is itself a success criterion: those objects remain governed by their existing stages and the endpoint-to-endpoint wire identity rule.
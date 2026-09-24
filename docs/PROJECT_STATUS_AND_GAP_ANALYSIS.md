# EKE-DX-WIRE — Engineering Development Status & Gap Analysis

Repository: davidmhuitt86/ds-extractor  
Platform: C++23 / CMake / OpenCV  
Primary target: structured reconstruction of automotive/ATV wiring diagrams  
Reference diagram: 1988 Honda FourTrax TRX300 wiring diagram  
Status baseline: current main branch

## 1. Project Objective

EKE-DX-WIRE is being developed as a deterministic extraction and reconstruction engine for poor-quality wiring diagrams. The objective is not raster tracing. The intended result is an engineering representation containing conductor geometry, topology, endpoint candidates, wires, components, symbols, terminals, connectors, splices, electrical nets, labels, semantic evidence, wire colors, provenance, confidence, and eventually a fully structured editable SVG.

Architecture:

Source Image/PDF → Geometry → Topology → Endpoints → Semantic Resolution → Engineering Objects → Structured Diagram Model → Editable SVG → Diagram Studio

## 2. Fundamental Engineering Rule

A wire is an endpoint-to-endpoint engineering trace.

A splice or junction is not itself a wire endpoint. A wire may pass through one or more splices/junctions and continue to its final endpoint. Shared conductor sections may therefore participate in multiple endpoint-to-endpoint traces when a topology branch occurs.

Example:

Terminal A --------o--------- Terminal B
                   |
                   +--------- Terminal C

Possible wire identities:

Wire A → B
Wire A → C

This rule prevents pixel segmentation from defining engineering wire identity.

## 3. Current Architecture

### Geometry

ConductorSegment represents observed conductor geometry. A detected line is not automatically a Wire.

RejectedGeometryEvidence preserves geometry rejected from conductor interpretation, including component-, connector-, text-, and unresolved-associated geometry.

### Topology

TopologyNode represents graph points. Current node types include ConductorEnd, Continuation, Junction, Splice, Crossing, ComponentBoundary, and Unresolved.

TopologyEdge represents graph connectivity backed by conductor geometry.

### Engineering endpoints

EndpointCandidate carries node, position, endpoint kind, terminal role, confidence, incident edges, evidence, optional component association, terminal name, function label, and wire color.

### Wires

Wire contains start endpoint, end endpoint, topology edges, conductor segments, confidence, and heavy-cable state.

### Electrical nets

ElectricalNet contains endpoint membership, splice membership, topology-edge membership, distribution role, confidence, and an optional semantic anchor.

## 4. Implemented Development Stages

The project has progressed from raw conductor extraction toward an explicit engineering-object pipeline.

### Foundation / Geometry

Implemented capabilities include C++23 project structure, CMake build, OpenCV image processing, adaptive thresholding, horizontal and vertical morphology, conductor candidate detection, centerline generation, deterministic IDs, conductor normalization, rejected-geometry evidence, shape detection/exclusion masks, and vector SVG projection.

### Topology Reconstruction

Topology reconstruction is implemented and separated from geometry extraction. The pipeline produces topology nodes, topology edges, continuations, junctions, splices, crossings, and component boundaries. Gap interpretation can infer selected continuation edges across small geometric gaps.

## 5. AP-WIRE Progress

### AP-WIRE-004 — Duplicate Wire Integrity
Deterministic protection against duplicate wire IDs after distribution decomposition. The validator reports duplicate-wire conditions.

### AP-WIRE-005 — Electrical Network Semantic Resolution
Established explicit handling of Ground, PowerFeed, and SharedFunctionFeed semantics. Conflicting role evidence is preserved rather than silently resolved.

### AP-WIRE-006 — Semantic Evidence Association
Established spatial association between recognized semantic evidence and engineering objects without mutating topology.

### AP-WIRE-007 — Text Evidence Interpretation
Established lexical classification and normalization for GroundLabel, PowerFeedLabel, SharedFunctionFeedLabel, ComponentLabel, ConnectorLabel, TerminalLabel, WireColorLabel, FunctionLabel, and Unknown.

### AP-WIRE-008 — Text Recognition Provider Boundary
Established an injectable external recognition-provider boundary. OCR is not coupled to the core extractor.

### AP-WIRE-009 — Recognition Evidence Adapter
Established a sidecar boundary for externally supplied recognition observations.

### AP-WIRE-010 — Recognition Input / Export Package
Established export of recognition context including source information, regions, coordinates, neighboring geometry, components/endpoints, semantic associations, and recognition instructions.

### AP-WIRE-011 — Recognition Observation Import
Recognition observations can be imported through the extraction pipeline using JSON observations.

### AP-WIRE-012 — Semantic Observation Resolution
Recognized role-bearing text is spatially resolved before it can influence circuit-role semantics.

### AP-WIRE-013 — Recognition Loop Measurement
Established baseline-versus-recognition-assisted measurement and semantic resolution reporting.

### AP-WIRE-014 — Engineering Object Semantic Resolution
Established deterministic semantic association against engineering objects, including ambiguity/conflict preservation, confidence, and provenance.

### AP-WIRE-015 — Engineering Object Semantic Materialization
Resolved semantic evidence can populate component labels, terminal names, function labels, and wire-color fields without overwriting conflicting information.

### AP-WIRE-016 — Component Identity Evidence Boundary
Established explicit identity-bearing evidence from component and connector labels without asserting canonical identity.

### AP-WIRE-017 — Component Identity Resolution
Established deterministic resolution of explicit identity evidence. One normalized identity resolves; multiple conflicting identities become Conflicted; missing evidence remains Unresolved.

### AP-WIRE-018 — Component Identity Registry & Canonicalization
Established the boundary between diagram evidence and canonical registry identity. The registry is injectable. The current static registry is a development/test boundary, not yet the production OEP registry.

### AP-WIRE-019 — Endpoint Semantic Reconstruction
Established explicit reconstruction of endpoint kind, terminal role, component association, confidence, and conflict state without changing conductor geometry, topology, splice semantics, or wire identity.

### AP-WIRE-020 — Connector & Terminal Model
Established explicit ConnectorCandidate and ConnectorTerminal objects. Connector terminals retain their originating endpoint IDs. Pin numbers and connector identities are not invented.

### AP-WIRE-021 — Component Symbol Recognition Boundary
Established ComponentSymbolRecognition as an explicit model boundary. Important limitation: the current implementation maps existing ComponentCandidateKind to ComponentSymbolKind. It is not yet true visual recognition of specific electrical symbols.

Supported current categories are Enclosure, CircularSymbol, ChassisGround, PrimitiveSymbol, and Unknown.

The latest TRX300 visual extraction confirmed that the system frequently detects component regions/bounds while failing to reconstruct the internal symbol geometry. This is one of the principal remaining engineering gaps.

### AP-WIRE-022 — Electrical Net Resolution
Established a single orchestration boundary combining topology/distribution decomposition, endpoint circuit-role evidence, semantic circuit-role evidence, and deterministic net normalization.

Rules include: no invented source, unresolved structures remain unresolved, cyclic structures remain unresolved, anchors must belong to their nets, referenced endpoints must exist, deterministic membership, no fuzzy matching, no OCR, and no topology mutation.

### AP-WIRE-022A — Extraction Baseline Validation & Diagnostic Expansion
Established deterministic warning-code classification for the existing structural validation report, plus a full coverage-diagnostic layer (`build_coverage_report`) that observes conductor-segment ownership, endpoint wire coverage, wire integrity, topology edge/node ownership, component terminal coverage (DiagramFurniture kept as a distinct, non-error classification), connector coverage, and electrical-net endpoint membership. Findings are deterministically ordered and exported in full under `extraction_audit.json`'s `coverage` block, with a compact summary in `review_manifest.json`.

This stage is observational. It does not repair geometry, topology, endpoints, wires, components, or electrical nets. See `docs/AP-WIRE-022A_AAR.md` for the after-action report and identified failure clusters.

## 6. Latest Extraction Baseline

Fresh TRX300 extraction against current main (HEAD at the time of writing:
`f229218`, after the DiagramFurnitureClassifier and coverage-diagnostics
changes). Numbers superseded by any later baseline in
`docs/AP-WIRE-022A_AAR.md`; do not treat this table as authoritative once
that AAR exists.

| Object | Result |
|---|---:|
| Conductor segments | 294 |
| Topology nodes | 692 |
| Topology edges | 877 |
| Endpoint candidates | 210 |
| Component candidates | 109 |
| Connector candidates | 0 |
| Connector terminals | 0 |
| Wires | 40 |
| Electrical nets | 10 |
| Gap bridges | 6 |
| Validation errors | 0 |
| Validation warnings | 30 |
| Unresolved wires | 0 |

The connector count dropped from a previously reported 10 candidates /
11 terminals to 0/0: the DiagramFurnitureClassifier change (commit
`d642afd`) established that the prior connector population was actually
the switch-continuity table, not real connector geometry. Component
candidates now split 47 CircularSymbol + 10 ChassisGround + 2 Enclosure
(59 real candidates) + 50 DiagramFurniture, with 0 PrimitiveSymbol.

Coverage diagnostics (AP-WIRE-022A) show that only 50 of 294 conductor
segments and 50 of 877 topology edges are currently claimed by a
reconstructed Wire, and only 33 of 210 endpoints belong to an electrical
net. See `docs/AP-WIRE-022A_AAR.md` for the full breakdown and likely
responsible pipeline stages.

AP-WIRE-023 added internal symbol geometry on top of this same baseline
without changing any of the numbers above: 17 of the 59 real component
candidates now carry at least one `SymbolPrimitive` (40 primitives total:
22 Unknown, 9 TerminalLead, 6 Rectangle, 2 Circle, 1 Line). See
`docs/AP-WIRE-023_AAR.md`.

## 7. Current SVG State

The current SVG is still primarily a conductor reconstruction artifact rather than the final structured engineering diagram.

The latest SVG contained 294 line elements, with no path, text, circle, or rectangle elements.

The final SVG must instead represent editable engineering objects and preserve their relationships to the engineering model.

## 8. PDF Support

Native Windows PDF loading has been implemented. The loader detects PDF input, uses Windows Runtime PDF APIs, renders the first page, converts it to an OpenCV image, and sends it through the same extraction pipeline.

The GUI file picker accepts PDF, PNG, JPEG, BMP, and TIFF.

## 9. Development GUI

The Windows development GUI currently provides a native file picker, PDF selection, source display, extraction controls, morphology controls, source/binary/mask/wire/topology views, endpoint and shape views, engineering-layer controls, zoom, fit view, mouse-wheel zoom, middle-mouse pan, reset view, build/test, release, output status, and artifact-path display.

The GUI remains a development/calibration workbench and is not the final Diagram Studio/EKE interface.

## 10. Automatic Visual Extraction Review

A new automated review system generates the following after extraction:

artifacts/extraction_review/

00_source.png
01_wires.png
02_wire_colors.png
03_symbols.png
04_terminals.png
05_connectors.png
06_splices.png
07_grounds.png
08_labels.png
09_topology.png
10_component_bounds.png
11_endpoint_debug.png
12_recognition.png
13_combined.png
review_manifest.json

The images are generated directly from the extraction model at source-image resolution, not from the GUI window. The review directory is replaced on every extraction so it represents only the newest run.

The manifest records source, page, dimensions, layer counts, electrical-net count, validation errors/warnings, and generation timestamp.

## 11. Automatic Review Publication

tools/dx-publish-review.ps1 was added.

The intended workflow is:

Extraction → Review generation → Manifest → Git commit → git push origin main

Only artifacts/extraction_review is staged by the publisher. Source code and build products are not staged by this publisher.

If extraction fails, review publication should not occur.

This removes the need to manually screenshot the GUI and send screenshots during iterative extractor development.

## 12. Release / Build Automation

The current Release workflow is a validation/synchronization operation:

RELEASE → verify repository → verify main → git pull origin main → CMake configure → Release build → Release CTest → relaunch GUI

The release workflow does not commit source changes, push source changes, create pull requests, stash changes, or reset changes.

The Release build is intentionally serial because parallel MSBuild previously exposed an artifact-generation race involving test object files.

## 13. Major Remaining Gaps

### Gap 1 — True Component Symbol Recognition

This is currently the largest visible gap. The system detects component regions but does not yet reliably understand the internal symbol geometry.

Required symbol families include switches, relays, motors, generators, lamps, batteries, resistors, diodes, rectifiers, coils, fuses, sensors, connectors, and compound symbols.

### Gap 2 — Internal Symbol Geometry Model

AP-WIRE-023 (complete; see `docs/AP-WIRE-023_AAR.md`) established this
layer: `ComponentSymbolGeometry`/`SymbolPrimitive` (Line, Circle,
Rectangle, TerminalLead, Unknown) attached to real (non-furniture)
component candidates, with deterministic IDs, provenance, and confidence.
17 of 59 real TRX300 components produced at least one primitive; the
other 42 (all `circular_symbol`) are, on inspection, plain rings/dots
with no internal geometry beyond their own outline. This closes Gap 2 to
the extent the fixture supports; Gap 1 (true symbol-family recognition)
and Gap 3 (terminal recognition) remain open and are AP-WIRE-024's scope.

### Gap 3 — True Terminal Recognition

Many endpoints remain unresolved. The system must distinguish actual component terminals, connector pins, wire continuation points, graphic intersections, symbol-internal geometry, ground connections, and unresolved endpoints.

### Gap 4 — Wire Semantic Completion

The 40 current wires need richer semantics such as wire color, stripe/color combination, gauge where observable, source component, destination component, source/destination terminals, electrical net, function, confidence, evidence, and provenance.

### Gap 5 — Wire Color Recognition

The model contains wire-color fields and recognition boundaries, but reliable automatic extraction across the entire diagram has not yet been demonstrated.

### Gap 6 — Label Recognition

OCR/recognition-provider integration exists, but the latest TRX300 extraction has not yet demonstrated comprehensive recognized-text evidence.

### Gap 7 — Production Component Registry

The registry boundary exists, but the production OEP/EKE component registry has not yet been connected. The current static registry is a development/test mechanism.

### Gap 8 — Complete Structured SVG

The current SVG is primarily line geometry. The final output must contain editable wires, symbols, terminals, connectors, labels, splices, junctions, grounds, annotations, and engineering metadata.

### Gap 9 — Electrical Semantics

Electrical-net resolution currently establishes connectivity and role evidence. It does not yet determine voltage, current, circuit behavior, loading, switching behavior, component function, or circuit correctness.

### Gap 10 — Engineering Validation / AAR

Current validation catches structural problems. A later AAR should compare source diagram, extracted engineering model, and structured SVG and report missing objects, extra objects, broken connectivity, unresolved terminals, identity conflicts, unsupported symbols, suspicious geometry, uncertain semantics, and wire/net discrepancies.

## 14. Revised Remaining AP Sequence

The next work should follow the evidence from the latest visual extraction rather than advancing the AP number blindly.

This sequence corrects a previous inconsistency in this document, which
described AP-WIRE-022A as a diagnostics stage but then listed AP-WIRE-023
as "Wire Semantic Completion" — skipping internal symbol geometry
extraction and terminal recognition, which must come first per Gap 1/2/3
above. The corrected sequence:

AP-WIRE-022 — Electrical Net Resolution — complete.

AP-WIRE-022A — Extraction Baseline Validation & Diagnostic Expansion — complete (see `docs/AP-WIRE-022A_AAR.md`).

AP-WIRE-023 — Internal Symbol Geometry Extraction — complete (see `docs/AP-WIRE-023_AAR.md` and `docs/AP-WIRE-023_Internal_Symbol_Geometry.md`). Established `ComponentSymbolGeometry`/`SymbolPrimitive` for real components; explicitly did not attempt symbol-family recognition or terminal association.

AP-WIRE-024 — Terminal Recognition & Component-Terminal Association — validated (see `docs/AP-WIRE-024_AAR.md`): 45/45 Release CTest, fresh TRX300 extraction confirms wire/topology/electrical-net identity byte-for-byte unchanged, all engineering-conflict cases correctly surfaced as `conflicted` rather than silently resolved. Real components with terminal evidence rose 18→20; TerminalCandidate count 48→56 (7 of 8 new candidates from the boundary/alignment fallback, only 1 from `TerminalLead` evidence). The AAR documents a real tradeoff worth tuning before AP-WIRE-025: the boundary/alignment fallback also demoted 3 previously-confident `ground` endpoints and 1 `component_terminal` endpoint to unresolved by introducing competing 9-14px claims. See `docs/AP-WIRE-024_Terminal_Recognition.md` for the design and `docs/AP-WIRE-024_AAR.md` for the validation, comparison tables, and recommended tightening.

AP-WIRE-025 — Wire Semantic Completion — validated (see `docs/AP-WIRE-025_AAR.md`): 46/46 Release CTest, fresh TRX300 extraction confirms wire/topology/electrical-net identity byte-for-byte unchanged. Attaches `WireSemanticResolution` (wire color, function, component/terminal/connector association, electrical-net association) to each of the 40 wires from existing evidence only, with an explicit Resolved/Unresolved/Conflicted status per field. On this fixture: 16/40 wires get a resolved component association (0 conflicted at the aggregate level, though one wire's non-aggregate detail correctly shows a Conflicted end from the AP-WIRE-024 boundary/alignment case — see the AAR §6a), 5/40 get an electrical-net association, and wire-color/function/connector association are 0/40 given no text-recognition evidence or connector population on this fixture/baseline — all Unresolved, not guessed. Deliberately did not add source/destination distribution-role fields: no defensible per-endpoint evidence is currently retained on `WireModel` for it (see the AAR's "known limitations"). See `docs/AP-WIRE-025_Wire_Semantic_Completion.md` for the design and `docs/AP-WIRE-025_AAR.md` for the full validation.

AP-WIRE-026 — Unified Engineering Diagram Reconstruction — validated (see `docs/AP-WIRE-026_AAR.md`): 47/47 Release CTest, fresh TRX300 extraction confirms wire/topology/electrical-net identity byte-for-byte unchanged. Establishes `EngineeringDiagram` (`include/eke_dx_wire/diagram/`), a read-only reference-preserving projection joining AP-WIRE-022 through AP-WIRE-025's output (109 components, 40 wires each with a wire-semantic-resolution reference, 106 splices with derived incident-wire lists, 115 labels covering every TextRegion unconditionally, 10 electrical nets) with independently re-derived reference-integrity validation (415 valid references, 0 invalid, 0 duplicate, 0 orphan) and confirmed byte-identical determinism across repeated runs. Directly traced the AP-WIRE-024 known conflict (`endpoint-candidate-b0e3d6bb622a227c`) through to the unified model and confirmed it is still never fabricated into a component association. A new reference image (`samples/trx300_complete_diagram_view.png`, an OEP Diagram Studio rendering of the same harness) was studied as an architectural target and produced an explicit gap-analysis table in the AAR: the one substantive missing capability is component symbol-family identity (switch/relay/motor/lamp/diode/alternator/battery as distinct engineering types) — a genuinely missing *upstream* capability, not something this assembly layer should fabricate. See `docs/AP-WIRE-026_Unified_Engineering_Diagram.md` for the design and `docs/AP-WIRE-026_AAR.md` for the full validation, reference-image coverage table, and architecture assessment.

AP-WIRE-027 — Structured SVG Export.

AP-WIRE-028 — Source-vs-Model Engineering Validation / AAR.

The 022A correction is important because AP-WIRE-021 currently establishes the symbol model boundary but does not yet perform deep visual recognition.

## 15. Recommended Engineering Order

1. Use the automated visual review harness to diagnose each extraction layer.
2. Extract internal symbol geometry from component regions.
3. Recognize symbol classes from those internal primitives.
4. Associate conductor endpoints with recognized symbol terminals.
5. Complete wire semantics using terminal, color, label, component, and net evidence.
6. Build the unified engineering diagram object graph.
7. Render the engineering model into structured editable SVG.
8. Perform source-versus-model validation and produce an engineering AAR.

## 16. Completion Criteria

The extractor should not be considered complete merely because every line was traced, every component has a bounding box, an SVG exists, or structural validation reports zero errors.

For any conductor, the completed system should be able to answer: what wire is this, where does it begin, where does it terminate, what component/terminal does it connect to, what electrical net is it part of, what color/function evidence supports the conclusion, and how confident is the system?

For any component, it should answer: what engineering component is represented, what symbol geometry supports the conclusion, where are its terminals, what wires attach to those terminals, and what evidence identifies it?

For any electrical net, it should answer: which endpoints and splices belong to it, what role evidence establishes it, and which portions remain unresolved?

For the final SVG, engineering objects should be editable independently without returning to the raster source.

## 17. Source-of-Truth Principle

The project should maintain this hierarchy:

Specification → Domain model → Pipeline stage → Validation → Artifact export → GUI visualization

The GUI must not become the authoritative extraction engine. The SVG must not become the authoritative engineering model. The raster image must not become the authoritative topology.

The engineering model remains the source of truth; visual artifacts are inspection and communication surfaces.

## 18. Current Overall State

The project has progressed beyond a basic computer-vision wire tracer. It now has explicit boundaries for geometry, topology, endpoints, semantic evidence, component identity, connectors/terminals, electrical nets, deterministic artifacts, and visual review.

The principal remaining transition is from geometric detection to engineering recognition.

The automatic review system now provides the instrumentation required to make that transition systematically: every extraction can produce repeatable layer images plus machine-readable counts, and those artifacts can be published for inspection without manually taking screenshots.

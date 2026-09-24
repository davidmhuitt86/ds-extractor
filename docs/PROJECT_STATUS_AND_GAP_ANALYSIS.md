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

As of AP-WIRE-027, `output/wires.svg` is a real structured engineering
SVG, not a raw conductor-line dump. It is produced by
`StructuredSvgExporter` from `EngineeringDiagram`/`WireModel` alone (no
source image, no OpenCV) and contains `<g>`/`<line>`/`<circle>`/`<rect>`/
`<text>` elements organized into components/connectors/wires/splices/
crossings/terminals/grounds/labels/annotations/electrical-nets/metadata
groups, each traceable back to its engineering object via `data-*-id`
provenance attributes, with Resolved/Unresolved/Conflicted status
preserved rather than collapsed. On the TRX300 baseline it renders 642
uniquely-identified elements with 0 dangling references (see
`docs/AP-WIRE-027_AAR.md`). Remaining gaps: no connectors render (0
resolve upstream on this baseline), 49/59 components still render the
generic unresolved placeholder rather than a specific symbol glyph
(pending future symbol-family recognition coverage, not a rendering
defect), and no auto-layout exists (source-page-pixel coordinates are
used as-is).

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

Closed by AP-WIRE-027 (see `docs/AP-WIRE-027_AAR.md`): the output now
contains structured wires, symbol glyphs (where resolved) or explicit
placeholders (where not), terminals, connectors, labels, splices,
crossings, grounds, annotations, and engineering-metadata groups, each
carrying provenance back to its source engineering object. What remains
open is coverage, not structure: connectors don't render because none
resolve upstream yet, and most components render the generic placeholder
because symbol-family recognition coverage (Gap 1) is still narrow - both
are upstream recognition gaps, not SVG-structure gaps.

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

AP-WIRE-026A — Component Symbol-Family Recognition — validated (see `docs/AP-WIRE-026A_AAR.md`): 48/48 Release CTest, fresh TRX300 extraction confirms every structural invariant byte-for-byte unchanged. Closes the symbol-family gap AP-WIRE-026 identified, with a deliberately narrow, evidence-honest scope: `SymbolFamilyRecognizer` (`src/topology/`) has exactly 2 rules — `ChassisGround` kind (a purpose-built ground-detector output, not generic shape resemblance) resolves `Ground` alone; a label keyword combined with a geometrically compatible `ComponentCandidateKind` resolves the other 9 taxonomy families (Lamp/Switch/Relay/Motor/Diode/Alternator/Battery/Solenoid/Coil) — label alone or geometry alone is never sufficient. On TRX300's deterministic baseline: 10/59 real components resolve (all `Ground`, matching the 10 chassis-ground shapes exactly), 0 conflicted, 49 correctly `Unresolved` (the other families need label-recognition evidence this baseline doesn't have — reported as zero, not invented). Confirmed byte-identical determinism across two runs, and re-verified the AP-WIRE-024 known conflict is still never fabricated, now through five APs (024→026A). `EngineeringDiagram.DiagramComponent` gained one reference field (`symbol_family_resolution_id`) rather than a duplicate representation. See `docs/AP-WIRE-026A_Symbol_Family_Recognition.md` for the design/evidence rules and `docs/AP-WIRE-026A_AAR.md` for the full validation and the exact AP-WIRE-027 contract.

AP-WIRE-027 — Structured Engineering SVG Export — validated (see `docs/AP-WIRE-027_AAR.md`): 50/50 Release CTest, fresh TRX300 extraction confirms every structural invariant byte-for-byte unchanged (294/692/877/210/40/10/0/30). Establishes `StructuredSvgExporter` (`src/export/`), a renderer consuming `EngineeringDiagram` + `WireModel` exclusively (zero OpenCV/source-image dependency, verified by grep) that produces real structured SVG - `<line>`/`<circle>`/`<rect>`/`<text>`/`<g>`, never a raster `<image>` wrapper - organized into components/connectors/wires/splices/crossings/terminals/grounds/labels/annotations/electrical-nets/metadata groups, each element carrying `data-object-type`/`data-*-id`/status/confidence provenance attributes. A new `SymbolRenderer` boundary (`src/export/symbol_renderer.cpp`) supplies one presentation-only glyph per `SymbolFamily` plus explicit unresolved/conflicted placeholders, gated strictly on `SymbolFamilyResolutionStatus` - no recognition heuristic exists in the renderer, and the AP-WIRE-026A symbol-family split stays exactly 10/49/0 as instructed, not artificially inflated. Wire color similarly renders only when `WireSemanticResolution.wire_color_status == Resolved` (raw text always preserved even when unmapped to a display color); splices (filled) and crossings (unfilled) are kept visually and structurally distinct. A new `SvgStructureValidator` (regex-based, no XML/DOM library) confirmed on the real TRX300 output: 642/642 unique element ids, 0 dangling `data-*-id` references, 0 raster fallback, 0 guessed-symbol-family violations. Confirmed byte-identical SVG determinism across two runs, and re-verified the AP-WIRE-024 known conflict (4 conflicted endpoint reconstructions) is still never fabricated into a rendered component/color association, now through six APs (024→027). Retired the old raw-line-dump `SvgExporter` and its call site, writing to the same canonical `output/wires.svg` path rather than a second artifact tree. See `docs/AP-WIRE-027_Structured_SVG_Export.md` for the design and `docs/AP-WIRE-027_AAR.md` for the full validation, object-coverage table, and reference-image comparison.

AP-WIRE-028 — Source-vs-Model Engineering Validation / Final AAR — complete (see `docs/AP-WIRE-028_AAR.md`): 50/50 Release CTest (no code changed in this AP — validation only), fresh TRX300 extraction byte-for-byte unchanged from AP-WIRE-027. Measured how much of the actual TRX300 diagram the current model represents, rather than re-validating internal consistency. Headline finding: only **5.7% of topology edges (50/877)** are currently assembled into an endpoint-to-endpoint `Wire` object — traced by direct code reading of `WireReconstructor` to an intentional, documented scope boundary (the path walk stops at any splice/junction/crossing node rather than guessing how to route through it; a "separate semantic decomposition stage" the code comment calls for does not yet exist), not a detection failure — confirmed by node-type classification of all 877 edges and by spot-checking sample wire spans (24-53px runs, 1-2 edges each). Two further root-caused findings: the connector-recognition path (0 `ConnectorCandidate`s) is structurally unreachable because this fixture has zero `PrimitiveSymbol`-kind components, even though a real connector glyph (a small interlocking-notch "bullet connector" symbol, visually distinct from the OEP reference's larger multi-pin housings, which were confirmed absent from the original scan) is directly present in the source; and 16/106 splice + 29/237 crossing nodes fall inside the two `DiagramFurniture` table bounding boxes (grid-line intersections of a truth table, not real junctions) and currently still render as such in the SVG. Everything else - 49/59 unresolved symbol families, 115/115 unresolved labels, 6/10 unresolved net roles - was confirmed to trace to a single shared, already-known cause: zero text-recognition evidence in this deterministic baseline, not a recognition-logic defect. Re-verified the AP-WIRE-024 conflict for a seventh consecutive AP. Produced a full source-vs-model coverage table, an 11-item error classification (with responsible boundary and recommended future AP per item), and a prioritized backlog derived from what was actually measured rather than the full candidate list the handoff offered. No implementation was performed. See `docs/AP-WIRE-028_AAR.md` for the full coverage tables, per-category validation (wire/topology/endpoint/component/connector/symbol-family/label/electrical-net/SVG/reference-image), and quantitative metrics with explicit "not reliably measurable" markers where a defensible denominator doesn't exist.

AP-WIRE-029 — Conductor Boundary & Wire Identity Specification — specification established / implementation not started (see `docs/AP-WIRE-029_Conductor_Boundary_and_Wire_Identity.md`). Documentation-only AP; no extraction, topology, model, or rendering code changed. Formally defines Wire as "a uniquely identifiable physical conductor path extending from one conductor boundary to the next conductor boundary," where "next" means the next boundary along the physical conductor, never the ultimate electrical destination. Establishes physical Wire identity as strictly distinct from electrical-net connectivity (a connector terminal or component terminal is always a hard Wire boundary even where current flows electrically through it into another Wire), and states that Splices/Junctions are not automatically Wire endpoints, Crossings never establish connectivity, and Continuation nodes are traversable but not identity-proving. Documents that AP-WIRE-028's measured 50/877-edge wire-reconstruction boundary is an implementation scope limit, not the authoritative definition of Wire, and records an evidence hierarchy and explicit never-guess invariants for a future reconstruction stage to be built against. Two items are marked OPEN rather than resolved by assumption (Junction's distinct engineering meaning, if any; whether shared-conductor geometry ever needs its own physical-copper-count resolution). No AP-WIRE-030 implementation was started.

AP-WIRE-030 — Conductor Boundary / Terminal Resolution Specification — specification established / implementation not started (see `docs/AP-WIRE-030_Conductor_Boundary_and_Terminal_Resolution.md`). Documentation-only AP; no extraction, topology, endpoint, terminal, component, connector, wire-reconstruction, electrical-net, rendering, model, test, or build file changed. Specifies, for AP-WIRE-029's Conductor Boundary/Wire architecture, what evidence allows a detected geometric conductor end to be classified as a Conductor Boundary and associated with a specific engineering terminal - separating detection, component association, terminal association, terminal-type classification, conflict handling, provenance, and unresolved-evidence preservation into eight distinct operations that must never be collapsed into one. Documents a boundary taxonomy mapped onto existing `EndpointKind`/`TerminalRole`/`TerminalCandidateKind` vocabulary (no new enum introduced), evidence rules for component/connector/ground/external-connection resolution, the architectural meaning of `TerminalLead` geometry, and a 12-row decision matrix. States plainly that existing `TerminalRecognizer` distance/alignment behavior is historical implementation behavior, not automatically blessed as final architecture, and requires that the 4 existing AP-WIRE-024 conflicts be preserved, not resolved, by any future implementation. Explicitly prohibits topology mutation, Wire-reconstruction logic, and electrical-net-driven boundary inference, and hands the branch-pairing/Wire-assembly question to AP-WIRE-031 without prescribing it. Three items marked OPEN. No AP-WIRE-031 implementation was started.

AP-WIRE-030 (implementation) — Conductor Boundary / Terminal Resolution — implemented and validated (see `docs/AP-WIRE-030_AAR.md`): 51/51 Release CTest, fresh TRX300 extraction confirms every AP-WIRE-028 structural invariant byte-for-byte unchanged (294/692/877/210/40/10/0/30). Adds `ConductorBoundaryResolver`, a read-only layer that classifies each of the 210 endpoints' Conductor Boundary (reusing existing `EndpointKind` values, no new enum) and independently tracks component/terminal/connector/connector-terminal/ground/external status - the one representational gap `EndpointSemanticReconstruction`'s single collapsed status could not close, per an explicit Model-Change Gate report the user approved before `ConductorBoundaryEvidence`/`ConductorBoundaryResolution`/`ConductorBoundaryCoverage` were added (additive only, no existing struct changed). On this baseline: 36/210 boundaries resolve (29 `ComponentTerminal` + 7 `Ground`), 0/210 get a specific terminal identifier and 0/210 get a connector association (both correctly, given already-traced zero-evidence root causes), and all 4 AP-WIRE-024 conflicts remain explicitly `Conflicted` with no winner selected - for two of them, the finer-grained evidence reveals a cross-category component-vs-ground conflict rather than a same-category one, a more precise diagnosis, not a resolution. Confirmed byte-identical determinism across two runs and zero effect on topology/Wire/electrical-net structures by construction (the resolver's signature takes no `TopologyNode`/`TopologyEdge`/`ElectricalNet` parameter at all). See `docs/AP-WIRE-030_AAR.md` for the full coverage tables, conflict-preservation detail, and three items still marked OPEN. No AP-WIRE-031 implementation was started.

AP-WIRE-031 — Physical Wire Identity Reconstruction — implemented and validated (see `docs/AP-WIRE-031_AAR.md`): 52/52 Release CTest, fresh TRX300 extraction confirms every non-wire AP-WIRE-028 structural invariant byte-for-byte unchanged (294/692/877/210/10/0/30). Adds `PhysicalWireIdentityReconstructor`, the pipeline's new sole authoritative Wire producer: runs the existing `WireReconstructor` unchanged for the conservative degree-1/Continuation-only case (marked Resolved), then extends physical Wire identity through a Splice/Junction/Crossing node only when explicit conductor-segment-sharing evidence justifies it - the one form of physical-continuity evidence AP-WIRE-029/030 permit - gated additionally on both endpoints carrying an AP-WIRE-030 Resolved boundary. A genuine evidence contradiction (one segment matching two or more other incident edges) produces every candidate continuation marked `Conflicted`, never a picked winner. `Wire` gained exactly two user-approved fields (`identity_status`, `identity_evidence_ids`); `WireReconstructor` and `DistributionDecomposer`/`ElectricalNetResolver` are code-unchanged. On TRX300: exactly one new wire discovered (40→41), a 14-edge path through both a splice and a crossing backed by 3 shared-segment evidences, everything else unchanged including all 4 AP-WIRE-024 conflicts, determinism, and downstream AP-WIRE-025/validation/SVG consumers (all unmodified, all still function against the new wire with no code changes needed in any of them). See `docs/AP-WIRE-031_AAR.md` for the full physical-continuity rules, splice/junction/crossing/continuation behavior, and known limitations.

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

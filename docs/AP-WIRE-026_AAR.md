# AP-WIRE-026 — After-Action Report

Unified Engineering Diagram Reconstruction

Status: **complete and validated**, with an explicit, evidence-grounded
answer to the architecture-assessment question in §11 (not a bare "yes").

## 1. Baseline

Pre-implementation commit: `3cd117f` (immediately after the accidental
force-push was recovered via merge and the reference image incorporated).

## 2. Final commit

`24c9fb3` (implementation + export). This AAR and the design doc are
committed immediately after.

## 3. Build environment

- Compiler: GCC 13.3.0 (this validation environment; the project targets
  MSVC on Windows, GCC/Clang elsewhere - no compiler-specific code was
  added in this AP)
- CMake: as configured by the repository's `CMakeLists.txt`
- **OpenCV: 5.1.0**, built from source at `/usr/local`
  (`-DOpenCV_DIR=/usr/local/lib/cmake/opencv5`). This environment's
  package manager only offers 4.6; the codebase requires OpenCV 5 (uses
  `opencv2/geometry/2d.hpp`, an OpenCV-5-only header) and this AP did not
  touch that requirement. No OpenCV 4 target, downgrade, or validation
  occurred.
- libcurl 8.5.0 (unchanged; this AP added no network dependency)

## 4. Test results

**47/47 Release CTest passed, 0 failed** (46 from the AP-WIRE-025
baseline + `dx-wire-test-engineering-diagram-builder`, 25 cases). Total
test time 0.24-0.26s across repeated runs.

## 5. Fresh TRX300 extraction

| Metric | AP-WIRE-025 baseline | AP-WIRE-026 | Δ |
|---|---:|---:|---:|
| Conductor segments | 294 | 294 | 0 |
| Topology nodes | 692 | 692 | 0 |
| Topology edges | 877 | 877 | 0 |
| Endpoints | 210 | 210 | 0 |
| Components | 109 | 109 | 0 |
| Connectors | 0 | 0 | 0 |
| Connector terminals | 0 | 0 | 0 |
| Wires | 40 | 40 | 0 |
| Electrical nets | 10 | 10 | 0 |
| Validation errors | 0 | 0 | 0 |
| Validation warnings | 30 | 30 | 0 |

Every required invariant held exactly. `EngineeringDiagramBuilder::build`
takes `WireModel` by `const&` and is invoked only after
`ExtractionArtifactWriter` has already written every other artifact, so
by construction it cannot have influenced any of the numbers above -
confirmed empirically by this unchanged table.

## 6. Unified model counts

| Object | Count |
|---|---:|
| `DiagramComponent` | 109 |
| — with resolved canonical identity | 0 (no text-recognition evidence in this baseline - see §9) |
| — with a symbol-geometry reference | 59 (exactly the AP-WIRE-023 real-candidate count) |
| — with ≥1 terminal candidate | 30 |
| — with ≥1 resolved endpoint association | 28 |
| `DiagramConnector` | 0 (TRX300 has none post-furniture-fix, per the AP-WIRE-022A AAR) |
| `DiagramWire` | 40 |
| — with a wire-semantic-resolution reference | 40 / 40 (every wire) |
| `DiagramSplice` | 106 (matches `topology_node_types.splice` exactly) |
| `crossing_node_ids` | 237 (matches `topology_node_types.crossing` exactly) |
| `DiagramLabel` | 115 (one per `TextRegion`, unconditionally) |
| — resolved | 0 (no text-recognition evidence in this baseline) |
| — with raw text | 0 (same reason) |
| `DiagramElectricalNet` | 10 |

`ComponentSymbolGeometry`/`SymbolPrimitive` and `WireSemanticResolution`
counts are unchanged from the AP-WIRE-023/025 baselines (17 components
with primitives, 40 primitives, 40 wire-semantic resolutions) - they are
referenced by id from the diagram, not duplicated, so there is nothing
new to count on their side.

## 7. Relationship validation

| Metric | Count |
|---|---:|
| Valid references | 415 |
| Invalid references | 0 |
| Duplicate relationships | 0 |
| Orphan objects | 0 |

Zero invalid references across all 9 checked categories (wire→endpoint
×2, wire→topology-edge, terminal→endpoint, terminal→component,
connector-terminal→connector, symbol-geometry→component,
primitive→component, net→endpoint, semantic-resolution→wire) confirms
every prior AP's output remains internally consistent when
cross-referenced independently. Zero duplicates and zero orphans on this
fixture - see the note in §12 about the orphan check's limited exercise
on this particular fixture.

## 8. State preservation

Directly verified (not inferred from aggregate counts):

- **Unresolved identity preserved**: a component with no
  `ComponentIdentityCanonicalization` entry reports
  `identity_status: unresolved`, `canonical_name: ""` (test case 15;
  confirmed on all 109 real-run components, since 0 have resolved
  identity in this baseline - see §9).
- **Conflicted identity preserved**: a `Conflicted` canonicalization
  status maps to `DiagramObjectStatus::Conflicted` with `canonical_name`
  left empty, never a guessed name (test case 16).
- **Unresolved terminal preserved**: an endpoint with no
  `component_id` (i.e. `EndpointSemanticReconstructionStatus::Unresolved`)
  never appears in any component's `endpoint_ids` (test case 17).
- **Conflicted terminal preserved**: an endpoint whose reconstruction was
  `Conflicted` (per AP-WIRE-019, `component_id` cleared upstream) is
  structurally impossible to fabricate into a component association,
  since the builder only ever reads `EndpointCandidate.component_id`,
  which is empty in that case (test case 18).
- **Unresolved wire semantics preserved**: `DiagramWire` references
  `WireSemanticResolution` by id only; the resolution's own
  `Resolved`/`Unresolved`/`Conflicted` per-field statuses (AP-WIRE-025)
  are untouched by this AP - the diagram never re-derives or overrides
  them.
- **AP-WIRE-024 known conflict preserved, checked directly against the
  real extraction** (not a synthetic test - see §8a below): the specific
  documented conflicted endpoint (`endpoint-candidate-b0e3d6bb622a227c`,
  from `docs/AP-WIRE-024_AAR.md` §7) does not appear in any
  `DiagramComponent.endpoint_ids` across the full 109-component,
  115-label, 40-wire real TRX300 diagram.

### 8a. Direct check against the real extraction (not just the unit test)

```
component-candidate-shape-region-845947b0afd7451a in diagram: True
endpoint-candidate-b0e3d6bb622a227c in that component's endpoint_ids: False
endpoint-candidate-b0e3d6bb622a227c fabricated into ANY component: False
wire-430f59213afaef3a wire_semantic_resolution_id: wire-semantic-resolution-54b8e2cd8967326d
```

This is the same wire/endpoint pair documented in the AP-WIRE-024 and
AP-WIRE-025 AARs, now traced one layer further into the unified model and
still correctly unresolved.

## 9. Determinism

Ran two independent fresh extractions
(`dx-extract extract samples/trx300ODG.png --output <dir>`, separate
output directories) and diffed
`artifacts/engineering_diagram/engineering_diagram.json` between them:

```
diff <run1>/engineering_diagram.json <run2>/engineering_diagram.json
exit code: 0 (byte-identical, zero diff lines)
```

**Byte-equivalent deterministic output confirmed.** The exporter has no
timestamp field (unlike `review_manifest.json`), so this is a true
byte-for-byte comparison, not "structurally equivalent modulo metadata."

## 9b. Semantic resolution baseline note

Wire-color/function/identity/label resolution are 0 in this run's counts
(§6) for the same reason documented in the AP-WIRE-025 AAR: this
validation used the deterministic extraction path
(no `--recognition`/`--vision-recognition`), matching every prior AP's
baseline methodology for comparability. This is a property of the
*evidence available in this run*, not a defect in `EngineeringDiagram` -
the fields and references exist and are correctly wired up (verified by
the unit tests, which do exercise Resolved/Conflicted cases with
synthetic evidence). A `--vision-recognition` run would be expected to
populate many of these; validating that combination is future work, not
part of this AP's baseline comparison.

## 10. Reference-image analysis

| Reference feature | Existing model (pre-026) | Unified model (026) |
|---|---|---|
| Component body/bounds | present (`ComponentCandidate.bounds`) | represented (`DiagramComponent.bounds`) |
| Component internal symbol geometry | present (AP-WIRE-023 `SymbolPrimitive`) | represented (`symbol_geometry_id` reference, not duplicated) |
| Component label (text) | present only when recognition evidence exists | represented (`semantic_labels`); 0 resolved on this baseline - evidence gap, not a model gap |
| **Component symbol-family identity** (switch/relay/motor/lamp/diode/alternator/battery/starter as *distinct engineering types*) | **absent** - no stage assigns this; AP-WIRE-021 only produces a coarse geometric bucket (`CircularSymbol`/`Enclosure`/`ChassisGround`/`PrimitiveSymbol`) | **not representable** - there is no upstream evidence to reference. This is the one real, named gap; see §11 |
| Connector body | present (empty on TRX300 post-furniture-fix) | represented (`DiagramConnector`, 0 instances) |
| Connector terminals | present (empty) | represented (0 instances) |
| Terminal numbering / pin codes | present as `EndpointCandidate.terminal_name`/`ConnectorTerminal.terminal_name`, only when recognition evidence exists | representable via `component → terminal_candidate_ids → TerminalCandidate.endpoint_id → EndpointCandidate.terminal_name` (a 2-hop join by design, not duplicated); 0 resolved on this baseline |
| Colored conductors | present (`WireSemanticResolution.wire_color`, AP-WIRE-025) | represented directly (`DiagramWire.wire_semantic_resolution_id`); 0/40 resolved on this baseline (evidence gap) |
| Endpoint-to-endpoint wire routing | present (`Wire` + topology edges + conductor segments) | represented; actual polyline geometry reached via `conductor_segment_ids` → `ConductorSegment.geometry`, not duplicated |
| Shared conductors | present only as an aggregate coverage-diagnostic count (`CONDUCTOR-SHARED`, AP-WIRE-022A) | **improved**: now a first-class, directly queryable relationship (`DiagramSplice.incident_wire_ids`) |
| Splices/junctions | present (`TopologyNode` type) | represented, with the new incident-wire linkage above |
| Grounds | present (`ChassisGround` kind, `Ground` endpoint kind, net role) | represented across all three |
| Electrical nets | present | represented, plus a derived `wire_ids` cross-reference that did not exist before (physical/electrical layers kept distinct per the AP's explicit requirement) |
| Unknown/unresolved text | present but not uniformly collected anywhere | **improved**: `DiagramLabel` unconditionally represents every `TextRegion`, including ones with zero recognition evidence - "do not discard text merely because it cannot currently be interpreted" is now structurally guaranteed, not just a stated intent |
| Unresolved/conflicted state generally | present per-subsystem, in different shapes per AP | **unified**: `DiagramObjectStatus` gives one consistent three-state vocabulary across identity, terminals, and (implicitly, via reference) wire semantics, without collapsing any of them to a boolean |
| Provenance | present per-subsystem (`ComponentIdentityEvidence` chain) | represented explicitly (`identity_evidence_ids` traces canonicalization → resolution → raw evidence ids) |
| Page-level coherent auto-routed layout (as drawn in the reference image) | absent | **intentionally absent** - out of scope per the AP's own instruction; belongs to a future rendering/layout stage, not the engineering model |

## 11. Architecture assessment

**"Can AP-WIRE-027 now consume `EngineeringDiagram` as its source of truth
without rediscovering engineering meaning from the source image?"**

**Partially yes, with one explicitly named exception.**

For topology, wire identity, splice/shared-conductor relationships,
electrical-net membership, component-terminal-connector associations,
symbol geometry, labels, and evidence/status/provenance, the answer is
**yes**: every relationship a renderer would need is already a reference
in the model, cross-validated (§7), and deterministic (§9). AP-WIRE-027
would not need to re-parse the source image or re-run any recognition
logic to draw a wire in the right color (once resolved), connect it to
the right terminal (once resolved), or know that two wires share a
splice.

The **named exception**: AP-WIRE-027 cannot yet know *what electrical
symbol to draw* for a component beyond its coarse geometric bucket. The
reference image draws a lamp filament, a switch lever, a coil zigzag, a
motor "M", a diode arrow, an alternator winding, and a battery block as
visually and semantically distinct symbols - `EngineeringDiagram` can only
say "this is a `CircularSymbol` with these internal primitives" or "this
is an `Enclosure`." No upstream AP (021 through 025) ever established
true symbol-family identity, and per the task's own explicit prohibition
("a component symbol does NOT mean component identity may be inferred
merely from visual similarity"), `EngineeringDiagram` correctly does not
invent one. **This is not something AP-WIRE-026 should have solved** - it
is a missing *upstream* capability (a symbol-family classification AP,
consuming AP-WIRE-023's primitive geometry with real evidence, not this
assembly layer), and AP-WIRE-027 will need either that capability or an
explicit fallback rule (e.g. render `Unknown`/`GeometricallyClassified`
components as a generic bounded box, exactly as the current source-image
review layers already do) until it exists.

## 12. Known limitations / scope boundary

Explicitly outside this AP, by design:

1. **Symbol-family identity** (§11) - not a 026 gap, a genuinely missing
   upstream capability.
2. **Page-level auto-layout** - the reference image's clean grid
   arrangement and orthogonal wire routing are rendering-stage concerns;
   `EngineeringDiagram` only carries source-page coordinates.
3. **Terminal numbering/wire-color/label resolution on this baseline** are
   0 because this validation used the deterministic (no-recognition)
   path for comparability with every prior AP's baseline - not because
   the model lacks the fields.
4. **The orphan-object check is under-exercised on TRX300**: because
   AP-WIRE-023 gives every real (non-furniture) component a
   `ComponentSymbolGeometry` entry unconditionally (even when it contains
   zero primitives), the "zero terminals AND zero endpoints AND zero
   symbol geometry" orphan condition can never fire on this fixture. The
   check is correct and will catch a genuine future case (e.g. a
   component AP-WIRE-023 skipped entirely), but TRX300 itself provides no
   positive test of it beyond the unit tests' synthetic cases.
5. No new review-image (PNG) layer was added, per the AP's own
   instruction that visual fidelity belongs to later stages.
6. `EngineeringDiagram` does not attempt to resolve, tie-break, or
   otherwise act on any `Conflicted`/`Unresolved` state it finds - it
   only reports it, per the "never solve missing information by letting
   a later stage rediscover it" principle applied to the model's own
   scope: this AP represents, it does not additionally resolve.

## Verdict

AP-WIRE-026 is **validated**. It establishes a real, deterministic,
reference-preserving unified model with zero structural drift from the
AP-WIRE-025 baseline, zero invalid/duplicate/orphan relationships across
415 cross-checked references, byte-identical determinism across repeated
runs, and directly-verified preservation of every unresolved/conflicted
state including the specific AP-WIRE-024 known conflict traced through
three AARs now. The one substantive gap (symbol-family identity) is named
explicitly rather than glossed over, with a clear statement of whose
responsibility it is. AP-WIRE-027 was not started.

# AP-DIAG-041 — Residual Splice Object-Mask Diagnostic

Status: IMPLEMENTED — diagnostic only; production extraction logic unchanged

## 1. Objective

Determine whether the current residual zero-wire population is caused, in whole or in part, by graphical object boundary geometry entering conductor topology.

The independent visual review of AP-DIAG-038 found no source-evidenced branch taps at the 35 terminal-Splice locations. It instead identified two dominant visual artifact classes: connector/component-body geometry coincident with conductor-like ink, and ordinary line crossings without a junction marker.

This AP therefore tests a narrow counterfactual before any Wire-identity change:

> What changes when established component-enclosure and connector-boundary geometry is excluded from the conductor detector before topology is built?

The AP does not decide that such masking is the correct production behavior. It measures the structural effect and preserves an auditable baseline.

## 2. Baseline

Source: `samples/trx300ODG.png`

Expected source dimensions: `898 x 549`

The diagnostic first runs the canonical `ExtractionPipeline` with the default configuration to establish the same-run baseline model.

The current live production baseline at AP start was `extraction-results` commit `0a1e3299c2e914eef32e842f3d494ffbb1e3b79b`.

## 3. Diagnostic implementation

Executable: `dx-audit-residual-splice-object-mask`

Source: `tools/ap_diag_041_residual_splice_object_mask.cpp`

Default invocation from the repository root:

```text
.\build\Release\dx-audit-residual-splice-object-mask.exe samples/trx300ODG.png artifacts/audit
```

Optional arguments:

```text
<image> <output-dir> <boundary-mask-thickness> <endpoint-match-radius>
```

Defaults: boundary mask thickness `2` px; endpoint match radius `8` px.

## 4. Counterfactual stages

For each scenario, the diagnostic reproduces the existing early production conductor/topology path using the canonical default configuration:

1. ImageLoader / ImageNormalizer
2. ShapeDetector
3. TextRegionDetector
4. MorphologyWireDetector
5. GeometryOwnershipClassifier
6. GroundApproachConductorRecovery
7. ConductorEvidenceEvaluator
8. ConductorNormalizer
9. TopologyReconstructor
10. GapInterpreter
11. EndpointReconstructor
12. WireReconstructor

No production class is modified by this tool.

## 5. Object masks

Two deterministic counterfactual masks are generated from the baseline model.

### Scenario A — established_object_boundary_ring

Adds a thin rectangle ring around `ComponentCandidateKind::Enclosure` component bounds and all currently recognized `ConnectorCandidate` bounds. The ring is `2 px` thick by default.

The experiment deliberately excludes circular primitives, chassis-ground symbols, diagram furniture, and arbitrary proximity regions. It is therefore not a blanket erase-everything-near-a-component test.

### Scenario B — established_object_bounds_filled_upper_bound

Fills the same enclosure/connector bounding boxes. This is explicitly an upper-bound stress test, not a proposed production algorithm. It can remove legitimate terminal-lead pixels inside a body and is included only to determine whether even an aggressive object exclusion would materially alter the residual population.

## 6. Residual-endpoint attribution

The diagnostic reconstructs the same residual endpoint population used by AP-DIAG-038: an endpoint is residual when no production Wire references it; Splice and Unresolved endpoint kinds are excluded; only degree-1 endpoint candidates participate.

It then replays the same segment-sharing walk used by AP-DIAG-038/039 to identify the terminal Splice node for each residual endpoint and deduplicates by that node. This independently establishes the 35-terminal-Splice population rather than importing a manually typed coordinate list.

## 7. Counterfactual resolved definition

This AP does not claim that its counterfactual is the authoritative `PhysicalWireIdentityReconstructor`.

Instead, a baseline residual endpoint is considered structurally resolved in a counterfactual only when: (1) a counterfactual EndpointCandidate exists within the configured spatial match radius, and (2) exactly one matched counterfactual endpoint participates in a `WireReconstructor` Wire. More than one matching wire endpoint is reported as ambiguous.

For terminal-Splice reporting: `fully_resolved` means every residual endpoint assigned to that terminal Splice resolves; `partially_resolved` means at least one endpoint resolves or is ambiguous but the full set does not; `unresolved` means none resolves.

## 8. Outputs

The tool writes `artifacts/audit/AP-DIAG-041_residual_splice_object_mask.json` plus two mask images: `established_object_boundary_ring_mask.png` and `established_object_bounds_filled_upper_bound_mask.png`.

The JSON contains canonical baseline counts, exact terminal-Splice records, current component/connector overlap attribution, mask parameters, counterfactual topology counts, counterfactual WireReconstructor counts, residual endpoint resolution counts, and terminal-Splice resolution counts.

## 9. Interpretation rules

### Outcome A

A meaningful number of the 35 terminal-Splice locations becomes structurally resolved under the boundary-ring mask, with the affected endpoints spatially coherent around the object boundary. This supports an upstream graphical-object contamination defect.

### Outcome B

The filled upper-bound mask changes the population substantially but the narrow boundary-ring mask does not. This indicates that the effect depends on broad body exclusion and is not yet evidence for a safe perimeter-only production fix.

### Outcome C

Neither mask materially changes the residual population. The current object-boundary hypothesis is not sufficient to explain the residual population. The next investigation should remain focused on topology formation, terminal association, or recognition evidence rather than introducing object masking.

### Outcome D

A mask produces new wires, but the new wires are visibly or structurally inconsistent with endpoint-to-endpoint engineering identity. Those wires are diagnostic artifacts and must not be promoted.

## 10. Architectural observation

The existing pipeline already excludes recognized component geometry through `GeometryOwnershipClassifier` before `TopologyReconstructor`.

However, `ConnectorGeometryDetector` is currently invoked after `TopologyReconstructor`, so connector-native recognition cannot itself prevent connector-body ink from participating in conductor topology on that same run.

Therefore, if the counterfactual shows a connector-boundary effect, the likely defect is an upstream recognition/order/data-flow gap, not a missing Wire-branching rule.

The experiment must also distinguish recognized connectors from connector-like graphics that were never recognized as `ConnectorCandidate`s. The current TRX300 baseline contains only two recognized connector candidates, so their mask is not a complete representation of every connector-shaped object visible in the source.

## 11. Scope exclusions

This AP does not change `PhysicalWireIdentityReconstructor`, add branch decomposition, promote Splice nodes to Wire endpoints, alter ElectricalNet resolution, change connector recognition thresholds, change component classification, change the source image, or assert that all 35 residual locations are caused by connector/component geometry.

## 12. Completion criteria

AP-DIAG-041 is complete when the tool has been compiled and run against the canonical TRX300 source and the generated JSON is reviewed for exact 35-terminal-Splice accounting, exact residual endpoint accounting, baseline-vs-counterfactual topology deltas, endpoint and terminal-Splice resolution deltas, recognized-object attribution, and absence of unsupported production conclusions.

A production implementation AP must not begin until those measurements are reviewed.

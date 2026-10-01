# AP-DIAG-024 — Crossing-Line Probe Forensic Comparison

## Status

**DIAGNOSTIC — FORENSIC EXTRACTION COMPLETE; DISCRIMINATOR NOT YET IDENTIFIED**

## Objective

Determine why AP-DIAG-023's adaptive probe rejected only 4 of the 21 known switch-matrix grid-gap false positives while preserving all 5 known genuine circular symbols.

## Baselines

- AP-DIAG-019 root-cause population: 21 false circular candidates + 5 genuine circular symbols.
- AP-DIAG-020 implementation baseline: `72feb34b91d45b956d7a5bf38d38ada664de79df`.
- AP-DIAG-023 production implementation: `dd5dc209d35786d6d36ffe54745ec9726f771c21`.
- AP-DIAG-023 extraction-results commit: `3119d99c067a72b3067ca2fac267b1e48f2d4a23`.
- Source: `samples/trx300ODG.png`, 898x549.

## Observed AP-DIAG-023 result

The current extraction contains 22 circular candidates versus 26 in the AP-DIAG-019 baseline.

- 4 of 21 known false candidates were rejected.
- 17 of 21 known false candidates remain.
- 5 of 5 known genuine circular symbols remain.
- Wires: 37 -> 37.
- Topology edges: 643 -> 643.
- Electrical nets: 12 -> 12.
- Terminal candidates: 18 -> 18.
- Connectors: 2 -> 2.
- Connector terminals: 1 -> 1.
- Validation errors: 0 -> 0.
- Runtime warning count remains 34.

The four known false candidates rejected by AP-DIAG-023 are at:
- (411,346), 10x9
- (477,304), 7x9
- (516,199), 10x10
- (516,209), 10x11

All five previously confirmed genuine circular symbols remain represented at:
- (511,448), 11x13
- (357,462), 17x16
- (761,361), 21x17
- (77,83), 18x24
- (90,98), 20x21

## Forensic finding

The AP-DIAG-023 change is correctly isolated to the probe-distance calculation:

`far = max(1, min(config.circle_crossing_probe_distance, max(bounds.width, bounds.height)))`

The existing continuity test then samples four side extensions. It does not inspect topology, connected-component identity, local grid periodicity, or the relationship between the four sides.

This explains the partial result: candidate scale is relevant, but it is not sufficient to characterize the entire false-positive population.

The surviving false-positive population spans the same candidate-scale range identified by AP-DIAG-019:

- 7x7 through 22x20.
- Known survivors include 7–11 px cells as well as larger 19x18 and 22x20 cells.
- Therefore a smaller fixed probe distance would be unjustified as a general solution.

## Candidate comparison

| Location | Size | AP-DIAG-023 |
|---|---:|---|
| 485,249 | 22x20 | survives |
| 618,249 | 19x18 | survives |
| 532,448 | 11x13 | survives |
| 420,259 | 11x9 | survives |
| 507,209 | 9x11 | survives |
| 466,190 | 11x9 | survives |
| 561,468 | 8x10 | survives |
| 421,249 | 10x10 | survives |
| 517,189 | 9x10 | survives |
| 526,304 | 9x9 | survives |
| 806,434 | 7x9 | survives |
| 181,329 | 9x9 | survives |
| 843,434 | 8x9 | survives |
| 357,69 | 9x9 | survives |
| 344,69 | 9x8 | survives |
| 806,355 | 7x7 | survives |
| 712,434 | 7x6 | survives |
| 516,209 | 10x11 | **rejected** |
| 516,199 | 10x10 | **rejected** |
| 477,304 | 7x9 | **rejected** |
| 411,346 | 10x9 | **rejected** |

The four rejected cases do not form a sufficiently distinct size class. The 7x9 and 10x11 rejected candidates coexist with 7x9 and 10x11 survivors elsewhere in the source.

## Code-level conclusion

The current `bounded_by_crossing_lines()` predicate is a geometric side-continuity test, not a complete grid-gap classifier.

Its evidence is:

1. Measure the two horizontal extensions on the top side.
2. Measure the two horizontal extensions on the bottom side.
3. Measure the two vertical extensions on the left side.
4. Measure the two vertical extensions on the right side.
5. Take the weakest of those eight measurements.
6. Reject only if the weakest value reaches the configured continuity threshold.

AP-DIAG-023 changed only the extension distance. It did not change what constitutes continuity.

The 4/21 improvement therefore confirms that probe scale participates in the failure, but the 17 survivors demonstrate that another geometric property controls the remaining cases.

## Real extraction forensic results

The instrumented extraction was published at extraction-results commit `2e29a392f3782f29e397388dbb07779f26fc2736`.

The audit contains 22 circular candidates: 17 known false positives and 5 known genuine circular symbols. The four AP-DIAG-023 rejected false candidates are absent from the accepted-circle evidence set because forensic fields are emitted only for accepted Circle candidates.

Measured population ranges show no single currently exposed scalar separates the two classes:

| Measurement | 5 genuine | 17 false survivors |
|---|---:|---:|
| Probe distance | 13–24 | 7–22 |
| Weakest side | 0–0.0476 | 0–0.3333 |
| Circularity | 0.6807–0.9194 | 0.7349–0.9273 |
| Aspect ratio | 1.05–1.3333 | 1.00–1.2857 |
| Edge support | 0.6944–1.0 | 0.7083–1.0 |
| Interior density | 0–0.24 | 0–0.28 |

All five genuine candidates have probe distance >= 13, but three false survivors also do. Weakest-side <= 0.05 contains all five genuine candidates but also seven false survivors.

The strongest evidence against another scalar threshold is the pair at `(511,448)` and `(532,448)`: both are 11x13 candidates with weakest-side continuity 0, while the first is known genuine and the second is known false. Their circularity and edge-support values also overlap substantially. The remaining discriminator therefore requires contextual/local evidence rather than another global size or continuity threshold.

Collateral populations remain unchanged: wires 37, topology edges 643, electrical nets 12, terminal candidates 18, connectors 2, connector terminals 1, validation errors 0, and validation warnings 34.

## What remains undetermined

The structured extraction artifact now exposes aggregate side-continuity measurements and circle metrics, but still does not expose individual corner measurements, local horizontal/vertical run lengths, or candidate-local raster crops required to identify the common property of the remaining 17 false positives.

Therefore this AP does **not** infer a new threshold, morphology rule, or detector criterion.

The next diagnostic evidence required is a per-candidate raster probe record containing:

- candidate bounding box;
- contour area and perimeter;
- circularity;
- aspect ratio;
- enclosing-circle center/radius;
- edge support;
- interior density;
- actual adaptive probe distance;
- top-left/top-right/bottom-left/bottom-right continuity values;
- weakest-side value;
- local horizontal/vertical run lengths immediately outside each side;
- candidate-local raster crop.

The five genuine circles must be included in that same record set so the eventual discriminator is derived from both populations.

## Evidence instrumentation

AP-DIAG-024 adds additive forensic fields to circular ShapeRegion and ComponentCandidate records. For every accepted Circle candidate, the extraction audit can now retain:

- adaptive probe distance;
- top, bottom, left, and right continuity values;
- weakest-side continuity;
- circularity;
- aspect ratio;
- enclosing-circle radius;
- edge support;
- interior density.

These fields are observational only. They do not alter the Circle acceptance decision.

Implementation commits:

- model evidence fields: `bf215bd703cde2080ba7dbab741f46fd43620f91`
- ShapeRegion evidence capture: `d572732647059e24684282eaae73984efd122003`, `76f953739b88aa14ce13d98dd46bb0b44cff0ff3`, `28cc01c5f1694e338938a1d8a0668bd536ff813a`
- candidate propagation: `01d1ab09753a1458a354ef6fdb18cf16aaabeb0f0`
- audit serialization: `237123ecf600a73290f78a2579f7ad006b4be85f`
- regression serialization assertions: `0596faddbe7d8d21113069772c42f49cab7d2a33`

## Disposition

**AP-DIAG-024 remains diagnostic and open.** The real extraction is complete, but the evidence does not justify another detector tuning change. The next diagnostic increment should expose corner-level probe measurements and candidate-local raster context. No production threshold should be changed from the current evidence.

No detector threshold, morphology, topology, wire, endpoint, connector, or net-resolution behavior was intentionally changed by this AP. The existing AP-DIAG-023 classification predicate remains unchanged; the new data exists solely to expose its measurements for forensic comparison.

The evidence does **not** justify another tuning change yet.

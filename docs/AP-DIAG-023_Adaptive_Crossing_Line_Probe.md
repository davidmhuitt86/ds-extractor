# AP-DIAG-023 — Adaptive Crossing-Line Probe for Tight Grid Cells

## Status

**IMPLEMENTED — LOCAL EXTRACTION VALIDATION PENDING.**

## Baseline

- Baseline implementation: `72feb34b91d45b956d7a5bf38d38ada664de79df` (AP-DIAG-020).
- Source: `samples/trx300ODG.png`, 898×549, page 0.
- AP-DIAG-019 established the root cause: `ShapeDetector::bounded_by_crossing_lines` uses a fixed 25px probe distance that is too large for the tightly-spaced switch-matrix candidates.
- The affected false-positive circular candidates have bounding boxes approximately 7–22px in their relevant dimensions.
- AP-DIAG-019 measured the existing guard returning false for all 21 false-positive grid-cell candidates, with weakest-side continuity 0.00–0.44, below the 0.5 threshold.

## Scope

This AP changes only the probe-distance interpretation inside `bounded_by_crossing_lines`.

It does **not** change:

- circularity thresholds
- aspect thresholds
- radius thresholds
- edge-support thresholds
- interior-density thresholds
- continuity threshold
- contour generation
- circle classification
- component promotion
- wire detection
- topology
- endpoint reconstruction
- net resolution

## Root Cause

The existing implementation used:

`far = circle_crossing_probe_distance = 25px`

for every candidate regardless of candidate scale.

For a tightly-spaced grid cell whose bounding box is substantially smaller than 25px, the probe can extend beyond the local grid geometry. The continuity measurement therefore evaluates unrelated or absent geometry rather than the immediate continuation of the four lines bounding the candidate.

## Implementation

The configured 25px value remains the **maximum** probe distance.

The actual probe is now:

`min(config.circle_crossing_probe_distance, max(bounds.width, bounds.height))`

with a minimum of 1px.

Thus:

- large candidates retain the existing configured ceiling;
- a 7px candidate probes at 7px;
- a 10px candidate probes at 10px;
- a 22px candidate probes at 22px;
- no candidate can probe farther than the configured 25px ceiling.

The existing four-sided continuity test and 0.5 threshold remain unchanged.

## Regression Test

Added a tight-grid synthetic crossing fixture to `tests/test_shape_detector.cpp`.

The fixture uses:

- a 10px enclosed cell;
- crossing conductors;
- only 12px of local continuation beyond the cell;
- the same four-sided continuation concept used by the production guard.

The fixture asserts that the candidate is rejected as a Circle.

The test is specifically intended to exercise the failure mechanism identified by AP-DIAG-019: the historical 25px probe exceeds the local geometry while the adaptive probe remains within the candidate's scale.

## Validation

The repository changes have been committed for local execution.

Required execution gate:

1. Clean Release build.
2. Full Release CTest suite.
3. Real TRX300 extraction.
4. Compare the resulting circular/component populations against the AP-DIAG-019 baseline.
5. Verify that genuine circular symbols remain recognized.
6. Verify that the 21 grid-cell false-positive population is reduced without collateral changes.

This AP is **not closed until the local extraction is executed and the before/after populations are inspected**.

## Expected Result

The expected targeted effect is:

- reduce the 21 switch-matrix grid-cell false-positive circular candidates;
- preserve the real circular-symbol population;
- leave wires, endpoints, topology, nets, and unrelated detector populations unchanged unless a directly demonstrated downstream consequence occurs.

No claim of successful TRX300 extraction improvement is made until that execution is performed.

## Production Implementation

Production file changed:

`src/image/shape_detector.cpp`

Configuration documentation changed:

`include/eke_dx_wire/image/shape_detector.hpp`

Regression test changed:

`tests/test_shape_detector.cpp`

Production implementation commit:

`dd5dc209d35786d6d36ffe54745ec9726f771c21`

## Next Step

Run the clean Release test gate and real TRX300 extraction.

If the 21 false positives are rejected while genuine circles remain intact, close AP-DIAG-023.

If the adaptive probe produces collateral rejection, stop and investigate the geometry before changing additional thresholds or detector stages.

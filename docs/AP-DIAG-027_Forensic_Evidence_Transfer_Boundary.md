# AP-DIAG-027 — Forensic Evidence Transfer Boundary

## Status

**IMPLEMENTED — awaiting local Release extraction verification**

AP-DIAG-027 isolates the AP-DIAG-026 anomaly in the forensic evidence transfer path. No Circle classifier predicate, threshold, contour rule, or topology behavior is changed.

## Finding

The published AP-DIAG-026 extraction contained non-zero raw outward-run measurements for Circle candidates while the four derived AP-DIAG-025/026 fields remained at their default values:

- `circle_probe_long_run_count = 0`
- `circle_probe_max_run_fraction = 0`
- `circle_local_horizontal_line_density = 0`
- `circle_local_vertical_line_density = 0`

This was inconsistent with the implementation of `measure_crossing_line_probe()`. The raw run values were already present, proving the measurement function was executing and at least part of its result was reaching the published model.

## Root Cause

The defect was found in `ShapeDetector::add_region()`.

`detect_circles()` populated all four AP-DIAG-025/026 fields on its local `forensic_region`:

- `circle_probe_long_run_count`
- `circle_probe_max_run_fraction`
- `circle_local_horizontal_line_density`
- `circle_local_vertical_line_density`

However, `add_region()` copied the earlier AP-DIAG-024/024B forensic fields into the newly constructed `ShapeRegion` and stopped after:

```cpp
region.circle_local_density_7x7 = forensic->circle_local_density_7x7;
region.circle_local_ring_density = forensic->circle_local_ring_density;
```

The four AP-DIAG-025/026 fields were therefore silently discarded and remained at their struct defaults of zero.

The subsequent `ComponentCandidateClassifier` transfer path already copied all four fields correctly. The audit exporter therefore had no opportunity to recover the values after the `add_region()` boundary.

## Correction

`ShapeDetector::add_region()` now transfers:

```cpp
region.circle_probe_long_run_count =
    forensic->circle_probe_long_run_count;
region.circle_probe_max_run_fraction =
    forensic->circle_probe_max_run_fraction;
region.circle_local_horizontal_line_density =
    forensic->circle_local_horizontal_line_density;
region.circle_local_vertical_line_density =
    forensic->circle_local_vertical_line_density;
```

No classification behavior was modified.

## Regression Contract

The shape-detector regression now verifies that an accepted Circle exposes derived measurements consistent with its raw measurements.

For the eight outward runs:

```text
max_run = max(run_1 ... run_8)
threshold = max(1, ceil(0.75 * probe_distance))
```

The test requires:

```text
circle_probe_long_run_count == count(run >= threshold)
circle_probe_max_run_fraction == max_run / probe_distance
```

The test also requires the horizontal and vertical local-density fields to remain finite and non-negative.

This specifically protects the measurement-to-`ShapeRegion` transfer boundary that AP-DIAG-026 did not cover.

## Commits

- `e66c445b7ecab3e41cca98dc751f196533e9f82d` — AP-DIAG-027 production transfer correction
- `c4190dcaa24031dfd5782bf4834180ab21c4c84e` — AP-DIAG-027 regression test

## Verification Required

Run a clean Release build and complete CTest locally.

Then run the normal TRX300 extraction/publication workflow and inspect:

```text
artifacts/audit/extraction_audit.json
```

For at least one Circle candidate with a non-zero raw run, verify:

```text
raw run values survive
        ↓
long_run_count is derived from those runs
        ↓
max_run_fraction == max(run) / probe_distance
        ↓
horizontal/vertical local-density values are not silently reset
        ↓
published audit contains the same values
```

The AP remains open until the regenerated published artifact confirms the values at the final audit boundary.

## Classification Gate

**No Circle classifier tuning is authorized by this AP.**

AP-DIAG-027 only repairs evidence propagation and establishes a regression contract. Any subsequent classifier work must use the corrected measurements from a fresh extraction.

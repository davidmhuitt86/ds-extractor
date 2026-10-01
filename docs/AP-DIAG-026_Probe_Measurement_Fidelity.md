# AP-DIAG-026 — Probe Measurement Fidelity

## Status

**IMPLEMENTED — LOCAL RELEASE VALIDATION PENDING**

AP-DIAG-026 repairs the forensic measurement layer introduced by AP-DIAG-025. No Circle classification rule is changed.

## Baseline

- Prior diagnostic: AP-DIAG-025
- Source: `samples/trx300ODG.png`
- Source dimensions: 898 × 549
- Known accepted genuine circular candidates: 5
- Known false circular survivors: 17
- Prior extraction: 22 circular candidates
- Prior structural measurements: all zero across all 22 candidates

## Finding

AP-DIAG-025 used an exact single-pixel outward sampler:

- horizontal/vertical runs required `binary.at(...) != 0` at one exact raster coordinate;
- production crossing-line continuity instead searches a configurable ±2-pixel band;
- normalized/anti-aliased conductors can therefore be valid under the production detector while being invisible to the forensic sampler.

AP-DIAG-025 also used a 5-pixel morphological opening for local line density. Thin conductor strokes can be removed by that operation, producing zero structural density even where conductor raster is present.

These two measurement choices made the AP-DIAG-025 extraction non-diagnostic.

## Change

The forensic measurements now use the same thickness-aware raster semantics as the production crossing-line probe:

1. Outward horizontal and vertical runs accept raster support anywhere within `circle_crossing_probe_thickness`.
2. Local horizontal/vertical density uses the same thickness-aware neighborhood support rather than morphological opening.
3. Existing AP-DIAG-024/025 evidence fields remain observational.
4. The Circle acceptance predicate remains unchanged:
   `probe.weakest_side >= config.circle_crossing_min_line_continuity` still rejects a candidate.
5. No detector threshold, contour criterion, circularity criterion, edge-support criterion, or topology logic is changed.

## Regression coverage

A synthetic accepted-circle fixture includes a thin conductor touching the circle's top bounding-box side. The test requires:

- non-zero `circle_probe_max_run_fraction`;
- non-zero `circle_probe_long_run_count`;
- non-zero `circle_local_horizontal_line_density`.

This verifies that the forensic layer can actually observe thin structural raster evidence.

## Required validation

Run a clean Release build and full CTest suite, then perform the normal extraction/publication workflow.

The next forensic comparison must use the same 5 known genuine and 17 known false candidate coordinates and compare:

- all eight outward runs;
- long-run count;
- maximum run fraction;
- horizontal/vertical local support density;
- existing AP-DIAG-024 probe evidence.

No classifier tuning is authorized until the repaired measurements have been observed on the real TRX300 extraction.

## Implementation commits

- `4e9c31154b059b168b79c4876d8f6b4458ba29e2` — thickness-aware forensic probe implementation
- `d51161df2e5e70cd7230aebcc06b75c6587fab85` — regression coverage

## Decision gate

AP-DIAG-026 closes only after:

1. clean Release compilation succeeds;
2. CTest passes;
3. real extraction is published;
4. repaired forensic measurements are non-degenerate;
5. the 5/17 known population comparison is completed;
6. classification remains unchanged during the evidence run.

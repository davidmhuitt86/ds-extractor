# AP-DIAG-025 — Structural Context Forensic Evidence

## Status

**DIAGNOSTIC — STRUCTURAL-CONTEXT INSTRUMENTATION IMPLEMENTED; EXTRACTION PENDING**

## Objective

Extend AP-DIAG-024B from local density and raw outward-run measurements into explicit structural-context evidence without changing Circle classification.

The remaining false-positive population demonstrates that global contour metrics and adaptive probe distance are insufficient. This increment therefore measures whether a candidate is embedded in long straight raster geometry and whether its immediate neighborhood contains horizontal or vertical line structure.

## Scope

Production classification is unchanged.

The following are observational only:

- corrected symmetric four-corner exterior patch density;
- count of eight outward runs occupying at least 75% of the adaptive probe distance;
- maximum outward-run fraction;
- local horizontal straight-line density;
- local vertical straight-line density.

No Circle threshold, topology, Wire, endpoint, connector, or electrical-net behavior is changed.

## Corner measurement correction

AP-DIAG-024B's original corner helper sampled an upper-left-oriented rectangle regardless of which corner was requested. AP-DIAG-025 corrects this instrumentation so:

- top-left samples the exterior upper-left quadrant;
- top-right samples the exterior upper-right quadrant;
- bottom-left samples the exterior lower-left quadrant;
- bottom-right samples the exterior lower-right quadrant.

This is an evidence-capture correction only.

## Structural measurements

circle_probe_long_run_count counts how many of the eight existing outward runs reach at least 75% of the adaptive probe distance.

circle_probe_max_run_fraction records the longest of those eight runs divided by the adaptive probe distance.

The local horizontal and vertical line densities apply a 5-pixel morphological line opening to a 7-pixel expanded neighborhood around the candidate and normalize the surviving ink by the clipped neighborhood area.

These measurements are deliberately descriptive. They do not assert that a detected line is an electrical conductor.

## Required validation

A real extraction must be performed on:

samples/trx300ODG.png

The five known genuine candidates and the 17 known false survivors from AP-DIAG-024 must be compared in the same run.

Particular attention remains on the controlled same-size pair:

- genuine: (511,448) 11x13
- false: (532,448) 11x13

No detector tuning is justified until this evidence is reviewed.

## Implementation

Initial implementation commits:

- 69480b6e4af1538384152592d1f5078cad6a353b — ShapeRegion structural fields
- 22a42ec9194645c2ad4ebf073314dc5870a09874 — ComponentCandidate structural fields
- 0fd1a9cbd0e91072c0109a4fc6c5c45e6ea99bc5 — candidate propagation
- d9348745b07fc88d023d8b152a161e1646b954b6 — raster measurements and corner correction
- b671106470c960185f95ced1dcb6c208c63abb55 — audit serialization
- 21bc2b6f8c41936b578e1f7ce1d4a77031c33362 — ShapeDetector regression coverage
- 91e044ae8ac61f7a51b010f34a8371c7e2b65efe — audit serialization regression coverage

## Disposition

This AP remains diagnostic until the instrumented extraction is executed locally and published to extraction-results.

No production classification change is permitted based solely on this instrumentation.

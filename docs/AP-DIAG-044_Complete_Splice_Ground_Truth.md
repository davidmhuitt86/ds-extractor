# AP-DIAG-044 — Complete Splice Ground-Truth Mapping

## Status

IMPLEMENTATION CANDIDATE — diagnostic only.

No production extraction logic is modified.

## Objective

Map the 19 manually verified splice locations in the TRX300 diagram against the exact current production extraction **without hard-coding source-image coordinates from the annotated screenshot**.

The user-provided annotated image is a separate resized/cropped rendering of the same diagram. It is therefore treated as an evidence image, not as a canonical source-coordinate system.

## Ground truth

There are exactly 19 manually verified source splice locations.

The diagnostic derives those 19 locations from the yellow marks in the annotated image, then registers the annotated image to the canonical production source image.

The diagnostic compares those registered locations with:

- current endpoint candidates
- current topology nodes
- the 35 AP-DIAG-042 terminal-splice residual mappings

Association threshold: 6 pixels.

## Registration and marker detection

The annotated image is supplied as an explicit runtime input:

1. Detect yellow annotation pixels in HSV space.
2. Locate exactly 19 marker centers using a fixed-size yellow-density window and non-maximum suppression.
3. Remove yellow pixels from the annotation image for registration so the markings do not become registration features.
4. Search a bounded isotropic scale range and translation window using normalized template correlation against the canonical source.
5. Reject the diagnostic if fewer/more than 19 markers are detected or if registration confidence is below the required threshold.
6. Transform the 19 annotation-space marker centers into canonical 898x549 source coordinates.
7. Only then associate them with production endpoint/topology/residual objects.

This makes the annotation image itself the provenance for the ground truth rather than manually transcribed coordinates.

## Invocation

    .\\build\\Release\\dx-audit-splice-ground-truth.exe samples\\trx300ODG.png "<annotated-image>" artifacts\\audit

Example using the user-provided annotation:

    .\\build\\Release\\dx-audit-splice-ground-truth.exe samples\\trx300ODG.png "Screenshot 2026-10-07 001715.png" artifacts\\audit

## Output

    artifacts/audit/AP-DIAG-044_splice_ground_truth.json

The report records:

- detected annotation-space marker positions
- registration scale, translation, and score
- transformed canonical source positions
- nearest endpoint and topology node
- nearest residual node
- 19-source-splice coverage
- residual-to-ground-truth associations

## Acceptance

The diagnostic is accepted only after:

1. It builds cleanly.
2. The canonical TRX300 source is accepted as 898x549.
3. The current production replay remains at 77 model wires.
4. All 35 AP-DIAG-042 residual mappings are consumed.
5. Exactly 19 annotation markers are detected.
6. Registration meets the required confidence threshold.
7. All 19 registered ground-truth splice records are emitted.
8. The report explicitly identifies ground-truth splice coverage.
9. Every residual is mapped to its nearest ground-truth splice.
10. No production extraction source is modified.
11. Clean Release build, full CTest, and the AP acceptance gate pass.

No production AP may be selected from the diagnostic until the generated report and the registered source positions have been reviewed.

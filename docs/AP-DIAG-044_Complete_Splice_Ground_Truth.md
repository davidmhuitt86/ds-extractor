# AP-DIAG-044 — Complete Splice Ground-Truth Mapping

## Status

IMPLEMENTATION CANDIDATE — diagnostic only.

No production extraction logic is modified.

## Objective

Map the 19 manually verified splice locations in the TRX300 source diagram against the exact current production extraction.

The ground-truth coordinates were taken from a crop of the same 898x549 TRX300 diagram used by the extraction pipeline and are treated as source-image coordinates.

## Ground truth

There are exactly 19 manually verified source splice locations:

- SPLICE-01 through SPLICE-19

The diagnostic compares those locations with:

- current endpoint candidates
- current topology nodes
- the 35 AP-DIAG-042 terminal-splice residual mappings

Association threshold: 6 pixels.

## Expected diagnostic question

The diagnostic does not change production behavior. It establishes which of the 19 source splices currently reach the residual population and which do not.

It also maps every one of the 35 residual records to its nearest ground-truth splice and reports whether that distance is within the association threshold.

This produces the evidence needed to distinguish:

- source splices represented by current residual topology
- source splices missed by the current pipeline
- residual topology that does not correspond to a source splice

## Invocation

    .\\build\\Release\\dx-audit-splice-ground-truth.exe samples\\trx300ODG.png artifacts\\audit

## Output

    artifacts/audit/AP-DIAG-044_splice_ground_truth.json

## Acceptance

The diagnostic is accepted only after:

1. It builds cleanly.
2. The exact TRX300 source is accepted as 898x549.
3. The current production replay remains at 77 model wires.
4. All 35 AP-DIAG-042 residual mappings are consumed.
5. All 19 ground-truth splice records are emitted.
6. The report explicitly identifies ground-truth splice coverage.
7. Every residual is mapped to its nearest ground-truth splice.
8. No production extraction source is modified.
9. Clean Release build, full CTest, and the AP acceptance gate pass.

No production AP may be selected from the diagnostic until the generated report has been reviewed.

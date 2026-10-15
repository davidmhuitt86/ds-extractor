# AP-DIAG-FIX-013 — Connector Interaction Evidence Propagation

## Status

Implementation committed for review. A clean Windows Release build and full
CTest run are required before acceptance.

## Defect

In `ExtractionPipeline::run()`, the connector observation loop determines
whether each conductor interaction is `Termination` or `PassThrough`.
The local `PinObservation` record also stores an `interaction` field, but
the aggregate initializer supplied only the segment ID, position, and overlap.
The omitted field therefore retained its default value, `Unknown`.

Later, the pipeline copies that default field into
`ConnectorConductorCrossingEvidence`. As a result, crossing-evidence records
lost information that had already been correctly classified upstream.

## Correction

The observation aggregate now receives all four values, including the
calculated `interaction`. No interaction-classification rule, geometry
threshold, connector acceptance rule, Wire identity, topology, or ElectricalNet
logic was changed.

## Regression coverage

Added `dx-wire-test-connector-interaction-propagation`. It runs the canonical
`samples/trx300ODG.png` through the real `ExtractionPipeline` and verifies:

- the production path emits connector pins and crossing-evidence records;
- no emitted crossing record has `Unknown` interaction after a conductor
  observation was classified as either `Termination` or `PassThrough`.

The test reports the resulting count of each classification without asserting
specific connector, pin, Wire, or ElectricalNet totals.

## Validation status

Repository changes are prepared, but the Windows/OpenCV Release build and
CTest results have not yet been run for this commit. Do not mark this AP
accepted until those checks pass.

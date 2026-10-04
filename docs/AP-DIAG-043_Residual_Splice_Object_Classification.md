# AP-DIAG-043 — Residual Splice Object Classification

Status: IMPLEMENTED — diagnostic only; no production extraction logic changed

## 1. Purpose

AP-DIAG-042 established exact production replay for the current TRX300
WireModel:

- 77 model Wires;
- 37 Pass-1 Wires;
- 40 Pass-2 candidate pairs;
- 0 replay/model pair differences;
- 35 residual zero-Wire endpoints;
- all 35 residual walks stop at 35 unique topology nodes classified by
  production as Splice stops.

AP-DIAG-043 examines those exact 35 terminal-Splice locations as source
objects. The goal is to distinguish an actual electrical splice/junction from
a topology node caused by a connector body, component body, ordinary line
crossing, or other local source geometry.

The AP is deliberately diagnostic. It does not change topology, endpoint
classification, connector recognition, or physical-Wire reconstruction.

## 2. Canonical input

Source:

    samples/trx300ODG.png

Expected current TRX300 model:

    wires              = 77
    endpoint_candidates = 189
    topology_nodes     = 532
    topology_edges     = 643

AP-DIAG-042 must first have been run for the same extraction:

    artifacts/audit/AP-DIAG-042_exact_production_replay.json

The tool validates that exactly 35 "splice_stop" trace mappings are present
and then reruns the canonical ExtractionPipeline against the same source.

Invocation:

    .\build\Release\dx-audit-residual-splice-object-classification.exe samples\trx300ODG.png artifacts\audit

## 3. Evidence domains

Each of the 35 terminal-Splice records receives independent evidence from:

1. Topology:
   - topology-node type;
   - graph degree;
   - unique incident ConductorSegments;
   - repeated ConductorSegment incidence.

2. Object geometry:
   - nearest non-furniture ComponentCandidate bounds;
   - distance from the terminal-Splice coordinate to the component bounds;
   - nearest ConnectorCandidate bounds;
   - distance to the connector bounds.

3. Source-raster evidence:
   - center ink density;
   - local ink density;
   - horizontal ray support;
   - vertical ray support;
   - bidirectional axis support.

The raster evidence is intentionally local and observational. It is not
allowed to alter the current model.

## 4. Diagnostic classifications

The tool emits the following classification labels:

- CONNECTOR_BODY_CANDIDATE
- COMPONENT_BODY_CANDIDATE
- AMBIGUOUS_OBJECT
- CROSSING_CANDIDATE
- REAL_SPLICE_CANDIDATE
- UNDETERMINED

These are diagnostic classifications, not production assertions.

CONNECTOR_BODY_CANDIDATE is emitted when a recognized ConnectorCandidate
bounds lies within the configured local geometry threshold.

COMPONENT_BODY_CANDIDATE is emitted when a recognized non-furniture
ComponentCandidate bounds lies within that threshold and no connector
candidate also matches.

AMBIGUOUS_OBJECT is emitted when both object domains match.

REAL_SPLICE_CANDIDATE requires a compact, high-density center-ink signature
consistent with a filled junction dot and no nearby recognized object bounds.

CROSSING_CANDIDATE requires bidirectional local axis ink without the filled
dot signature.

Otherwise the location remains UNDETERMINED.

## 5. Outputs

Machine-readable report:

    artifacts/audit/AP-DIAG-043_residual_splice_object_classification.json

Source crops:

    artifacts/residual_splice_classification/splice-001.png
    ...
    artifacts/residual_splice_classification/splice-035.png

Contact sheet:

    artifacts/residual_splice_classification/AP-DIAG-043_contact_sheet.png

The report contains the full 35-record evidence population, classification
counts, object-distance measurements, topology measurements, and raster
measurements.

## 6. Non-goals

This AP does not:

- modify PhysicalWireIdentityReconstructor;
- modify WireReconstructor;
- promote Splices to endpoints;
- change connector recognition;
- change component recognition;
- add masking;
- add a branch decomposition rule;
- change Wire count;
- change ElectricalNet resolution;
- modify the TRX300 extraction.

## 7. Acceptance

AP-DIAG-043 is complete when:

- all 35 AP-DIAG-042 terminal-Splice mappings are consumed;
- all 35 locations resolve to current model objects;
- the report contains evidence for every location;
- the 35 crops and contact sheet are generated;
- no production source is modified.

No production conclusion is permitted from the heuristic classification counts
alone. The contact sheet and per-record source evidence must be reviewed
before selecting the next production AP.

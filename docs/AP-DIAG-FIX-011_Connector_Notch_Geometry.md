# AP-DIAG-FIX-011 — Connector Notch Geometry

## Status

IMPLEMENTED — awaiting fresh TRX300 extraction verification.

## Finding

The AP-DIAG-FIX-010 extraction reduced connector candidates from 50 to 30, but the review still showed many large rectangular/circular/non-connector regions being materialized.

The remaining false-positive population demonstrates that conductor evidence alone is not sufficient. A wire crossing or merged symbol can provide multiple conductor observations even when the enclosing geometry is not a connector.

## Source-grounded connector geometry rule

The TRX300 connector family is characterized by a physical notch/interlock in the connector body.

Therefore connector-body recognition now requires measurable contour concavity corresponding to a notch.

Generic properties are no longer sufficient:

- non-convex contour alone — insufficient;
- interior void alone — insufficient;
- polygon vertex count alone — insufficient.

The detector now computes OpenCV convexity defects and requires at least one defect whose depth is at least 2.0 pixels by default.

This remains an image-geometry criterion only. It does not infer connector pin identity, electrical continuity, mating, Wire identity, or ElectricalNet membership.

## Combined recognition boundary

A ConnectorCandidate now requires both:

1. characteristic connector-notch geometry;
2. at least two independent ConductorSegment observations.

The second condition remains in AP-DIAG-FIX-010.

Pass-through continuity is not required. A component-attached connector may have conductor termination rather than conductor pass-through.

## Tests

`tests/test_connector_geometry_detector.cpp` now covers:

- inline notched connector with pass-through conductor;
- component-attached notched connector with no pass-through conductor;
- generic non-convex body without the connector-family notch, which must not be recognized.

## Verification required

A fresh extraction must determine:

- remaining ConnectorCandidate count;
- remaining ConnectorPin count;
- whether the known connector locations survive;
- whether false positives visible in the previous review disappear;
- physical Wire count;
- ElectricalNet count;
- validation errors/warnings.

No final connector count is assumed until those results are observed.

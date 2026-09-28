# AP-DIAG-FIX-010 — Connector Conductor-Evidence Gate

## Status

IMPLEMENTED — awaiting fresh TRX300 extraction verification.

## Trigger

The 2026-09-28 extraction review generated **50 ConnectorCandidate records** from a source diagram for which the current working engineering inventory is **13 physical connector instances**.

The review image showed the dominant false-positive class directly: circular symbols, ground-related geometry, lamp bases, and other non-connector diagram objects were being promoted by the geometry-only connector stage.

The same review reported:

- 50 connector candidates
- 26 connector terminals
- 38 physical wires
- 13 electrical nets
- 0 validation errors

The 50-candidate result is therefore treated as a detector/materialization defect, not as evidence that the source contains 50 connectors.

## Root Cause

The image-stage ConnectorGeometryDetector currently accepts a body when:

- the contour is non-convex, or
- the contour has an interior void, or
- the polygon approximation contains more than four vertices.

The forensic design material already established that polygon vertex count is not discriminating by itself. In particular, ordinary circular and symbol geometry can satisfy the same generic contour conditions.

The governing connector design requires a hybrid model:

1. connector-body geometry provides candidate evidence;
2. independent conductor interaction provides semantic evidence.

The previous pipeline promoted the geometry candidate before checking whether any independent conductor evidence supported it.

## Fix

`src/pipeline/extraction_pipeline.cpp` now requires a connector geometry candidate to have at least **two distinct ConductorSegment observations** before a `ConnectorCandidate` is materialized.

The gate is intentionally applied **after** conductor interaction classification and **before** ConnectorCandidate/ConnectorPin materialization.

This preserves the distinction:

- `ConnectorBody` = image evidence;
- `ConductorSegment` interaction = independent conductor evidence;
- `ConnectorCandidate` = semantic connector candidate supported by both.

The gate does **not** require pass-through continuity. A conductor terminating at a component-attached connector is still valid evidence because the existing interaction classifier records it as `Termination`.

## Source-grounding of the minimum

The pre-existing TRX300 connector census recorded 2–4 visible conductor-side pins for every one of its 12 inventoried connector locations. The current engineering review subsequently established a working count of 13 physical connector instances.

The minimum of two is therefore based on the observed source family rather than on a generic contour-size threshold.

No maximum pin count is imposed by this fix.

## Invariants

This fix must not:

- create Wire records;
- move Wire endpoints;
- create topology edges;
- create ElectricalNets;
- pair connector conductors into arbitrary entry/exit identities;
- infer mating relationships;
- require pass-through continuity;
- treat a ConnectorPin as an EndpointCandidate.

## Verification Required

A fresh extraction must verify:

1. connector count falls substantially from 50;
2. false-positive geometry visible in `05_connectors.png` / `12_recognition.png` is suppressed;
3. source connector locations remain represented;
4. connector terminal count is supported by actual conductor evidence;
5. physical-wire count remains stable unless an independently justified connector interaction changes endpoint attribution;
6. electrical-net identity is unchanged by connector recognition;
7. validation errors remain zero;
8. no connector candidate survives with fewer than two independent conductor-segment observations.

This AP does not declare the final connector count until the fresh extraction is executed and reviewed.

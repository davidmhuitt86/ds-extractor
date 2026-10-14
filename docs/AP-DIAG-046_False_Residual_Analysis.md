# AP-DIAG-046 — False Residual Analysis

Status: IMPLEMENTED — diagnostic only; no production extraction logic changed

## Purpose

AP-DIAG-044 established 35 residual zero-Wire endpoint locations: 10 correspond to the 19 manually verified source splices and 25 do not. AP-DIAG-046 analyzes exactly those 25 non-ground-truth residuals before any endpoint or splice production rule is changed.

## Canonical input

Source: `samples/trx300ODG.png`

Required artifact: `artifacts/audit/AP-DIAG-044_splice_ground_truth.json`

Expected model: 77 wires, 189 endpoint candidates, 532 topology nodes, 643 topology edges.

## Evidence

Each residual receives:
- AP-DIAG-044 endpoint/node identity and nearest ground-truth splice distance;
- topology node type, degree, unique/repeated conductor segments;
- endpoint position, kind, role, and node-to-endpoint distance;
- nearest recognized component and connector bounds;
- local raster ink density and horizontal/vertical support.

## Diagnostic classifications

- CONNECTOR_BODY_CANDIDATE
- COMPONENT_BODY_CANDIDATE
- AMBIGUOUS_OBJECT_CANDIDATE
- CROSSING_CANDIDATE
- CONDUCTOR_END_CANDIDATE
- SPLICE_CANDIDATE
- ENDPOINT_NODE_SEPARATION_CANDIDATE
- OTHER_TOPOLOGY_CANDIDATE

These are observational labels only. They must not be promoted directly into production logic.

## Outputs

`artifacts/audit/AP-DIAG-046_false_residual_analysis.json`

`artifacts/false_residual_analysis/residual-001.png` through `residual-025.png`

`artifacts/false_residual_analysis/AP-DIAG-046_contact_sheet.png`

## Non-goals

No production extractor, topology, endpoint, splice, crossing, connector, component, wire count, or ElectricalNet logic is modified.

## Acceptance

Exactly 25 false residuals must be consumed; all 25 IDs must resolve in the 77-wire baseline; all evidence, crops, and the contact sheet must be produced; no production source may change; clean Release build, CTest, and the AP acceptance gate must pass. Classification counts alone are insufficient for production conclusions; the contact sheet and per-record evidence must be reviewed.

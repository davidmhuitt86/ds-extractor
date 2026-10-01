# AP-DIAG-028 — Circle Survivor Classification Forensics

## Status

**IMPLEMENTED — awaiting fresh Release extraction verification**

This AP closes the forensic comparison phase and implements the smallest contextual classifier supported by the corrected AP-DIAG-027 evidence.

No classifier thresholds were changed in ShapeDetector. The new gate runs after conductor detection and before downstream component semantics.

## Baseline

Source: samples/trx300ODG.png, 898 × 549, extraction-results after AP-DIAG-027.

22 circular candidates: 5 source-confirmed genuine and 17 source-confirmed false survivors.

Published population: 30 components, 18 terminal candidates, 2 connectors, 1 connector terminal, 230 conductor segments, 192 endpoint candidates, 37 wires, 533 topology nodes, 643 topology edges, 12 electrical nets.

## Forensic Result

Fourteen false survivors have circle_probe_max_run_fraction = 1.0 and circle_probe_long_run_count > 0. Example (420,259): max run fraction 1.0, long run count 7, horizontal line density 0.946087, vertical line density 0.942609.

The remaining ambiguous false candidates are (344,69), (357,69), and (532,448). The first two are 9-pixel-scale candidates adjacent to parallel line geometry but have no conductor intersection or qualifying conductor endpoint. Candidate (532,448) is especially important because it has the same 11 × 13 dimensions as genuine (511,448) and the same max-run fraction of 0.0769231, yet has no qualifying conductor attachment.

## Genuine Population Cross-check

All five source-confirmed genuine circles satisfy the new rule:

- (511,448): normalized conductor endpoint approximately 4.5 px from candidate bounds.
- (357,462): conductor segment intersects candidate bounds.
- (761,361): conductor segments intersect candidate bounds.
- (77,83): conductor segment intersects candidate bounds.
- (90,98): conductor segments intersect candidate bounds.

All five have circle_probe_max_run_fraction below 0.75.

## Implemented Rule

Rule 1: reject a circular candidate when circle_probe_max_run_fraction >= 0.75.

Rule 2: for candidates surviving Rule 1, retain the circle only when at least one normalized conductor segment intersects the candidate bounds or has an endpoint no more than 5 px from the candidate bounds.

The test uses normalized conductor geometry rather than terminal recognition, so the classifier does not depend on a later semantic stage.

## Architectural Placement

ShapeDetector → MorphologyWireDetector → ComponentCandidateClassifier → CircleContextClassifier → DiagramFurnitureClassifier → GeometryOwnershipClassifier.

ShapeDetector remains responsible for geometric candidates and forensic evidence. CircleContextClassifier correlates candidates with normalized conductor geometry.

## Regression Coverage

Added circle_context_classifier.hpp, circle_context_classifier.cpp, and test_circle_context_classifier.cpp.

The unit regression covers long-run rejection, direct conductor intersection acceptance, endpoint-within-5-px acceptance, nearby-but-unattached rejection, and pass-through of non-circle candidates.

## Verification Gate

The implementation is not closed until a fresh clean Release extraction is published and the audit confirms 22 baseline circles reduce to 5 retained genuine circles: 14 rejected by line/grid evidence and 3 rejected by conductor-context evidence.

No final extraction-fidelity conclusion is claimed before that fresh artifact is inspected.
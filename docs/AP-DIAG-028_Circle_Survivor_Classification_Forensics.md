# AP-DIAG-028 — Circle Survivor Classification Forensics

## Status

**CLOSED — verified by fresh Release extraction**

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

## Verification Result

Fresh published Release extraction verified the expected classification result:

- circular symbols: **5**
- false circle survivors: **0**
- 14 baseline line/grid false survivors rejected
- 3 baseline ambiguous false survivors rejected
- all 5 source-confirmed genuine circles retained

Collateral population check:

- terminal candidates: 18 — unchanged
- connectors: 2 — unchanged
- connector terminals: 1 — unchanged
- conductor segments: 230 — unchanged
- endpoint candidates: 192 — unchanged
- wires: 37 — unchanged
- topology nodes: 533 — unchanged
- topology edges: 643 — unchanged
- electrical nets: 12 — unchanged
- validation errors: 0

The published audit also reports zero unreferenced conductor segments and zero invalid wires.

**AP-DIAG-028 is closed.**

The remaining major diagnostic population is the pre-existing reconstruction gap: 118 endpoint candidates currently have zero Wire membership, 602 topology edges are not owned by a Wire, and 28 Wires remain fully unresolved. Those are carried forward to the next AP rather than being attributed to the circle classifier.
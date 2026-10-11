# AP-DIAG-FIX-009 — Source-ID-Invariant Topology Reconstruction

Status: implementation on branch; acceptance pending Windows/Visual Studio/OpenCV 5.0.0 validation and full artifact review. This is a production-code change and must not be merged until the gates below pass.

## Evidence that motivated the fix

AP-DIAG-048B ran the canonical image in one process, with the same image path, image bytes, pipeline object, and default configuration, varying only `source_id`:

- basename: `trx300ODG.png`
- path: `samples\\trx300ODG.png`

Both variants generated 230 normalized conductor segments and had identical normalized conductor-geometry multisets. Their segment sequence differed. Yet downstream structural geometry differed: basename produced 533 nodes, 192 endpoints, and 78 wires; path produced 532 nodes, 189 endpoints, and 77 wires. Both produced 643 edges. The report also showed differences in node positions, endpoint positions, edge geometry, and wire endpoint pairs.

This is a source-ID invariance defect. Stable identifiers are allowed to vary with `source_id`; physical topology geometry and population must not.

## Mechanism

`ConductorNormalizer::normalize()` assigns segment IDs using the canonical geometry plus `provenance.source_id`, then sorts the normalized segments by those IDs. As a result, changing only `source_id` changes the sequence presented to `TopologyReconstructor`.

The topology reconstructor aggregates split-point evidence in input sequence order. Its first-match snap clustering keeps the existing representative coordinate when a later point falls within `snap_tolerance`; it does not recompute a canonical representative. Consequently, changing only segment sequence can change retained coordinates and downstream topology classification.

## Implementation

`TopologyReconstructor::reconstruct()` now creates a non-owning sequence of pointers and sorts it by canonical, undirected segment geometry before initializing split points, evaluating pairwise intersections, aggregating node evidence, and creating edges. The original segment IDs and all source-derived identity conventions are retained for traceability; IDs no longer control geometry-processing order.

The existing `dx-wire-test-topology` adds an adversarial regression fixture where four orthogonal segments yield nearby but non-identical intersections inside the snap tolerance. It constructs the same geometric input in reverse vector order and compares sorted node geometry/type/connectivity signatures and edge geometry signatures, ignoring generated IDs. The two graph signatures must be identical.

## Scope / non-goals

- Changes only topology-reconstruction processing order and its regression test.
- No change to source-ID format, stable-ID implementation, conductor normalization, snap tolerance, intersection tolerance, crossing/splice policies, node merge logic, or wire identity policy.
- No artificial pinning to the TRX300 counts or source ID.
- This AP does not claim full pipeline acceptance until the AP-DIAG-048B control is rerun against both source-ID variants.

## Acceptance gate

1. Configure and compile Release with the Windows/Visual Studio/OpenCV 5.0.0 toolchain.
2. Build `dx-wire-test-topology` and `dx-audit-source-id-invariance`.
3. Run the topology test with assertions enabled and run all CTest tests.
4. Run AP-DIAG-048B on `samples\\trx300ODG.png` with both source-ID variants in one process.
5. Confirm the two variants have equal multisets of node coordinates/types/connectivity, endpoint geometry/kinds/node classes, geometry-normalized edges, and wire endpoint pairs. Raw IDs may differ because they intentionally encode source ID.
6. Inspect the complete report; do not accept from totals alone. Check counts agree between each run's arrays and counts object.
7. Perform a clean Release build, all CTest tests, check no unexpected warnings, inspect production metrics, and confirm no unrelated changes.
8. Integrate normally; only then run the main-only mandatory `tools/dx-ap-gate.ps1`. Do not weaken the gate.
9. If any downstream geometry signature remains different, this AP is not complete. Continue diagnosis instead of adjusting expected counts.

## Expected diagnostic nuance

`normalized_segment_geometry_sequence_equal` may remain false after this fix because the model's normalized segment list is still sorted by source-derived IDs. That field records the upstream sequence AP-DIAG-048B is investigating; the required invariant is that topology and wire geometry/signatures downstream of `TopologyReconstructor` no longer change when only `source_id` changes.

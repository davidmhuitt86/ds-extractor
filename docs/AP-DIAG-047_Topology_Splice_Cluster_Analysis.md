# AP-DIAG-047 — Topology Splice Cluster / Duplicate Analysis

Status: IMPLEMENTED — diagnostic only; acceptance pending local build, runtime evidence, artifact review, and the mandatory AP gate.

## Purpose

Determine whether nearby topology Splice nodes represent separate source junctions, duplicate/fragmented topology, or ordinary geometry that has been promoted to Splice. This AP follows the AP-DIAG-044 ground-truth mapping and the AP-DIAG-045/046 evidence. It must not alter production extraction.

## Canonical inputs

- Source image: `samples/trx300ODG.png` (898 × 549).
- Required report: `artifacts/audit/AP-DIAG-044_splice_ground_truth.json`.
- Expected production replay: 77 wires, 189 endpoint candidates, 532 topology nodes, 643 topology edges.
- Ground truth: exactly 19 source splice records.
- Residual population: exactly 35 residual records in AP-DIAG-044.

The diagnostic fails closed if the source dimensions, model counts, ground-truth record count, residual record count, or residual node references differ from the expected canonical population.

## Operational definitions (diagnostic only)

- **Spatial cluster:** single-linkage connected component among `TopologyNodeType::Splice` nodes, where each connecting pair is no farther than 12.0 source-image pixels apart.
- **Near pair:** two distinct Splice nodes no farther than 6.0 pixels apart.
- **Exact-duplicate candidate pair:** two distinct Splice nodes no farther than 2.0 pixels apart. This is a report threshold, not an approved production snap/merge tolerance.
- **Ground-truth match:** node within 6.0 pixels of an AP-DIAG-044 source splice marker. This preserves AP-DIAG-044's existing association threshold.
- **Residual label:** the exact AP-DIAG-044 residual record keyed by topology-node ID; its existing `matches_ground_truth` value is copied without reinterpretation.

The 12 px radius is deliberately an observational neighborhood chosen to surface nearby topology structures while retaining node-level positions and edge evidence. Single-linkage clusters can span more than 12 px end-to-end; the report includes member coordinates and pairwise proximity lists so this cannot be mistaken for a maximum cluster diameter.

## Evidence captured

For each spatial cluster:
- stable cluster ID, centroid, member count, and crop;
- every member Splice node ID, exact position, graph degree, incident topology-edge IDs, adjacent topology-node IDs, unique incident conductor-segment IDs, endpoint-candidate IDs, residual status, and nearest ground-truth/residual associations;
- nearby non-Splice topology nodes within 12 px of any cluster member, with type, position, and degree;
- ground-truth splice IDs within 6 px;
- whether the cluster contains any ground-truth-matched residual and/or false residual.

Global evidence includes:
- canonical model and population counts;
- total Splice-node count, cluster count, multi-node cluster count, and minimum pairwise distance among distinct Splice nodes;
- all Splice-node pairs within 2 px and within 6 px;
- an invariant that every Splice node appears in exactly one spatial cluster;
- one crop per cluster and a contact sheet.

## Outputs

- `artifacts/audit/AP-DIAG-047_topology_splice_cluster_analysis.json`
- `artifacts/audit/splice_cluster_analysis/cluster-001.png` … one crop per cluster
- `artifacts/audit/splice_cluster_analysis/AP-DIAG-047_contact_sheet.png`

## Non-goals

No production source, endpoint, topology, splice/crossing classification, conductor segmentation, wire identity, electrical-net, or geometry tolerance changes. The diagnostic must not merge nodes or recommend a production threshold based on proximity alone.

## Acceptance criteria

1. The diagnostic target compiles on the user's Windows/OpenCV 5.0.0 environment.
2. It runs against the canonical source and AP-DIAG-044 report.
3. It verifies and reports exactly 77 wires, 189 endpoints, 532 nodes, 643 edges, 19 ground-truth splice records, and 35 residual records.
4. Every residual node ID resolves to a topology node.
5. Every topology Splice node appears in exactly one cluster; no node is silently dropped or duplicated.
6. Each cluster member reports its node identity, exact coordinates, degree, unique incident conductor segments, endpoint associations, residual label, and nearest ground-truth evidence.
7. Pair lists include every Splice-node pair within 2 px and every pair within 6 px; distances are computed from canonical source coordinates.
8. Every cluster crop and the contact sheet are written and readable.
9. No production logic is modified.
10. Clean Release build, all CTest tests, AP-specific executable build/run, artifact review, and `tools/dx-ap-gate.ps1` pass before this AP is accepted or merged.

Classification or cluster counts alone are not grounds for production changes. Review the JSON and contact sheet before defining a follow-on production AP.
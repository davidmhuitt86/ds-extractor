# AP-DIAG-015 — Structured Extraction Audit Artifact

## Status

IMPLEMENTED

## Objective

Restore the machine-readable reconciliation boundary identified by AP-DIAG-AUDIT-014.

The canonical extraction artifact is:

\`artifacts/audit/extraction_audit.json\`

The artifact is generated from the completed \`WireModel\`. It performs no detector pass, topology reconstruction, semantic inference, or threshold tuning.

## Implementation

- Added \`ExtractionAuditExporter\`.
- Added schema version \`2\`.
- Added deterministic source/run identity metadata.
- Added object-addressable records for:
  - component candidates
  - terminal candidates
  - connector candidates
  - connector terminals
  - conductor segments
  - endpoint candidates
  - wires
  - topology nodes
  - topology edges
  - electrical nets
- Added per-wire endpoint, topology-edge, conductor-segment, identity-status, and identity-evidence references.
- Added validation issues with severity, code, object ID, and detail.
- Preserved coverage diagnostics and full coverage findings.
- Records are copied and sorted by stable object ID before serialization so exporter ordering does not depend on detector vector insertion order.
- Updated \`ExtractionArtifactWriter\` to use the new exporter at the existing canonical path.
- Added a deterministic regression test.

## Reconciliation contract

The artifact intentionally distinguishes:

1. component candidates from electrical components;
2. connector candidates from connector terminals;
3. endpoint candidates from terminal candidates;
4. conductor segments from endpoint-to-endpoint Wire records;
5. topology nodes/edges from reconstructed Wire identity;
6. electrical-net membership from Wire identity.

A splice or junction remains a topology object. It is not serialized as a Wire endpoint merely because it is a graph node.

## Determinism

No timestamp or runtime-generated identifier is emitted. Source identity, page, dimensions, and object IDs establish the extraction run identity.

The regression test writes the same model twice and requires byte-identical JSON.

## Verification

Repository-level verification added:

\`dx-wire-test-extraction-audit-exporter\`

A local Windows Release build could not be executed from the hosted environment because the repository's GitHub host is not reachable from the execution container. The code and CMake integration were inspected directly after each repository write.

## Required next action

Run the normal local extraction on the cropped TRX300 source.

Then verify that:

\`artifacts/audit/extraction_audit.json\`

exists in the generated extraction output and is published to the \`extraction-results\` branch.

AP-DIAG-016 should then perform the first object-by-object reconciliation using the newly published artifact. No detector tuning should be performed until that reconciliation is complete.

# AP-DIAG-040 — Publish Source-Level Splice Reconciliation Artifacts

Status: COMPLETE — tooling/publishing only; no production extraction logic changed

## Purpose

Make the deterministic AP-DIAG-038 source-level splice reconciliation outputs
reviewable from the dedicated `extraction-results` branch.

Previously, `tools/dx-publish-review.ps1` published only the extraction review
images and structured extraction audit. This meant AP-DIAG-038 could generate
its crops locally, but the artifacts were discarded from the review branch.

## Change

The publisher now recognizes:

`artifacts/splice_reconciliation`

as an optional diagnostic artifact tree.

When present locally, the publisher:

1. removes the previous splice-reconciliation tree from the temporary publication
   worktree;
2. copies the newly generated reconciliation artifacts into that tree;
3. stages the tree together with the normal extraction review and audit;
4. includes the tree in the cached-diff test so a splice-only update cannot be
   mistaken for a no-op.

The splice-reconciliation tree remains diagnostic only. Its presence does not
alter extraction topology, endpoint classification, Wire reconstruction, or
validation.

## Preservation rule

The publication boundary remains:

- `artifacts/extraction_review/`
- `artifacts/audit/extraction_audit.json`
- optional `artifacts/splice_reconciliation/`

No other working-tree content is published.

## Next action

Run AP-DIAG-038 on the current `main` checkout:

```
.\tools\dx-splice-reconciliation.ps1
```

Then run the publisher:

```
.\tools\dx-publish-review.ps1
```

The resulting `extraction-results` branch should contain:

```
artifacts/splice_reconciliation/
├── raw/
├── annotated/
├── splice_reconciliation_manifest.json
└── splice_reconciliation_contact_sheet.png
```

The source crops can then be reviewed directly against the current 898 x 549
source image and the 35 terminal Splice locations established by AP-DIAG-039.

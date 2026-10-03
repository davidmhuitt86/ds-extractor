# AP-DIAG-038 — Source-Level Residual Splice Reconciliation Tool

Status: TOOLING ONLY — no production extraction logic changed

## Purpose

AP-DIAG-037 established that the 36 residual zero-wire endpoints ultimately
terminate at 35 Splice nodes, but the current environment cannot directly
inspect the PNG binary used by the Windows extraction run.

This AP adds a deterministic local diagnostic tool that uses the existing
review artifact source image and structured extraction audit to generate
exact source-level crops around those 35 terminal Splice nodes.

The tool does not alter topology, endpoints, Wire reconstruction, or any
production artifact.

## Tool

`tools/dx-splice-reconciliation.ps1`

Default inputs:

```
artifacts/extraction_review/00_source.png
artifacts/audit/extraction_audit.json
```

The script replays the current PhysicalWireIdentityReconstructor continuity
logic to identify the 35 terminal Splice nodes, then produces:

```
artifacts/splice_reconciliation/
├── raw/
│   ├── splice-001.png
│   └── ...
├── annotated/
│   ├── splice-001.png
│   └── ...
├── splice_reconciliation_manifest.json
└── splice_reconciliation_contact_sheet.png
```

The `raw/` crops are untouched source pixels. The `annotated/` crops add
only a review crosshair, ordinal, Splice ID, and coordinates.

## Review protocol

Inspect the raw or annotated crops in deterministic ordinal order.

For each Splice determine whether the source itself establishes:

1. a through conductor and a distinct branch conductor;
2. two or more equally plausible physical-conductor continuations;
3. no defensible through relationship.

The review must be based on visible source evidence such as explicit junction
symbols, continuation/branch construction, line-style conventions, or other
source-defined graphical conventions. It must not use proximity, shortest
path, electrical-net membership, or inferred convenience.

## Acceptance target

This AP does not establish a production Wire-count target.

It establishes only that the exact 35 residual splice locations can be
reconciled against the source image without manually re-finding coordinates.

## Next implementation gate

A production branch-decomposition AP may begin only after the generated crops
provide a repeatable source-backed rule. Synthetic tests must then cover the
specific rule and its negative cases before TRX300 output is changed.

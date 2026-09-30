# AP-DIAG-017 — Extraction Review Artifact Publication

## Status

IMPLEMENTED

## Objective

Restore publication of the complete extraction review artifact set to the dedicated `extraction-results` branch without changing detector or extraction behavior.

## Root Cause

The repository intentionally ignores generated extraction artifacts through `.gitignore`:

```
artifacts/
output/
project.json
```

The review publisher previously used:

```
git add -A -- artifacts\extraction_review artifacts\audit\extraction_audit.json
```

Git does not stage ignored, untracked paths with ordinary `git add -A`.

The structured audit artifact appeared on `extraction-results` because that file was already tracked from the prior publication correction. The newly regenerated visual review files were ignored and therefore never entered the commit.

This explains the observed state:

- `artifacts/audit/extraction_audit.json`: published
- `artifacts/extraction_review/`: absent

## Fix

Updated `tools/dx-publish-review.ps1` to stage the two explicitly published artifact paths with force:

```
git add -A -f -- artifacts\extraction_review artifacts\audit\extraction_audit.json
```

This preserves the repository-wide ignore rule while explicitly allowing the dedicated publisher to publish generated extraction artifacts.

No detector, topology, semantic, or extraction code was changed.

## Expected publication contract

Every successful review publication must contain both:

```
artifacts/extraction_review/
artifacts/audit/extraction_audit.json
```

The review directory is expected to contain the generated visual layers and `review_manifest.json`.

## Verification

The fix was committed to `main` as:

`f656b52e6c14d5ddec097fefd3ecdc35bf24c096`

Commit:

`DX-REVIEW: force-stage ignored extraction artifacts`

Required local verification:

1. Pull latest `main`.
2. Perform a clean Release build.
3. Run the Release test suite.
4. Run an unchanged TRX300 extraction.
5. Verify `artifacts/extraction_review/` exists locally.
6. Verify `artifacts/audit/extraction_audit.json` exists locally.
7. Run `tools/dx-publish-review.ps1`.
8. Verify the resulting `extraction-results` tree contains both artifact paths.

## Reconciliation boundary

No detector tuning is authorized by this AP.

The next analysis should use the published visual layers together with the structured audit to classify:

- zero-Wire endpoints;
- topology edges not owned by reconstructed Wires;
- topology-only conductor segments;
- components without terminal evidence;
- endpoints outside electrical nets;
- connector classification.

Only after those objects are classified against the source should an extraction defect be promoted to a detector-change AP.

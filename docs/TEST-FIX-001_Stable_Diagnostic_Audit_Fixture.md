# TEST-FIX-001 — Stable Diagnostic Audit Fixture

## Problem

The AP-DIAG-018 source-to-object reconciliation and AP-DIAG-019 correlated-failure investigation are checked-in historical reports derived from a particular extraction audit. Their tests used the mutable production output at `artifacts/audit/extraction_audit.json` as their source of object IDs and populations.

A later extraction run can legitimately replace that production output with a different model while the historical AP-DIAG-018 and AP-DIAG-019 reports remain unchanged. The tests then cross-compare records from different runs and fail (on Windows, an assertion failure can surface from CTest as exit code `0xc0000409`). That failure indicates the artifacts are not from the same run; it does not by itself indicate that connector-boundary production code failed.

## Correction

- Preserve the baseline audit used by these historical reports as `tests/fixtures/ap_diag_baseline/extraction_audit.json`.
- Make both report-integrity tests load that fixture explicitly.
- Leave `artifacts/audit/extraction_audit.json` available for the latest production extraction and review.

The fixture is an exact copy of the canonical baseline audit on `main` at implementation time. It is not regenerated as part of normal extraction.

## Scope

Test/artifact isolation only. No production detector, ownership, topology, endpoint, connector, wire identity, or tolerance behavior changes.

## Validation required

On Windows, perform a clean Release build, run the two affected tests and then the full CTest suite. Confirm that the diagnostic tests still validate their historical reports and that a newer production audit can coexist without changing their inputs. Run `git diff --check`.

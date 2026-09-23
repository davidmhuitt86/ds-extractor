# AP-WIRE-022A — Extraction Baseline Validation & Diagnostic Warning Expansion

## Purpose

AP-WIRE-022A establishes the first diagnostic instrumentation boundary after electrical-net resolution.

The objective is to measure and classify the existing extraction result before changing extraction algorithms. This stage must answer where the current model is uncertain, without treating uncertainty as a reason to fabricate engineering objects.

## Scope

AP-WIRE-022A currently classifies the existing structural validation warnings by deterministic validation code.

The diagnostic path:

`WireModel` → `WireModelValidator` → warning-code histogram → `ExtractionAudit` → audit/review artifacts

No conductor geometry, topology, endpoint identity, wire identity, component identity, or electrical-net membership is modified.

## Diagnostic Contract

Each validation warning is retained as a normal `WireValidationIssue`.

The audit additionally records:

- warning code
- number of occurrences for that code

The warning summary is sorted deterministically by validation code.

This makes the existing aggregate warning count actionable without changing validator semantics.

## Exported Artifacts

`artifacts/audit/extraction_audit.json` now contains:

`validation.warning_codes`

`artifacts/extraction_review/review_manifest.json` now contains:

`validation_warning_codes`

These fields expose the warning population produced by the exact extraction run.

## Engineering Invariants

1. Diagnostics never repair the model.
2. Diagnostics never infer missing endpoints.
3. Diagnostics never convert geometric endpoints into semantic terminals.
4. Diagnostics never alter wire identity.
5. Diagnostics never infer component identity.
6. Diagnostics never alter electrical-net membership.
7. Warning classification is deterministic.
8. Existing validation severity remains authoritative.
9. A warning count is evidence about the extractor, not evidence that a missing engineering object exists.

## Baseline Interpretation

The TRX300 baseline currently reports 31 validation warnings and zero validation errors.

The next diagnostic pass should use the warning-code population to determine which warnings are caused by:

- unresolved endpoint semantics
- incomplete terminal association
- unresolved electrical-net roles
- topology/path reconstruction
- other structural conditions

Algorithmic changes should be made only after the failure population has been identified and a regression case exists.

## Regression Requirement

The extraction-audit test verifies that warning-code summaries are generated deterministically from validation issues.

## Next Diagnostic Work

The next AP-WIRE-022A increment should extend the same instrumentation pattern to coverage diagnostics, including:

- conductor segments referenced by endpoint-to-endpoint wires
- conductor segments referenced by topology but not wires
- endpoints not referenced by any wire
- component candidates without terminal evidence
- topology nodes/edges with anomalous ownership
- electrical-net endpoint coverage

These diagnostics must remain observational until their failure populations are understood.


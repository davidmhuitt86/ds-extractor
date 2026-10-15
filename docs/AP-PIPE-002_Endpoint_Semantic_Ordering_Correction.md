# AP-PIPE-002 — Endpoint Semantic Ordering Correction

**Status:** Updated on top of current main; Windows Release build and full CTest revalidation pending.  
**Base:** `394df3158c74dd92b9fe3fcdf1117a50cf44c6aa`

## Problem

Connector-pin association and terminal materialization occurred before the pipeline had finalized endpoint semantics. In particular, connector association code read `EndpointCandidate.kind` while endpoints still carried their provisional geometric classification, and `ConnectorTerminalModelBuilder::build_native()` ran before `EndpointSemanticReconstructor` and `EngineeringObjectSemanticApplier`.

That ordering allows an endpoint later classified as a ground or component terminal to have already been materialized as a connector terminal.

## Change

1. Geometric association now resolves only when exactly one endpoint is supported by the connector's observed conductor evidence. It no longer treats the early endpoint kind as semantic clearance.
2. The connector-terminal builder is moved to after endpoint semantic reconstruction and semantic application.
3. Immediately before materialization, associations are reconciled against finalized endpoint kinds. Known incompatible kinds become conflicted; unresolved kinds stay unresolved; only geometric conductor ends and connector terminals are eligible.
4. Regression coverage exercises `build_native()` with geometric, ground, component-terminal, and unresolved endpoint kinds.

## Scope boundary

This change does not modify connector geometry detection, conductor ownership, topology reconstruction, wire identity, or electrical-net resolution. The previously identified connector-boundary ownership issue (AP-DIAG-049) is intentionally not changed here; the source-specific diagnostic JSON must be reviewed before production suppression of any conductor-like geometry.

## Validation record

- Source-level call-order review: performed.
- Regression assertions added: yes.
- Prior branch validation (2026-10-14): clean Windows Release build passed; CTest **67/67 passed** on the earlier base.
- This branch has now been advanced to include current `main`, including AP-DIAG-FIX-013 and its new regression test. Rerun a clean Release build and full CTest; the previous validation does not cover the refreshed tree.
- Project preflight / acceptance gate: not run.

Do not merge until the refreshed local Windows build and full tests pass and the resulting diff is reviewed.

# AP-PIPE-002 — Endpoint Semantic Ordering Correction

**Status:** Implemented and locally validated on Windows Release.  
**Validated branch head before this documentation update:** `bba4510066a5e09a8f6ebec3066c728b5f03fe7b`  
**Integrated base:** `394df3158c74dd92b9fe3fcdf1117a50cf44c6aa`

## Problem

Connector-pin association and terminal materialization occurred before the pipeline had finalized endpoint semantics. In particular, connector association code read `EndpointCandidate.kind` while endpoints still carried their provisional geometric classification, and `ConnectorTerminalModelBuilder::build_native()` ran before `EndpointSemanticReconstructor` and `EngineeringObjectSemanticApplier`.

That ordering allowed an endpoint later classified as a ground or component terminal to be materialized prematurely as a connector terminal.

## Change

1. Geometric association resolves only when exactly one endpoint is supported by the connector's observed conductor evidence. It no longer treats the early endpoint kind as semantic clearance.
2. The connector-terminal builder runs after endpoint semantic reconstruction and semantic application.
3. Immediately before materialization, associations are reconciled against finalized endpoint kinds. Known incompatible kinds become conflicted; unresolved kinds stay unresolved; only geometric conductor ends and connector terminals are eligible.
4. Regression coverage exercises `build_native()` with geometric, ground, component-terminal, and unresolved endpoint kinds.

## Scope boundary

This change does not modify connector geometry detection, conductor ownership, topology reconstruction, wire identity, or electrical-net resolution. The previously identified connector-boundary ownership issue (AP-DIAG-049) is intentionally not changed here; the source-specific diagnostic JSON must be reviewed before suppressing any conductor-like geometry.

## Validation record

Maintainer-reported Windows validation on 2026-10-15, after checking out and fast-forwarding `fix/ap-pipe-002-semantic-ordering` to the validated head:

- CMake configure: PASS.
- Clean Windows Release build (`cmake --build build --config Release --clean-first`): PASS.
- Targeted `connector-terminal-model` regression: **1/1 passed**.
- Full Release CTest suite: **68/68 passed**.
- GUI executable check (`build/Release/dx-extractor-gui.exe`): PASS.
- GitHub Actions / repository AP preflight and acceptance gate: not run. The test evidence is the maintainer's local log, not CI.

Build completed with existing compiler warnings (including `getenv` deprecation, Release assertion-flag override, and discarded `[[nodiscard]]` returns); there were no build failures.

The result validates the AP-PIPE-002 source and its regression tests. AP-DIAG-049 connector-boundary ownership remains a separate, open issue and is not resolved by this change.

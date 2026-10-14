# AP-DIAG-048A — Model Population Reproducibility

Status: diagnostic implementation; acceptance pending local build/run and artifact review.

## Purpose
Capture stable IDs and full identity-bearing fields for the canonical pipeline replay so differences in wire, endpoint, and topology-node populations can be compared between runs and diagnostic branches. This tool does not compare against a presumed baseline internally and does not modify production behavior.

## Run
```powershell
cmake -S . -B build
cmake --build build --config Release --target dx-audit-model-fingerprint --parallel 1
.\build\Release\dx-audit-model-fingerprint.exe samples\trx300ODG.png artifacts\audit\AP-DIAG-048A_model_fingerprint.json
```

Run the same command twice without changing branch, source image, configuration, or build. Then run once on the AP-DIAG-047 branch and once on the AP-DIAG-048 branch using the same clean Release configuration. Compare counts and stable IDs; do not assume the old summary counts identify the cause.

## Captured fields
- Source path and dimensions
- Counts for wires, endpoints, nodes, edges, components, connectors
- Every topology node's ID, position, type, and connectivity flag
- Every endpoint's ID, node association, position, kind, role, confidence, incident edge IDs
- Every wire's ID, endpoint IDs, and conductor-segment IDs

## Acceptance
Build target, run twice in identical context, compare outputs, run clean Release build and all CTest tests, inspect JSON, and execute the main-only acceptance gate only after normal integration. No production changes are allowed.

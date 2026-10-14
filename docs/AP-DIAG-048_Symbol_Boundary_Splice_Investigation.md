# AP-DIAG-048 — Symbol-Boundary / False-Splice Investigation

Status: IMPLEMENTED ON BRANCH; acceptance pending local compile, runtime evidence, artifact review, and mandatory AP gate.

## Purpose
Measure whether production-classified Splice topology nodes lie inside or within 3 source-image pixels of component or connector candidate bounds. Record graph degree, incident edge IDs, adjacent topology nodes, conductor-segment count, local raster ink density, and nearest symbol-boundary distances. This is a diagnostic only; it does not propose or apply topology changes.

## Runtime
```powershell
cmake -S . -B build
cmake --build build --config Release --target dx-audit-symbol-boundary-splices --parallel 1
.\build\Release\dx-audit-symbol-boundary-splices.exe samples\trx300ODG.png artifacts\audit\AP-DIAG-048_symbol_boundary_splices.json
```

## Output
- `artifacts/audit/AP-DIAG-048_symbol_boundary_splices.json`
- Canonical source dimensions and model population counts
- One record per production Splice node, including nearest component/connector bounds, 3 px proximity flags, local ink density, incident edge IDs, adjacent node IDs, degree, and unique conductor-segment count

## Interpretation constraints
- Proximity to a symbol bounding box is evidence for review, not proof of a false splice.
- Component/connector bounds can overlap genuine terminals and legitimate electrical junctions.
- Ink density is descriptive only; it must not independently classify a node.
- The 3 px threshold is diagnostic only and is not a production snap, suppression, or merge tolerance.
- Compare these records against AP-DIAG-044 ground-truth markers and AP-DIAG-046 false-residual evidence before drawing causal conclusions.

## Acceptance gate
1. Confirm the tool compiles on Windows/OpenCV 5.0.0.
2. Run against canonical `samples/trx300ODG.png` and preserve the JSON report.
3. Verify counts against the current branch's production replay; investigate any drift instead of changing expected counts to make the diagnostic pass.
4. Review all nodes flagged near component/connector bounds, including genuine ground-truth splice locations.
5. Confirm no production source was modified.
6. Run clean Release build, all CTest tests, this target, and `tools/dx-ap-gate.ps1`.
7. Do not mark accepted until the local run and artifacts are reviewed.

## Non-goals
No production logic changes, no node merges, no suppression rules, no changed thresholds in production, and no ground-truth relabeling.

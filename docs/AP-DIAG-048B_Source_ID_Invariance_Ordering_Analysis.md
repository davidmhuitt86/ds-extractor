# AP-DIAG-048B — Source-ID Invariance and Ordering Analysis

Status: diagnostic implementation on branch; acceptance pending Windows/OpenCV build, runtime report, artifact review, clean Release build, all CTest tests, and the mandatory gate after normal integration.

## Purpose

Determine why `ExtractionPipeline::run(image_path, source_id)` produces different wire/endpoint/node populations when the same image is run with two source identifiers. This diagnostic runs both conventions sequentially in one process and captures identity-independent geometry so stable-ID changes are not mistaken for geometry changes.

## Controlled variants

For the same provided image path, the tool runs:

- `basename_source_id`: `source_path.filename().string()`
- `path_source_id`: `source_path.string()`

The image path, bytes, pipeline instance/configuration, executable, and runtime process remain the same. Only the `source_id` argument is intentionally varied. The report records source-image dimensions, file size, and an FNV-1a-64 checksum. The checksum is a repeatability fingerprint, not a cryptographic integrity claim.

## Runtime

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target dx-audit-source-id-invariance --parallel 1
.\build\Release\dx-audit-source-id-invariance.exe samples\trx300ODG.png artifacts\audit\AP-DIAG-048B_source_id_invariance.json
```

## Evidence captured per run

- Population counts: normalized conductor segments, wires, endpoints, topology nodes, topology edges, components, and connectors.
- Normalized segments in their actual `WireModel::conductor_segments` sequence, with geometry key, segment ID, endpoints, thickness, heavy-cable flag, and provenance source ID.
- Topology nodes by coordinate, type, and electrical-connectivity flag.
- Topology edges represented through endpoint positions and normalized segment geometry, independent of source-derived edge/segment IDs.
- Endpoints with position, kind, associated topology-node type, and incident-edge count.
- Wires with endpoint positions and conductor-segment geometry keys.

## Derived comparisons

The JSON report calculates:

- Normalized conductor-segment geometry multiset equality versus sequence equality, including the first sequence divergence.
- Node and endpoint position multiset equality, plus coordinate differences.
- Type/connectivity differences at coordinates present in both runs.
- Edge geometry multiset equality using segment geometry and endpoint coordinates.
- Wire endpoint-pair equality, listing pairs present in only one variant.

Raw stable IDs are still recorded for traceability, but ID differences alone are not considered proof of structural differences because IDs explicitly depend on `source_id`.

## Interpretation constraints

- Do not infer causation from count differences alone.
- If normalized segment geometry multisets match but sequence differs and downstream graph geometry diverges, the ordering-sensitive path in the normalizer/topology reconstructor becomes a supported candidate mechanism.
- If normalized segment geometry multisets differ, investigate upstream detection/normalization behavior before attributing downstream changes to ordering.
- A passing diagnostic build/runtime is not acceptance of a production fix.
- Do not change production extraction logic, node merge tolerances, thresholds, or stable-ID policy in this AP.
- Do not use the recorded FNV-1a-64 value as a security checksum.

## Acceptance gate

1. Build this target using Windows + Visual Studio + OpenCV 5.0.0.
2. Run it against canonical `samples\trx300ODG.png` and preserve `artifacts\audit\AP-DIAG-048B_source_id_invariance.json`.
3. Confirm both embedded runs use identical source dimensions/size/checksum and the two expected source-ID values.
4. Parse the JSON successfully and verify all reported segment/node/edge/endpoint/wire counts agree with the two raw run sections.
5. Review first segment sequence divergence, segment geometry multiset comparison, node coordinate/type differences, edge comparison, and wire endpoint-pair differences.
6. Confirm no production files changed.
7. Run clean Release build and all CTest tests; only run `tools/dx-ap-gate.ps1` after the AP has been integrated on `main` normally.
8. Keep this AP open until runtime output and the generated report are reviewed.

No production correction is authorized by this diagnostic alone.

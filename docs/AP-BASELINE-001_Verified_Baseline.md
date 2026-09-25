# AP-BASELINE-001 — Verified EKE-DX-WIRE Baseline

This document freezes the repository's verified, reproducible state after
AP-WIRE-FIX-003. It is a reference point for subsequent work, not a
specification — see the referenced AP-WIRE-029/AP-WIRE-031 documents for
the governing semantics themselves.

## A. Repository commit SHA

Verified at commit `8a116ac182a510dc772b9ad87a3ed8f448635080`
(`AP-WIRE-FIX-003 — Deduplicate equivalent physical wire discoveries`),
working tree clean at the time of verification.

## B. Branch

`claude/modest-dirac-a3d7pw`

## C. Build configuration

`CMAKE_BUILD_TYPE=Release`, `DX_WIRE_BUILD_TESTS=ON`,
`CMAKE_EXPORT_COMPILE_COMMANDS=ON`, built from a fully clean `build/`
directory (no reused object files).

## D. Compiler/toolchain

- `g++ 13.3.0` (Ubuntu 13.3.0-6ubuntu2~24.04.1)
- `cmake 3.28.3`
- OpenCV `5.1.0` (`/usr/local`, components: core imgproc imgcodecs highgui)
- CURL `8.5.0`

## E. Assertion configuration

Verified directly from `compile_commands.json`, not inferred from source:

- Production (`src/app/main.cpp`, `dx-extract`): `-O3 -DNDEBUG`, no
  `-UNDEBUG` — real Release behavior, unchanged.
- All 53 registered test targets: `-O3 -DNDEBUG -UNDEBUG` (in that order),
  per the AP-TEST-FIX-001 target-scoped CMake block — `assert()` is real in
  every test binary.
- `eke_dx_wire` library object files: zero carry `-UNDEBUG`.
- `dx-wire-test-assertions-enabled` (the AP-TEST-FIX-001 compile-time
  probe, `#ifdef NDEBUG` / `#error`): built and passed, confirming the
  protection is live, not just present in `CMakeLists.txt`.

## F. CTest result

53/53 passing on a from-scratch clean build (`rm -rf build`, fresh
`cmake` configure, full `-j$(nproc)` build). Assertions execute for real,
per (E) — this is not "the process exited 0."

## G. TRX300 input identity

The task text named `trx300OGD.pdf` as the canonical input; no file by
that exact name exists in the repository. The file used consistently as
canonical input by every prior AP-WIRE/AP-TEST-FIX document (AP-WIRE-022A
through AP-WIRE-FIX-003) is `samples/trx300ODG.png`
(SHA-256 `a145fffeb4cc9261152930936737048e39e92b030de5c9427b8f60e427d5da61`).
A different file, `samples/trx300OGD2.pdf`
(SHA-256 `2a8eb9827a756e098fe9749038a62692148961ca8ee67984f9ea80286a913e8b`),
also exists but has never been used as the baseline input in this
repository's history. This baseline uses `samples/trx300ODG.png`,
matching every prior AP measurement; the naming discrepancy is recorded
here rather than silently resolved.

## H. Extraction configuration

`dx-extract extract samples/trx300ODG.png --output <dir>`, the same
invocation and default `ExtractionConfig` used throughout AP-WIRE-022A
onward. No gap, terminal-recognition, shape-detector, or topology
parameter was touched by this task.

## I. Physical Wire count

**40**, confirmed by three independent measures: the CLI's own summary
line, `extraction_audit.json`'s `wires` field, and
`artifacts/topology/topology.json`'s `wires` array length.

## J. Wire identity status counts

From `artifacts/topology/topology.json` (`identity_status` per wire):

- Resolved: 40
- Conflicted: 0
- Unresolved: 0

## K. AP-WIRE conflict count

**0.** Corroborated three ways: 0 Conflicted wires (above), 0 conflicted
entries in every `conductor_boundaries` sub-count (`boundary`, `component`,
`terminal`, `connector`, `connector_terminal`), and 0 conflicted
`symbol_families`/`wire_semantics` entries in the audit.

## L. Electrical Net count

**10** (`electrical_nets` in the audit and in `topology.json`).

## M. Validation error count

**0** (`validation.errors` in `extraction_audit.json`).

## N. Warning count

- **Compiler warnings**: 7, all pre-existing and unrelated to
  AP-TEST-FIX-001/AP-WIRE-FIX-003 (5 `-Wunused-function` in production
  `src/image/` and `src/export/` files predating this work; 2
  `-Wunused-result` on `[[nodiscard]]` returns in two pre-existing test
  files). Identical set, same files/lines, before and after this task's
  clean rebuild.
- **Runtime validation warnings**: 30, matching the expected baseline
  exactly — `NET-ROLE-UNRESOLVED` × 6, `WIRE-GEOMETRIC-ENDPOINTS` × 24
  (from `extraction_audit.json`'s `validation.warning_codes`).

## O. Determinism result

Two fresh extractions of `samples/trx300ODG.png`, from the same clean
build, were compared file-by-file (`diff -rq`) across their entire output
trees (audit JSON, engineering-diagram JSON, topology JSON, all
`extraction_review` PNGs, `wires.svg`, the manifest). Result: **zero
differences** between the two runs.

A separate comparison against the extraction produced during the
AP-WIRE-FIX-003 validation (a different session, ~36 minutes earlier)
found exactly one differing byte range across the entire tree: the
`generated_at` wall-clock timestamp field in
`artifacts/extraction_review/review_manifest.json`. Every other file,
including `extraction_audit.json` and `output/wires.svg`, was
byte-identical. This is the expected behavior of a metadata timestamp
field, not a determinism defect in the extraction itself.

## P. Artifact hashes

SHA-256, computed with `sha256sum` against the fresh baseline run
(`samples/trx300ODG.png` in, clean Release build):

| Artifact | SHA-256 |
|---|---|
| `artifacts/audit/extraction_audit.json` | `e3874ce465392cef73453bf570ddbeacf0fb8b5911c3a3a4f084c6982f63dd9e` |
| `output/wires.svg` | `eb0d87c8c6db457313693bcd119145aace405def19d85b71025251c4464833e5` |
| `artifacts/topology/topology.json` | `f1c210610b2ed60bb23e39da7f51480e102aff98642561d3671f12b53b5e2036` |
| `artifacts/engineering_diagram/engineering_diagram.json` | `7d56fb89d6353645224d02e8fee2b3542139df754ab277a708633511f5fa0f7d` |
| Input `samples/trx300ODG.png` | `a145fffeb4cc9261152930936737048e39e92b030de5c9427b8f60e427d5da61` |

These are reproducible from the byte-identical two-run comparison in (O);
any future extraction whose `extraction_audit.json` or `output/wires.svg`
hash differs from this table has changed the extraction's observable
output and warrants investigation before being accepted as a new
baseline.

## Q. Relevant existing AP specifications

- `docs/AP-WIRE-029_Conductor_Boundary_and_Wire_Identity.md` — governing
  Wire-identity semantics (authoritative; not restated here).
- `docs/AP-WIRE-030_Conductor_Boundary_and_Terminal_Resolution.md` —
  AP-WIRE-030 boundary resolution semantics.
- `docs/AP-WIRE-031_AAR.md` — the PhysicalWireIdentityReconstructor
  implementation this baseline's Wire output comes from.

## R. Known limitations

See section 11 below (test-infrastructure limitation) and:

- The TRX300 fixture does not exercise every topology shape the
  AP-WIRE-029 semantics describe (e.g. no `Junction` node exists in it;
  confirmed 0 instances in every prior AP-WIRE-028 through -031
  measurement) — synthetic unit tests, not TRX300, are the only coverage
  for those shapes.
- 30 pre-existing runtime validation warnings remain open
  (`NET-ROLE-UNRESOLVED`, `WIRE-GEOMETRIC-ENDPOINTS`); this baseline
  records their count as expected, not as resolved.
- 7 pre-existing compiler warnings (unused-function in production code,
  unused `[[nodiscard]]` results in two tests) remain open; out of scope
  for this and prior AP tasks.

## Semantic baseline (currently verified)

The following Wire-identity semantics are currently implemented and
verified by the test suite and the TRX300 measurements above. This is a
summary for orientation, not a redefinition — AP-WIRE-029
(`docs/AP-WIRE-029_Conductor_Boundary_and_Wire_Identity.md`) remains the
authoritative specification.

- A Wire is an endpoint-to-endpoint physical conductor identity.
- Splice is never a Wire endpoint.
- Junction is never a Wire endpoint.
- Crossing is never a Wire endpoint.
- Continuation nodes are traversable (a degree-2 node has exactly one
  possible next edge).
- Electrical Net identity and physical Wire identity are different
  concepts; neither implies the other.
- Shared conductor geometry may legitimately participate in multiple
  distinct endpoint-to-endpoint Wire identities (verified by
  `test_physical_wire_identity_reconstructor.cpp`'s shared-segment and
  multi-way-fork cases).
- Ambiguous physical identity is reported as `Conflicted` (or left with
  no Wire at all when there is zero evidence) rather than guessed by any
  proxy — never by shortest path, straightness, color, or net structure.
- Ambiguous fork expansion (AP-WIRE-FIX-003) is deterministic and occurs
  exactly once per `(topology node, ConductorSegment)` arrival context,
  regardless of which eligible endpoint's walk reaches it first, given a
  fixed, sorted seed order.

## Verified history

| AP | Purpose | Disposition | Commit | Changed production behavior? |
|---|---|---|---|---|
| AP-WIRE-031 | Implement physical Wire identity reconstruction (endpoint-to-endpoint, Splice/Junction/Crossing pass-through) | Shipped | `feec402` | Yes |
| AP-WIRE-TUNE-001 | Extraction parameter sensitivity study (19 params swept against TRX300) | Study only, no code change; baseline reconfirmed at 41 wires (pre-FIX-001/002) | `7208f10` | No |
| AP-WIRE-TUNE-002 | Forensic characterization of `gap_interpretation.maximum_gap` thresholds | Study only; found conflict "resolution" via this parameter is evidence suppression, not legitimate | `4528920` | No |
| AP-WIRE-TUNE-003 | Forensic root-cause of CONFLICT-02 | Study only; located defect in `SymbolGeometryExtractor` (overlapping component boxes) | `2f5a16a` | No |
| AP-WIRE-FIX-001 | Fix overlapping component geometry attribution (CONFLICT-02) | Shipped | `a45da38` | Yes |
| AP-WIRE-TUNE-004 | Classify remaining CONFLICT-01/03/04 | Study only; root-caused to `ShapeDetector::detect_circles()` bus-crossing misclassification | `33f2617` | No |
| AP-WIRE-FIX-002 | Fix bus-crossing circle false positives (CONFLICT-01/03/04) | Shipped | `b69abad` | Yes |
| AP-TEST-FIX-001 | Restore real `assert()` evaluation in Release test targets (NDEBUG had silently disabled it) | Shipped; fixed 5 stale test fixtures, found and reported the AP-WIRE-031 fork-dedup defect | `b284db1` | No (test infrastructure + test-fixture fixes only) |
| AP-WIRE-FIX-003 | Fix the AP-WIRE-031 multi-way-fork over-discovery defect AP-TEST-FIX-001 exposed | Shipped | `8a116ac` | Yes |
| AP-BASELINE-001 | Freeze this verified state as a reference baseline | This document | (this commit) | No (documentation only) |

## 11. Known test-infrastructure limitation

AP-TEST-FIX-001's `-UNDEBUG` protection is scoped to `CONFIG:Release`
only (`$<$<CONFIG:Release>:-UNDEBUG>` in `CMakeLists.txt`). A test build
configured as `RelWithDebInfo` or `MinSizeRel` would still define
`NDEBUG` with no override, silently disabling `assert()` again exactly as
the original AP-TEST-FIX-001 defect did for `Release`. Neither
configuration is currently part of this repository's verified,
assertion-enabled test matrix — only `Release` (this baseline) and
`Debug` (which never defines `NDEBUG`) are covered.

This is **not** fixed by AP-BASELINE-001, per that task's explicit scope
(documentation/baseline only, no build-system changes). It is recorded
here as a future test-infrastructure hardening item: extending the same
`get_property(DIRECTORY PROPERTY TESTS)` + `target_compile_options`
pattern to `RelWithDebInfo`/`MinSizeRel` if either configuration is ever
added to CI or local verification workflows.

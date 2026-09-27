# AP-DIAG-FIX-006 — Rejected Geometry Evidence Handoff Correction

## 1. Starting SHA

`5c949b3046ff5e4b61aa5eda8c88441dae80d6ed` (AP-DIAG-AUDIT-005). Verified
via `git rev-parse HEAD`, `git branch --show-current` (`main`),
`git status --short` (empty/clean) before any change was made.

## 2. Audit Finding Being Corrected

AP-DIAG-AUDIT-005 (`docs/AP-DIAG-AUDIT-005_Rejected_Geometry_Data_Flow.md`,
`artifacts/audit/rejected_geometry_data_flow.json`) conclusively
established, with runtime instrumentation (since fully reverted):

- `model.rejected_geometry` is correctly populated (10 entries unscoped,
  9 scoped).
- The **local** `rejected_geometry` vector passed into
  `TerminalLocationDetector::detect()` is empirically **empty**
  (`size()==0`) on this build, because it had already been moved into
  `model.rejected_geometry` beforehand.
- `TerminalLocationDetector::has_ownership_evidence()` (AP-DIAG-FIX-005)
  legitimately accepts an associated `RejectedGeometryEvidence` entry as
  sufficient terminal-ownership evidence.
- Exactly one endpoint, `endpoint-candidate-9da9c73ab52013a3`, has **zero**
  owned `SymbolPrimitive`s and depends entirely on two
  `ComponentAssociated` `RejectedGeometryEvidence` entries for its
  ownership evidence — so it never passed the ownership gate in
  production, resolving to `geometric`/unresolved instead of
  `component_terminal`.
- A controlled experiment proved this is the **only** consequence: no
  Wire, topology, or `ElectricalNet` result differs.
- Conclusion: **C. Observable production extraction defect.**

## 3. Root Cause

`src/pipeline/extraction_pipeline.cpp` moved the local `rejected_geometry`
vector into `model.rejected_geometry` **before** passing that same
(now moved-from) local variable into
`terminal_detector.detect(..., rejected_geometry, ...)`. The detector
therefore never received the evidence the pipeline had already computed
for it.

## 4. Exact Code Change

**Before** (`src/pipeline/extraction_pipeline.cpp`, prior to this AP):

```cpp
model.conductor_segments = normalized_segments;
model.rejected_geometry = std::move(rejected_geometry);
model.nodes = graph.nodes;
model.edges = graph.edges;
model.endpoint_candidates = endpoint_artifacts.candidates;

TerminalLocationDetector terminal_detector(config_.terminals);
const TerminalLocationArtifacts terminal_artifacts =
    terminal_detector.detect(
        component_candidates,
        endpoint_artifacts.candidates,
        rejected_geometry,
        model.symbol_primitives);
model.terminal_candidates = terminal_artifacts.candidates;
```

**After**:

```cpp
model.conductor_segments = normalized_segments;
model.nodes = graph.nodes;
model.edges = graph.edges;
model.endpoint_candidates = endpoint_artifacts.candidates;

// AP-DIAG-FIX-006: TerminalLocationDetector::has_ownership_evidence()
// accepts an associated RejectedGeometryEvidence entry as sufficient
// terminal ownership evidence, so it must consume the populated
// rejected_geometry evidence before that local vector is moved into
// model.rejected_geometry - AP-DIAG-AUDIT-005 confirmed the previous
// ordering left the detector with an empty (moved-from) vector.
TerminalLocationDetector terminal_detector(config_.terminals);
const TerminalLocationArtifacts terminal_artifacts =
    terminal_detector.detect(
        component_candidates,
        endpoint_artifacts.candidates,
        rejected_geometry,
        model.symbol_primitives);
model.terminal_candidates = terminal_artifacts.candidates;
model.rejected_geometry = std::move(rejected_geometry);
```

The only change is the position of the single statement
`model.rejected_geometry = std::move(rejected_geometry);`, moved to
occur immediately after `detect()` has consumed the local vector, and
after `model.terminal_candidates` has been assigned from its result. No
other statement, signature, or type changed.

### Data flow, before and after

```
BEFORE:

    rejected_geometry
        |
        +--> std::move --> model.rejected_geometry
        |
        +--> moved-from vector --> TerminalLocationDetector

AFTER:

    rejected_geometry
        |
        +--> TerminalLocationDetector
        |
        +--> std::move --> model.rejected_geometry
```

## 5. Why the Change Is Semantically Correct

- `rejected_geometry` (local) is fully produced and classified by the
  time `detect()` is called — nothing between its construction (pipeline
  lines populating it from `GeometryOwnershipClassifier`/
  `ConductorEvidenceEvaluator`, then `RejectedGeometryClassifier::classify()`
  enriching it in place) and the reordered `detect()` call reads or
  mutates it further, so moving the assignment later changes nothing
  about what `rejected_geometry` contains at the point `detect()` uses it.
- `model.rejected_geometry` still receives the exact same, complete data
  — the move simply happens after one additional read (`detect()`'s
  `const&` parameter), which is precisely what `std::move` followed by a
  read-only consumer is meant to support: the vector is read, then its
  ownership is transferred, with no copy and no evidence duplicated or
  discarded.
- No other code path reads the local `rejected_geometry` variable between
  the old and new positions of the move (confirmed: the only other
  statements in between are `model.nodes = graph.nodes;`,
  `model.edges = graph.edges;`, `model.endpoint_candidates = ...;`, and
  the `TerminalLocationDetector` construction/call — none reference
  `rejected_geometry`).
- `TerminalLocationDetector`, `TerminalRecognizer`,
  `RejectedGeometryEvidence`, `SymbolPrimitive` ownership rules, Wire
  reconstruction, `ElectricalNetResolver`, topology, component
  classification, and shape detection are all untouched — this is a
  pure data-flow ordering correction, exactly as the audit prescribed.

## 6. Test Added

`tests/test_rejected_geometry_handoff.cpp` (new) — a **production-path**
regression, not a manually-constructed-fixture test. It runs the real,
unmodified `ExtractionPipeline::run()` against the real canonical
`samples/trx300ODG.png` sample (loaded via the `DX_WIRE_SOURCE_DIR`
compile definition, matching the established pattern in
`tests/test_recognition_input_exporter.cpp`), using the exact same
`source_id` string (`"samples/trx300ODG.png"`) the canonical `dx-extract`
CLI uses — necessary because every generated ID is content-addressed from
`source_id`, so a different `source_id` string would produce different
IDs and the test would not find the target endpoint at all (this was
caught during test-first verification, see below).

The test asserts directly from the pipeline's real output:

1. `model.rejected_geometry` is non-empty (evidence was produced).
2. `endpoint-candidate-9da9c73ab52013a3` exists.
3. Its `kind` is `EndpointKind::ComponentTerminal`.
4. Its `confidence` is `ConfidenceClass::High`.
5. Its `component_id` is
   `component-candidate-shape-region-845947b0afd7451a`.
6. The attributed component owns **zero** `SymbolPrimitive`s (no
   fabricated primitive was introduced to make the test pass).
7. The attributed component has an associated `RejectedGeometryEvidence`
   entry (the real, pre-existing evidence driving the result).

**Test-first verification**: the test was run against the pre-fix
ordering (via `git stash` of only `src/pipeline/extraction_pipeline.cpp`,
rebuild, run, then `git stash pop` to restore the fix) and **failed** —
first with "endpoint not found" (because an initial draft used an
arbitrary `source_id` of `"trx300"`, which produces different
content-addressed IDs — corrected to the exact literal string
`dx-extract` uses, `"samples/trx300ODG.png"`), then, after that
correction, with `assert(target->kind == EndpointKind::ComponentTerminal)`
failing (the endpoint was found with `kind=GeometricConductorEnd`,
confirming the actual defect). After restoring the fix, the test passes.

`tests/test_terminal_location_detector.cpp` and
`tests/test_terminal_ownership_evidence.cpp` (AP-DIAG-FIX-005's own
tests) are unmodified and continue to pass — this AP does not touch
`TerminalLocationDetector`'s ownership logic itself.

Registered in `CMakeLists.txt` as `dx-wire-test-rejected-geometry-handoff`,
following the same `DX_WIRE_SOURCE_DIR` compile-definition pattern as
`dx-wire-test-recognition-input-exporter`.

## 7. Before/After Endpoint Result

| Field | Before | After |
|---|---|---|
| `kind` | `geometric` | `component_terminal` |
| `terminal_role` | `unknown` | `component_terminal` |
| `confidence` | `low` | `high` |
| `component_id` | `""` | `component-candidate-shape-region-845947b0afd7451a` |

Exactly matches the AP-DIAG-AUDIT-005 controlled-experiment prediction.

## 8. Before/After Global Metrics (TRX300, unscoped)

Compared via an isolated `git worktree` rebuild of pre-fix `5c949b3`
(removed after use; `git worktree list` confirms only `main` remains)
against a fresh post-fix extraction:

| Metric | Before | After | Identical? |
|---|---:|---:|---|
| `component_candidates` | 81 | 81 | byte-identical |
| `component_symbol_recognitions` | 81 | 81 | byte-identical |
| `symbol_primitives` | 34 | 34 | byte-identical |
| `nodes` | 686 | 686 | byte-identical |
| `edges` | 876 | 876 | byte-identical |
| `wires` | 35 | 35 | byte-identical |
| `wire_semantics` | 35 | 35 | byte-identical |
| `electrical_nets` | 10 | 10 | byte-identical |
| `connector_candidates` / `connector_terminals` | 0 / 0 | 0 / 0 | byte-identical |
| `rejected_geometry` (model's own copy) | 10 | 10 | byte-identical |
| `endpoint_candidates` | 203 | 203 | **1 entry differs** (§7) |
| `conductor_boundary_evidence` | 218 | 219 | +1 (new `terminal_candidate` evidence entry for the corrected endpoint) |
| `component_terminal` endpoints | 11 | 12 | +1 |
| `geometric` endpoints | 188 | 187 | -1 |
| `ground` endpoints | 4 | 4 | unchanged |
| ChassisGround (6) | 6 | 6 | unchanged |
| Ground-role nets (4) | 4 | 4 | unchanged |
| Validation errors | 0 | 0 | unchanged |
| Validation warnings | 34 | 34 | **unchanged** — the corrected endpoint is not attached to any Wire, so `WIRE-GEOMETRIC-ENDPOINTS` (the only warning code sensitive to endpoint kind) is unaffected |

No other difference exists anywhere in the whole-diagram inventory.

## 9. Scoped Result

Identical pattern, confirmed via the same before/after comparison against
`fixtures/trx300/scope_production.json`:

- `rejected_geometry` (model): 9 (unchanged, both before/after).
- `component_terminal`: 11 → 12; `geometric`: 170 → 169.
- The same single endpoint, `endpoint-candidate-9da9c73ab52013a3`,
  changes identically (`component_terminal`, `high`, same component).
- `wires` (35), `wire_semantics`, `electrical_nets` (10),
  `component_candidates` (34), `symbol_primitives` (34), `nodes` (528),
  `edges` (641) all byte-identical before/after.
- No unexpected differences.

## 10. Unscoped Result

See §8 in full — `rejected_geometry` (model): 10 (unchanged). Same single
endpoint changes identically. No unexpected differences.

## 11. Determinism

- Two unscoped runs: `topology.json` byte-identical; only
  `extraction_review/review_manifest.json`'s documented volatile
  `generated_at` field differed.
- Two scoped runs: `topology.json` byte-identical.
- The corrected endpoint attribution (`kind`, `confidence`,
  `component_id`) is identical across all runs and both scopes.

## 12. Warning Changes

**None.** Compiler warnings: exactly 8 pre-existing, verified via a full
from-scratch clean rebuild — 0 new. Runtime validation warnings: 34
before and after — unchanged, because `WIRE-GEOMETRIC-ENDPOINTS` (the
only validator sensitive to endpoint `kind`) only fires for a Wire whose
*both* endpoints are geometric, and the corrected endpoint is not an
endpoint of any Wire at all (confirmed dangling, unchanged from
AP-DIAG-AUDIT-004/FIX-005/AUDIT-005).

## 13. Invariants Verified

- Physical Wire records: byte-identical, all 35, including `Wire::id`,
  endpoints, `topology_edges`, `conductor_segments`, `identity_status`,
  `identity_evidence_ids`.
- `wire_semantics`: byte-identical (35 entries).
- Topology nodes and edges: byte-identical (686/876 unscoped, 528/641
  scoped).
- Conductor segments: unaffected (not touched by this change at all —
  confirmed no reference to `conductor_segments` near the moved
  statement).
- Electrical nets: byte-identical (10, including all 4 ground-role and
  6 unknown-role).
- Component candidates and symbol primitives: byte-identical.
- ChassisGround recognitions: byte-identical (6).

## 14. Explicitly Out-of-Scope Findings (not touched)

- Ground endpoint coverage gap (2 of 6 ChassisGround components without
  a resolved Ground endpoint) — unrelated root cause, untouched.
- Any terminal-attribution issue unrelated to `rejected_geometry`.
- ChassisGround classification, `ShapeDetector` behavior.
- Connector taxonomy (still 0/0, unfabricated).
- OCR / text recognition.
- Source scoping behavior.
- Wire deduplication / physical Wire identity (AP-DIAG-FIX-004).
- `ElectricalNetResolver` semantics.

## 15. Final Commit

See report below for the exact SHA.

## 16. Push Status

See report below.

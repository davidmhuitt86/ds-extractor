# AP-DIAG-AUDIT-005 — Rejected Geometry Evidence Data-Flow Audit

This is an audit document. No production classifier, extractor, topology,
terminal-resolver, Wire, ground, scope, or model code was modified to
produce it. A controlled, temporary diagnostic instrumentation was added
to `src/pipeline/extraction_pipeline.cpp` and
`src/topology/terminal_location_detector.cpp` to empirically observe
runtime state, then fully reverted (`git status --short` confirmed clean,
byte-for-byte identical to HEAD, before any committed change was made).

## 1. Executive Summary

The moved-from `rejected_geometry` local vector that AP-DIAG-FIX-005
flagged as a side finding is a **real, observable production extraction
defect**, not merely a latent or theoretical one. On this build's actual
C++ standard library implementation, the vector is empirically confirmed
**empty** (not just "valid but unspecified") immediately after the move,
and `TerminalLocationDetector::detect()` receives that empty vector in
every real extraction run. A controlled experiment (Control A: current
committed behavior; Control B: the same extraction with the ordering
corrected so `detect()` receives the fully populated vector) proves this
materially changes output: exactly one endpoint
(`endpoint-candidate-9da9c73ab52013a3`) resolves to `component_terminal`
(high confidence) under Control B but resolves to `geometric`/unresolved
under Control A (current, committed behavior) — identically in both
unscoped and scoped extraction. No Wire, topology, or ElectricalNet
result is affected, because this endpoint is not part of any physical
Wire (dangling, as AP-DIAG-AUDIT-004 already established).

**Conclusion: C. Observable production extraction defect** (see §18-19).

## 2. Starting Repository State

- `git rev-parse HEAD` (before audit work): `754f0781365f411b0c463bf0b4e86097d5f13d36`
- `git branch --show-current`: `main`
- `git status --short` (before audit work): empty (clean)
- `git log -1 --oneline`: `754f078 AP-DIAG-FIX-005 — Terminal Attribution Ownership Evidence`
- No branch was created, switched, or reset. No `git worktree` was left
  behind (none was needed for this audit — no independent historical
  rebuild was required, since the defect was traced entirely within the
  current HEAD's own source).

## 3. AP-DIAG-FIX-005 Baseline (reconfirmed, not assumed)

Full clean Release rebuild + `ctest`, then a fresh canonical unscoped
extraction:

| Metric | Value |
|---|---:|
| Tests | 57/57 passing |
| Assertions | active (`dx-wire-test-assertions-enabled` passing) |
| Compiler warnings | 8 (pre-existing, unchanged) |
| Wire records | 35 |
| Electrical nets | 10 |
| ChassisGround entries | 6 |
| Ground-role nets | 4 |
| `component_terminal` endpoints | 11 |
| Validation errors | 0 |
| Validation warnings | 34 |

All match the AP-DIAG-FIX-005 stated baseline exactly.

## 4. RejectedGeometryEvidence Definition

`include/eke_dx_wire/core/model.hpp:73-81`:

```cpp
enum class RejectedGeometryClass {
    ComponentAssociated,
    ConnectorAssociated,
    TextAssociated,
    Unresolved
};

struct RejectedGeometryEvidence {
    std::string id;
    Segment2D geometry {};
    std::string reason;
    double measurement = 0.0;
    Provenance provenance {};
    RejectedGeometryClass classification = RejectedGeometryClass::Unresolved;
    std::string associated_object_id;
};
```

`WireModel::rejected_geometry` (`include/eke_dx_wire/core/model.hpp:895`)
is `std::vector<RejectedGeometryEvidence>`. Each record represents a piece
of line-like geometry that was **excluded** from conductor topology
because it was determined to be owned by something else (a component's
interior/boundary, connector geometry, or text) rather than a real
electrical conductor — `reason` records the specific rule that rejected
it (`graphical_object_ownership_component_overlap`,
`graphical_object_ownership_component_boundary`,
`graphical_object_ownership_text_overlap`, or
`insufficient_supporting_ink`), and `classification`/`associated_object_id`
record which object it was attributed to.

## 5. Producer Inventory

- **`GeometryOwnershipClassifier::classify()`**
  (`src/image/geometry_ownership_classifier.cpp:337-360` — component/text
  overlap; `:372-388` — component-boundary-trace, AP-DIAG-FIX-001) —
  produces `GeometryOwnershipArtifacts::rejected`.
- **`ConductorEvidenceEvaluator::evaluate()`**
  (`src/image/conductor_evidence_evaluator.cpp:79-101`) — produces
  `ConductorEvidenceArtifacts::rejected` (e.g.
  `insufficient_supporting_ink`).
- **`RejectedGeometryClassifier::classify()`**
  (`src/image/rejected_geometry_classifier.cpp`, declared
  `include/eke_dx_wire/image/rejected_geometry_classifier.hpp:18-21`) —
  **not a producer of new records**; it takes
  `std::vector<RejectedGeometryEvidence>&` **by mutable reference** and
  enriches existing entries in place (this is confirmed by its signature
  — it returns `void` and takes the vector by reference, not by value or
  const reference).

## 6. Consumer Inventory

- **`TerminalLocationDetector::detect()`**
  (`src/topology/terminal_location_detector.cpp`) — the consumer this
  audit is centered on (§10).
- **`topology_exporter.cpp:321-336`** — serializes `model.rejected_geometry`
  (the model's own copy, populated via the move at
  `extraction_pipeline.cpp:253`) into `artifacts/topology/topology.json`'s
  `rejected_geometry` array. This consumer is unaffected by the defect —
  it reads from `model.rejected_geometry`, never from the moved-from local.
- **Tests**: `tests/test_rejected_geometry_classifier.cpp` (producer/
  enrichment logic only), `tests/test_geometry_ownership_classifier.cpp`
  (producer logic only), `tests/test_terminal_location_detector.cpp`
  (one case, `connector_body`, constructs `RejectedGeometryEvidence`
  directly and passes it straight to `detect()` — see §14).
- **`TerminalRecognizer::recognize()`** — **not a consumer**. Confirmed
  by exhaustive grep: `rejected_geometry`/`RejectedGeometryEvidence`
  appear zero times in `src/topology/terminal_recognizer.cpp` or
  `include/eke_dx_wire/topology/terminal_recognizer.hpp`. Its AP-DIAG-FIX-005
  ownership check depends solely on `SymbolPrimitive` ownership.

## 7. Complete Data-Flow Trace

```
raster/input (samples/trx300ODG.png)
   |
   v
ShapeDetector / normalization  (unrelated to rejected_geometry)
   |
   v
GeometryOwnershipClassifier::classify()          [src/image/geometry_ownership_classifier.cpp:337-388]
   -> GeometryOwnershipArtifacts::rejected
   |
   +--> ConductorEvidenceEvaluator::evaluate()    [src/image/conductor_evidence_evaluator.cpp:79-101]
   |      -> ConductorEvidenceArtifacts::rejected
   v
extraction_pipeline.cpp:128-133
   std::vector<RejectedGeometryEvidence> rejected_geometry =
       ownership.rejected;
   rejected_geometry.insert(..., evidence.rejected...);
   |
   v
RejectedGeometryClassifier::classify(rejected_geometry, ...)   [extraction_pipeline.cpp:135-140]
   (mutates rejected_geometry IN PLACE - enriches classification/
    associated_object_id fields; does not add or remove entries)
   |
   v
local `rejected_geometry` now fully populated + classified (10 entries, unscoped TRX300;
9 entries, scoped)
   |
   +-----------------------------+
   |                              |
   | model.rejected_geometry =    | terminal_detector.detect(
   |   std::move(rejected_geometry)|     ...,
   | [extraction_pipeline.cpp:253] |     rejected_geometry,   <- SAME local variable,
   |                              |     model.symbol_primitives)
   v                              |     [extraction_pipeline.cpp:258-264]
model.rejected_geometry           |
(fully populated: 10/9 entries)   v
   |                          TerminalLocationDetector::detect()
   v                          receives EMPTY vector (observed
topology_exporter.cpp:321-336     size()==0 on this build - see §9)
(serializes into
 topology.json's
 "rejected_geometry" array,
 correct/unaffected)
```

**The move statement is `extraction_pipeline.cpp:253`
(`model.rejected_geometry = std::move(rejected_geometry);`). The
subsequent use of the same, now-moved-from local `rejected_geometry`
variable is `extraction_pipeline.cpp:263`, ten lines later, as the third
argument to `terminal_detector.detect(...)`.**

## 8. Move/Copy/Reference Analysis

- `rejected_geometry` (local) is first **copy-constructed** from
  `ownership.rejected` at `extraction_pipeline.cpp:128-129`.
- It is then extended via `insert()` (copies) from `evidence.rejected` at
  `:130-133`.
- It is passed **by mutable reference** into
  `RejectedGeometryClassifier::classify()` at `:135-140` and mutated in
  place (no copy, no move).
- It is **move-assigned** into `model.rejected_geometry` at `:253`.
- The same local variable is then passed **by const reference**
  (`const std::vector<RejectedGeometryEvidence>&`, per
  `TerminalLocationDetector::detect()`'s signature) into `detect()` at
  `:263` — after having already been moved from.
- `model.rejected_geometry`'s exact type: `std::vector<RejectedGeometryEvidence>`
  (member of `WireModel`, `include/eke_dx_wire/core/model.hpp:895`) —
  identical type to the local; the move-assignment is a same-type vector
  move-assignment, not a conversion.

## 9. C++ Move Semantics

- **C++ language/library guarantee (A)**: `std::vector`'s move assignment
  operator leaves the source object in a "valid but unspecified state"
  ([container.requirements.general], via the Allocator-aware container
  move-assignment requirements). The C++ standard does **not** guarantee
  the moved-from vector is empty.
- **Observed state on the actual implementation (B)**: this repository
  builds with libstdc++ (GCC). `std::vector`'s move assignment operator in
  libstdc++ swaps/transfers the internal begin/end/capacity pointers and
  leaves the source with null pointers — `size() == 0` and `empty() == true`
  after the move. This was **empirically confirmed**, not assumed: a
  temporary diagnostic (`std::fprintf` before/after the move, and inside
  `detect()`) was added, the project rebuilt, and a real extraction run
  observed directly:

  ```
  AUDIT005 pre-move rejected_geometry.size()=10
  AUDIT005 post-move local rejected_geometry.size()=0 empty=1
  AUDIT005 model.rejected_geometry.size()=10
  AUDIT005 pre-detect() local rejected_geometry.size()=0
  AUDIT005 detect() received rejected_geometry.size()=0 symbol_primitives.size()=34 components.size()=81
  ```

  (scoped extraction reproduced the identical pattern: pre-move 9,
  post-move 0, `detect()` receives 0 — see §16).

- **State required by `TerminalLocationDetector` (C)**: `detect()`
  declares `rejected_geometry` as `const std::vector<RejectedGeometryEvidence>&`
  with a default of `{}` — an empty vector is a **valid, accepted** input
  (it is optional evidence, not a required precondition; see §12). The
  function does not crash, assert, or misbehave on empty input — it
  simply finds no matching ownership evidence via that channel.
- **Evidence actually available through the production extraction path
  (D)**: currently **none** — `detect()` always receives `size()==0` for
  this parameter in the compiled, committed pipeline, regardless of how
  many real `RejectedGeometryEvidence` records exist in
  `model.rejected_geometry` for the same extraction.
- **Observable extraction consequences (E)**: real, but narrow — see
  §15, exactly one endpoint's classification differs.

This audit explicitly distinguishes: **"valid but unspecified"** (A, the
only thing the C++ standard promises) from **"guaranteed empty"** (not
true — no such guarantee exists in the standard) from **"observed empty
on this implementation"** (B — true, confirmed by direct instrumentation
on the actual compiler/standard-library combination this project builds
with; this is what makes the defect not merely theoretical).

## 10. TerminalLocationDetector Analysis

`include/eke_dx_wire/topology/terminal_location_detector.hpp:30-34` /
`src/topology/terminal_location_detector.cpp`:

```cpp
[[nodiscard]] TerminalLocationArtifacts detect(
    const std::vector<ComponentCandidate>& components,
    const std::vector<EndpointCandidate>& endpoints,
    const std::vector<RejectedGeometryEvidence>& rejected_geometry = {},
    const std::vector<SymbolPrimitive>& symbol_primitives = {}) const;
```

Two distinct uses inside `detect()`:

1. **`has_ownership_evidence()`** (added by AP-DIAG-FIX-005) iterates
   `rejected_geometry` looking for a `ComponentAssociated`/
   `ConnectorAssociated` entry whose `associated_object_id` matches the
   candidate component. **Condition when empty**: this loop simply never
   matches via this channel — `has_ownership_evidence()` falls through to
   its `SymbolPrimitive` check; if that also fails, the component is
   skipped entirely (no candidate for it). **Condition when populated
   with matching evidence**: the component passes the ownership gate
   (`return true` inside the loop) even if it owns zero `SymbolPrimitive`s.
   **Result can change**: yes — confirmed empirically (§9, §15).
2. **`attachment_distance()`**'s rejected-geometry-narrowing branch
   (pre-existing, AP-GEOMETRY-005) additionally shrinks the computed
   distance to a component when a `ComponentAssociated`/
   `ConnectorAssociated` entry's geometry is closer than the raw
   bounding-box distance. **Condition when empty**: this `min()` never
   fires; only the raw bounding-box distance is used. **Condition when
   populated**: distance can be smaller, which can raise the resulting
   `ConfidenceClass` (e.g. `low`→`high`) for a component that already
   passed the ownership gate.

Both branches only ever **iterate** `rejected_geometry`
(`for (const auto& evidence : rejected_geometry)`) — the function does
not require it to be non-empty, does not assert on it, and treats it
purely as optional supplemental evidence (see §12). The result **does**
reach `EndpointCandidate` (via `TerminalCandidate` →
`TerminalSemanticEvidenceBuilder` → `EndpointSemanticReconstructor` →
`model.endpoint_candidates`, confirmed in §7 of AP-DIAG-FIX-005's own
trace and reconfirmed here by direct JSON diff, §15). It can affect Wire
semantics (`wire_semantics.start/end_component_id`) **only if** the
affected endpoint is itself a Wire endpoint — for the one endpoint this
audit found affected, it is not (§13).

## 11. TerminalRecognizer Analysis

Confirmed by exhaustive search (`grep -c "rejected_geometry\|RejectedGeometry" src/topology/terminal_recognizer.cpp include/eke_dx_wire/topology/terminal_recognizer.hpp` → 0 matches): `TerminalRecognizer` neither directly nor indirectly consumes `RejectedGeometryEvidence`. Its `recognize()` signature has no such parameter, and its AP-DIAG-FIX-005 ownership check (`owned_primitives.empty()`) is based exclusively on `SymbolPrimitive` ownership. **The AP-DIAG-FIX-005 ownership rule's `TerminalRecognizer` half has zero dependency on `rejected_geometry`; only its `TerminalLocationDetector` half does.**

## 12. AP-DIAG-FIX-005 Ownership-Evidence Interaction

Tracing the full chain `SymbolPrimitive` / `RejectedGeometryEvidence` /
`TerminalLead` / component bounding box / component ownership / conductor
segment / `TerminalCandidate` / `EndpointCandidate` /
`EndpointSemanticReconstructor`:

- `TerminalRecognizer`'s first phase matches an owned
  `SymbolPrimitiveKind::TerminalLead` to the nearest endpoint — this is
  genuine, mandatory ownership evidence, unrelated to `RejectedGeometryEvidence`.
- `TerminalRecognizer`'s boundary-alignment fallback (post-FIX-005)
  requires the component to own **at least one** `SymbolPrimitive` of any
  kind, plus a conductor-direction alignment check — again, no
  `RejectedGeometryEvidence` involvement.
- `TerminalLocationDetector::has_ownership_evidence()` treats an owned
  `SymbolPrimitive` and an associated `RejectedGeometryEvidence` entry as
  **equally sufficient, alternative (OR'd)** forms of ownership evidence
  — neither is individually mandatory; either suffices.

Classifying `RejectedGeometryEvidence` per the task's taxonomy (based on
the actual algorithm, not the name): it is **(B) optional corroborating
evidence** at the `TerminalLocationDetector` ownership gate — one of two
alternative sufficient conditions, not a required one (a component can
still pass via `SymbolPrimitive` ownership alone). It is not (A)
mandatory (the gate already has an independent, sufficient path via
`SymbolPrimitive`), not (C) exclusion evidence or (D) rejection evidence
in the sense of vetoing an otherwise-valid candidate (the code has no
"reject if present" logic — quite the opposite, its presence can only
ever add a candidate or raise confidence, never remove one), and not (E)
merely diagnostic (§9-10 and §15 prove it materially changes committed
extraction output when actually supplied).

## 13. endpoint-candidate-9da9c73ab52013a3 Forensics

1. **Producer**: `TerminalLocationDetector::detect()`, via the ownership
   gate's `RejectedGeometryEvidence` branch (confirmed by Control B's
   `conductor-boundary-evidence` entry
   `terminal-candidate-endpoint-candidate-9da9c73ab52013a3:component-candidate-shape-region-845947b0afd7451a`
   — the `terminal-candidate-` ID prefix is `TerminalLocationDetector`'s,
   per AP-DIAG-FIX-004's established naming convention).
2. **Evidence**: the endpoint (149.5, 181) lies interior to
   `component-candidate-shape-region-845947b0afd7451a`'s bounding box
   (`allow_interior_attachment` → distance 0.0 → `high` confidence). This
   component owns **zero** `SymbolPrimitive`s but has **two**
   `ComponentAssociated` `RejectedGeometryEvidence` entries
   (`conductor-segment-6f10027fd45a78e2`, `conductor-segment-d0e51de9a7d54ea2`
   — both `reason=graphical_object_ownership_component_boundary`) —
   confirmed directly against `artifacts/audit/terminal_attribution_inventory.json`
   and `terminal_attribution_findings.json` (AP-DIAG-AUDIT-004).
3. **Does `rejected_geometry` participate?** Yes — it is the **only**
   ownership evidence this component has (zero `SymbolPrimitive`s), so
   without it, this component fails `has_ownership_evidence()` entirely.
4. **Does the production path supply `rejected_geometry`?** No — confirmed
   empirically (§9): `detect()` receives an empty vector in every real
   extraction run.
5. **Would the candidate exist if `rejected_geometry` were populated?**
   Yes — confirmed directly by the Control A/B experiment (§15): under
   Control B (populated), this exact endpoint resolves to
   `component_terminal`, `high` confidence, attributed to
   `component-candidate-shape-region-845947b0afd7451a`.
6. **Disposition**: this is a **direct, confirmed consequence of the
   missing `rejected_geometry` evidence** — not an independent or
   unrelated AP-DIAG-FIX-005 side effect, and not indeterminate.
   AP-DIAG-FIX-005's own documentation speculated this exact outcome
   ("if reachable, would have preserved this specific endpoint via its
   two `component_associated` conductor-segment evidence entries") — this
   audit confirms that speculation was correct. The endpoint currently
   resolves to `geometric`/unresolved (component_id empty) in the
   committed, actual pipeline output; it is not attached to any physical
   Wire (dangling — confirmed unchanged from AP-DIAG-AUDIT-004/FIX-005),
   so this has no Wire, topology, or net-level consequence.

## 14. Unit-Test vs Production-Path Comparison

| Test | Constructs evidence | Passed to | Exercises pipeline handoff? |
|---|---|---|---|
| `tests/test_rejected_geometry_classifier.cpp` | Directly | `RejectedGeometryClassifier::classify()` only | No — producer/enrichment logic only |
| `tests/test_geometry_ownership_classifier.cpp` | N/A (produces it) | N/A | No — producer logic only |
| `tests/test_terminal_location_detector.cpp` (`connector_body` case) | Directly (manual fixture) | `TerminalLocationDetector::detect()` directly | **No** — bypasses `ExtractionPipeline` entirely; the vector is a fresh, non-moved-from local in the test |
| `tests/test_terminal_ownership_evidence.cpp` (AP-DIAG-FIX-005) | N/A for this parameter (tests only pass `{}` or omit it) | `TerminalLocationDetector::detect()` / `TerminalRecognizer::recognize()` directly | No — and does not exercise the `RejectedGeometryEvidence` half of the ownership gate at all |

**No test in this repository exercises the actual production handoff**
(`ExtractionPipeline` → `SymbolGeometryExtractor`/`GeometryOwnershipClassifier`
→ `TerminalLocationDetector`) with real `RejectedGeometryEvidence` data —
this matches the established pattern (confirmed in AP-DIAG-FIX-004/005)
that this codebase has no integration test running the full
`ExtractionPipeline` against a real sample image; all tests are
unit-level with synthetic fixtures. The `test_terminal_location_detector.cpp`
`connector_body` case **proves the ownership-gate logic is correct** when
given real evidence — it does not, and cannot, prove that evidence
reaches the function in production. This is exactly the discrepancy the
task asked to identify.

## 15. Controlled Comparison Experiment

**Temporary diagnostic change** (fully reverted before commit; `git status --short`
confirmed empty afterward): in `src/pipeline/extraction_pipeline.cpp`,
the `model.rejected_geometry = std::move(rejected_geometry);` statement
was temporarily moved to **after** the `terminal_detector.detect(...)`
call (Control B), rebuilt, and run against the canonical TRX300 input.
Control A is the current, actual, committed production behavior
(unmodified). Both were run against the same `dx-extract` binary family,
same input, same scope conditions.

| Category | Control A (current) | Control B (populated) | Identical? |
|---|---:|---:|---|
| `component_candidates` | 81 | 81 | Yes |
| `component_symbol_recognitions` | 81 | 81 | Yes |
| `symbol_primitives` | 34 | 34 | Yes |
| `nodes` | 686 | 686 | Yes |
| `edges` | 876 | 876 | Yes |
| `conductor_boundary_resolutions` | 203 | 203 | **No** — 1 entry differs (the affected endpoint's resolution) |
| `conductor_boundary_evidence` | 218 | 219 | **No** — Control B has 1 additional entry (a new `terminal_candidate` evidence record) |
| `wires` | 35 | 35 | Yes, byte-identical |
| `wire_semantics` | 35 | 35 | Yes, byte-identical |
| `electrical_nets` | 10 | 10 | Yes, byte-identical |
| `endpoint_candidates` | 203 | 203 | **No** — exactly 1 endpoint differs |
| `rejected_geometry` (model's own copy) | 10 | 10 | Yes — the model's copy is always correct; only the detector's *input* differed |
| Validation errors | 0 | 0 | Yes |
| Validation warnings | 34 | (not separately re-tallied; the one changed endpoint is not part of any Wire, so `WIRE-GEOMETRIC-ENDPOINTS` is unaffected) | — |

The single difference, in full:

```
Control A: endpoint-candidate-9da9c73ab52013a3
  kind=geometric, terminal_role=unknown, confidence=low, component_id=""

Control B: endpoint-candidate-9da9c73ab52013a3
  kind=component_terminal, terminal_role=component_terminal,
  confidence=high, component_id="component-candidate-shape-region-845947b0afd7451a"
```

Control B was re-run once more and confirmed deterministic
(topology.json byte-identical across the two runs).

**The experiment required no permanent instrumentation.** All temporary
`std::fprintf` diagnostics and the temporary statement-reordering were
reverted via restoring the original files from a pre-edit backup, and
`git status --short`/`git diff --stat` confirmed a byte-for-byte clean
working tree before any committed change was made.

## 16. Scoped vs Unscoped Results

| | Unscoped | Scoped |
|---|---:|---:|
| `rejected_geometry` count (model) | 10 | 9 |
| `detect()`-received count (Control A, actual production) | 0 | 0 |
| `detect()`-received count (Control B) | 10 | 9 |
| Endpoint affected | `endpoint-candidate-9da9c73ab52013a3` | same endpoint, same component, identical before/after states |
| Wire/topology/net differences | none | none |

The defect and its single observable consequence are **identical in both
scopes** — the condition is not scope-dependent, because the move/reuse
bug is entirely a pipeline-sequencing issue, unrelated to scoping logic.

## 17. Related Moved-From Evidence Handoffs

Searched every `= std::move(...)` in `src/pipeline/extraction_pipeline.cpp`,
`src/topology/*.cpp`, `src/image/*.cpp`, `src/export/*.cpp` for the same
`local_container -> std::move(local_container) -> subsequent consumer(local_container)`
pattern:

| Location | Pattern | Classification |
|---|---|---|
| `extraction_pipeline.cpp:253` (this audit's subject) | `model.rejected_geometry = std::move(rejected_geometry)`, then `rejected_geometry` reused at `:263` | **CONFIRMED DEFECT** |
| `extraction_pipeline.cpp:233` | `model.text_recognition_evidence.push_back(std::move(accepted))` inside a per-iteration loop; `accepted` is freshly declared each iteration and never read again in that iteration | SAFE |
| `extraction_pipeline.cpp:517` | `model.wires.push_back(std::move(wire))` inside a per-iteration `for (auto wire : ...)` loop; `wire` never read again in that iteration | SAFE |
| `component_identity_evidence_builder.cpp:81` | `strongest[key] = std::move(evidence)`; `key` was already computed from `evidence`'s fields before the move; `evidence` not read again in that iteration | SAFE |
| `component_identity_resolver.cpp:75` | `resolution.evidence_ids = std::move(evidence_ids)`; local `evidence_ids` not referenced again afterward | SAFE |
| `electrical_net_resolver.cpp:63` | `result.wires = std::move(decomposed.wires)`; only `decomposed.nets` (a different member) is read afterward, never `decomposed.wires` | SAFE |
| `electrical_net_resolver.cpp:125` | `result.nets = std::move(normalized_nets)`; `normalized_nets` not referenced again afterward | SAFE |

**No other occurrence of this defect pattern exists in the codebase.**
`extraction_pipeline.cpp:253`/`:263` is the sole confirmed instance.

## 18. Findings

| ID | Finding | Severity | Evidence |
|---|---|---|---|
| AUDIT-005-001 | `extraction_pipeline.cpp` passes a moved-from `rejected_geometry` vector to `TerminalLocationDetector::detect()`; empirically confirmed empty (size 0) on this build in every real extraction run (unscoped and scoped) | MEDIUM | §9, §15, §16 — direct runtime instrumentation |
| AUDIT-005-002 | This causes one specific, confirmed endpoint (`endpoint-candidate-9da9c73ab52013a3`) to resolve to `geometric`/unresolved in production when it would resolve to `component_terminal` (high confidence) if the ownership evidence it depends on (its only ownership evidence — it owns zero `SymbolPrimitive`s) reached the detector | MEDIUM | §13, §15 — Control A/B diff |
| AUDIT-005-003 | No Wire, topology, or ElectricalNet result is affected — the one impacted endpoint is not part of any physical Wire | none (informational) | §15 — `wires`/`wire_semantics`/`electrical_nets` byte-identical between Control A and B |
| AUDIT-005-004 | No unit test in the repository exercises the real pipeline handoff of `RejectedGeometryEvidence` into `TerminalLocationDetector` — all existing coverage uses manually constructed fixtures passed directly to the function under test | LOW | §14 |
| AUDIT-005-005 | `TerminalRecognizer`'s AP-DIAG-FIX-005 ownership rule has zero dependency on `rejected_geometry` and is entirely unaffected by this defect | none (informational) | §11 |

## 19. Severity Classification

- **AUDIT-005-001/002 = MEDIUM**: this affects a legitimate engineering
  object's semantic classification (a real endpoint's terminal identity),
  but the impact is localized — a single endpoint, not attached to any
  Wire, with zero downstream engineering consequence in the currently
  emitted model. This is neither CRITICAL (does not corrupt topology or
  Wire semantics) nor HIGH (does not affect multiple engineering objects
  or create a systematic false attribution — it is the opposite of
  AP-DIAG-AUDIT-004's finding: this is evidence being **lost**, not
  fabricated) nor LOW (it is not merely "isolated attribution uncertainty
  with no downstream engineering impact" — it is a confirmed, reproducible
  discrepancy between the evidence the architecture computed and the
  evidence the architecture actually used, on a specific, named,
  real endpoint).
- **AUDIT-005-004 = LOW**: a test-coverage gap, not an extraction defect
  in itself.

## 20. Downstream Impact

`NO DOWNSTREAM IMPACT` beyond the one endpoint's own classification field
(`kind`, `terminal_role`, `confidence`, `component_id`). No topology node
or edge changes (the endpoint's node already exists regardless of its
terminal classification). No Wire is created, removed, or modified — this
endpoint is dangling in both Control A and Control B. No ElectricalNet
role or membership changes. `WIRE-GEOMETRIC-ENDPOINTS`/`NET-ROLE-UNRESOLVED`
validation warning counts are unaffected (the endpoint's dangling status
means it is not evaluated by wire-level validators).

## 21. Determinism

- Control A (current production): already established deterministic by
  AP-DIAG-FIX-005; reconfirmed here (two runs, byte-identical
  `topology.json`, differing only in `review_manifest.json`'s documented
  volatile `generated_at` field — not re-shown here, see AP-DIAG-FIX-005 §12).
- Control B (temporary diagnostic ordering): run twice; `topology.json`
  byte-identical across both runs.
- Both scoped and unscoped extractions reproduce the identical single-endpoint
  delta between Control A and B.

## 22. Recommended Next AP

**AP-DIAG-FIX-006 — Rejected Geometry Evidence Handoff Correction**: move
the `model.rejected_geometry = std::move(rejected_geometry);` statement
in `extraction_pipeline.cpp` to occur **after** the
`terminal_detector.detect(...)` call (exactly the reordering used as
Control B in this audit), so `TerminalLocationDetector` receives the
evidence the architecture already computed for it. This is the smallest
possible fix: a single statement reorder, no signature change, no new
evidence structure, no change to `TerminalLocationDetector` or
`TerminalRecognizer` themselves. Expected consequence, precisely bounded
by this audit's own experiment: `endpoint-candidate-9da9c73ab52013a3`
would resolve to `component_terminal` (high confidence); no other
endpoint, Wire, topology, or net result would change. A test-first
regression should assert this exact endpoint transition, plus a
regression proving `model.rejected_geometry`'s serialized content
(`topology.json`) is unaffected by the reorder (since `model.rejected_geometry`
is already populated from the same source data regardless of when the
move happens relative to the `detect()` call — the only correction is to
the detector's *input*, not the model's own field).

## 23. Explicit Non-Changes

Per this AP's audit-only mandate, the following were **not** modified:

- `extraction_pipeline.cpp`'s committed behavior (the moved-from-vector
  handoff remains exactly as AP-DIAG-FIX-005 left it).
- `TerminalLocationDetector`, `TerminalRecognizer`.
- `SymbolGeometryExtractor`, `ShapeDetector`.
- Wire reconstruction, `ElectricalNetResolver`.
- The data model (`RejectedGeometryEvidence`, `WireModel`, or any other
  type).
- Ownership semantics established by AP-DIAG-FIX-005.

All temporary diagnostic instrumentation (stderr prints in
`extraction_pipeline.cpp` and `terminal_location_detector.cpp`, and the
temporary Control-B statement reordering) was fully reverted from working
files (restored from a pre-edit backup) before any committed change was
made; `git status --short` and `git diff --stat` confirmed a byte-for-byte
clean working tree relative to HEAD `754f078` before this audit's own
documentation and JSON artifact were added.

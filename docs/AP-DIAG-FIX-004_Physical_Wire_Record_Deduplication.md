# AP-DIAG-FIX-004 — Physical Wire Record Deduplication

## 1. Starting State Verification

- `git rev-parse --show-toplevel`: `/home/user/ds-extractor`
- `git branch --show-current`: `main`
- `git status --short` (before any change): empty (clean)
- `git rev-parse HEAD` (before any change): `689d72802e940f59da2a7a8af75a9e4cd8e06e34`
  — matches AP-DIAG-AUDIT-003's reported Final SHA exactly.
- No branch was created or switched at any point in this AP. The one
  `git worktree add` used for independent pre-fix comparison (§6) is a
  separate checkout directory, not a branch operation on this
  repository's own branch state; it was removed
  (`git worktree remove --force`) once its extraction was captured, and
  `git worktree list` confirms only `main` remains.

## 2. Exact Failure Reproduced From Forensic Evidence

Per AP-DIAG-AUDIT-003's `artifacts/audit/post_ground_fix_delta.json`, the
37 Wire records produced on `main` contained exactly two duplicate pairs,
both sharing the same `topology_edges` and `conductor_segments` within
each pair:

| Group key (topology edge) | Duplicate Wire IDs |
|---|---|
| `topology-edge-7b4bc0d5b22a8513` | `wire-9da1927f1f6dbf98`, `wire-fd53d84a92e53bb4` |
| `topology-edge-7dccad922e6f2958` | `wire-49477c4a4828ab6e`, `wire-7fa7c11e9eed226a` |

Reading the two members of each pair directly from a fresh unscoped
extraction (pre-fix) confirmed both AUDIT-003's finding and the specific
endpoint identities:

- Pair 1: `endpoint-candidate-9d7e7957c56e3777` <-> `endpoint-candidate-87250d4701edd231`,
  `topology_edges=["topology-edge-7b4bc0d5b22a8513"]`,
  `conductor_segments=["normalized-conductor-segment-0787ea8a78f08cba"]`,
  both members `identity_status=resolved`, endpoint order reversed between
  the two records.
- Pair 2: `endpoint-candidate-6193f59e31ae96ec` <-> `endpoint-candidate-bfae64deea64562f`,
  `topology_edges=["topology-edge-7dccad922e6f2958"]`,
  `conductor_segments=["normalized-conductor-segment-6ec8c451d0ef2961"]`,
  both members `identity_status=resolved`, endpoint order reversed between
  the two records.

Both pairs' `identity_evidence_ids` are also identical within each pair
(same two `ConductorBoundaryResolution` IDs, order-independent since they
were appended per-endpoint at the pipeline merge boundary). No topology
node, no component, no electrical net, and no AP-WIRE-031 endpoint
invariant was implicated — the duplication is a Wire-record-emission
defect only, exactly as AUDIT-003 characterized it.

## 3. Root Cause

`PhysicalWireIdentityReconstructor` (Pass 1 wrapping `WireReconstructor`,
Pass 2's `walk_forward`/`walk_from`) was traced in full. Both passes'
internal dedup mechanisms — `WireReconstructor`'s
`if (endpoint.id > other_endpoint) continue` (smaller-ID-first) plus
`consumed_edges`, and Pass 2's `seen_pairs` with `a > b` swap
normalization — are each individually sound and internally consistent.
Temporary `fprintf`-based instrumentation (added to a backed-up copy of
`physical_wire_identity_reconstructor.cpp`, reverted immediately after
confirming the finding; `git status --short` reconfirmed clean before any
production change was made) proved that **neither pass of
`PhysicalWireIdentityReconstructor` produced both members of either
duplicate pair** — only one member of each pair
(`wire-49477c4a4828ab6e` and `wire-fd53d84a92e53bb4`) came from that
reconstructor at all.

Searching the codebase for other Wire-producing call sites
(`grep -rln "wires.push_back|.wires.emplace_back" src/`) found the true
second producer: `DistributionDecomposer`
(`src/topology/distribution_decomposer.cpp`), invoked from *within*
`ElectricalNetResolver::resolve()` — an entirely separate call chain from
`PhysicalWireIdentityReconstructor`. `DistributionDecomposer` is
anchor-based (requires a uniquely-identified Ground/ExternalConnection
anchor and a tree-shaped electrically-connective path) and **always**
orders its Wire's endpoints anchor-first:

```cpp
wire.start_endpoint = anchor->id;
wire.end_endpoint = target_id;
```

regardless of the lexicographic relationship between `anchor->id` and
`target_id`. `WireReconstructor`'s convention is "smaller endpoint ID
string first," unconditionally. When both reconstructors independently
discover the same physical wire (which can legitimately happen — one via
ordinary endpoint-to-endpoint continuation walking, the other via
anchor-based distribution decomposition), they can disagree on which
endpoint is `start` and which is `end`.

The pipeline (`src/pipeline/extraction_pipeline.cpp`) already anticipated
this overlap with an explicit "AP-WIRE-004" merge/dedup mechanism:

```cpp
std::unordered_set<std::string> emitted_wire_ids;
// ... populate from model.wires (PhysicalWireIdentityReconstructor's output) ...
for (auto wire : net_artifacts.wires) {           // DistributionDecomposer's output
    if (emitted_wire_ids.insert(wire.id).second) {
        // ... emit as a new Wire ...
    }
}
```

This is exactly the "previous deduplication mechanism" the task
description refers to, and it exists specifically to prevent duplicate
ambiguity expansion across the two reconstructors. It escapes this case
because `wire.id` is computed as
`stable_id("wire", source_id + ":" + page + ":" + start + ":" + end)` —
direction-dependent. Reversing `start`/`end` changes the hash input, so
the same physical wire produces two different `Wire::id` values when
discovered by both reconstructors with opposite endpoint ordering, and the
raw-ID-keyed `std::unordered_set` cannot recognize them as the same wire.

**The duplication originates at this pipeline merge boundary, not inside
`PhysicalWireIdentityReconstructor`.** Per the task's own allowance
("Preferred location: `PhysicalWireIdentityReconstructor` or the
narrowest existing Wire emission/deduplication boundary... unless
forensic tracing proves the duplicate originates there"), the fix targets
this merge boundary instead.

## 4. Canonical Wire Identity Key

A new, pure, independently-testable function is introduced:

- `include/eke_dx_wire/topology/wire_identity_key.hpp`
- `src/topology/wire_identity_key.cpp`

```cpp
std::string canonical_wire_identity_key(const Wire& wire);
```

It normalizes exactly what the demonstrated duplicate condition requires
and nothing more:

1. `start_endpoint`/`end_endpoint` are lexicographically ordered (smaller
   first) — this is what makes reversed endpoint ordering equivalent:
   endpoint ordering is a traversal-direction artifact, not part of
   physical Wire identity (Section 4 of the task spec). Two records with
   endpoints A->B and B->A over the *same physical conductor path* are
   the same wire.
2. `topology_edges` and `conductor_segments` are each sorted before being
   folded into the key, because reversing traversal direction also
   reverses the recorded path order — without sorting, the same physical
   path traversed in opposite directions would produce different
   sequences and fail to canonicalize.
3. The key concatenates the normalized endpoint pair with the sorted
   edge/segment lists (prefixed to keep the two lists from colliding).
   `identity_status`, `identity_evidence_ids`, confidence, and
   `heavy_cable` are deliberately excluded from the key — they are Wire
   *attributes*, not part of what defines whether two records represent
   the same physical conductor path, and Section 6 of the task forbids
   changing Wire identity semantics.

Because the key includes the full (sorted) `topology_edges` and
`conductor_segments`, it does **not** collapse two Wires merely because
they share an endpoint pair via a different path, or share a conductor
segment with a different endpoint pair (AP-WIRE-029: one net != one wire,
one wire != one net). It only collapses the exact case AUDIT-003
demonstrated: same endpoint pair + same topology path + same conductor
path + reversed ordering.

## 5. Why Shared-Conductor Wires Remain Distinct

Two Wires `A: endpoint1<->endpoint2` and `B: endpoint3<->endpoint4` that
both reference conductor segment `X` produce keys
`"endpoint1|endpoint2|...|S:X"` and `"endpoint3|endpoint4|...|S:X"`
respectively — the endpoint-pair prefix differs, so the keys differ.
This is asserted directly in `tests/test_wire_identity_key.cpp` (Section
15's mandatory shared-conductor regression) and is exercised end-to-end
by the pre-existing, unmodified `tests/test_physical_wire_identity_reconstructor.cpp`
(its four-way-fork / ambiguity-expansion scenarios, AP-WIRE-FIX-003),
which continues to pass unmodified with the fix in place.

## 6. Implementation

- `src/pipeline/extraction_pipeline.cpp`: the merge-boundary
  `emitted_wire_ids` (raw `wire.id`) is replaced with
  `emitted_wire_keys` (`canonical_wire_identity_key(wire)`), for both the
  initial population from `model.wires` and the insertion check for
  `net_artifacts.wires`. No other logic in this function changed — the
  Resolved-status/evidence-ID annotation for `DistributionDecomposer`
  wires, and the final `id`-based sort, are untouched.
- No other production file was modified.
  `PhysicalWireIdentityReconstructor`, `WireReconstructor`,
  `DistributionDecomposer`, `ElectricalNetResolver`, `ShapeDetector`,
  `ConductorBoundaryResolver`, and `SourceScoper` are all byte-identical
  to HEAD `689d728`.

## 7. Tests

`tests/test_wire_identity_key.cpp` (new), written and confirmed passing
against the fix, covers:

1. AUDIT-003's exact reproduced pair 1 (reversed endpoints, identical
   path) -> keys match.
2. AUDIT-003's exact reproduced pair 2 (reversed endpoints, identical
   path) -> keys match.
3. A multi-edge/multi-segment path reversed end-to-end (traversal
   direction also reverses recorded sequence order) -> keys still match,
   proving the sort step is necessary and correct.
4. Legitimate shared-conductor Wires (different endpoint pairs, same
   conductor segment; Section 15 mandatory) -> keys differ.
5. Different endpoint pair, no shared geometry -> keys differ.
6. Same endpoint pair, genuinely different conductor path (different
   `topology_edges`/`conductor_segments`) -> keys differ — only the exact
   demonstrated duplicate condition collapses.
7. Same endpoint pair and same `topology_edges`, different
   `conductor_segments` -> keys differ.

`tests/test_physical_wire_identity_reconstructor.cpp` and
`tests/test_distribution_decomposer.cpp` are unmodified and continue to
pass, preserving AP-WIRE-FIX-003's four-way-fork/ambiguity-expansion
coverage.

Full suite: **56/56 passing** (55 pre-existing + 1 new), on a from-scratch
clean Release build, with the assertion-enabled test configuration
(`-DNDEBUG -UNDEBUG` for test targets) intact and
`dx-wire-test-assertions-enabled` passing.

Compiler warnings: exactly the same 8 pre-existing warnings, verified via
a fully clean rebuild (`rm -rf` build directory, reconfigure, rebuild) —
zero new warnings, zero errors.

## 8. Before/After Metrics (TRX300)

Both a fresh unscoped extraction (`samples/trx300ODG.png`) and a fresh
scoped extraction (`--scope fixtures/trx300/scope_production.json`) were
run against the fix, and independently compared against a from-scratch
rebuild of pre-fix HEAD `689d728` in an isolated `git worktree` (removed
afterward; `git worktree list` confirms only `main` remains).

| Metric | Baseline (689d728) | Fixed | Scope-independent? |
|---|---:|---:|---|
| Wire records (unscoped) | 37 | 35 | — |
| Wire records (scoped) | 37 | 35 | Yes — same delta both scopes |
| Distinct physical conductors (unscoped) | 35 | 35 | — |
| Distinct physical conductors (scoped) | 35 | 35 | Yes |
| Duplicate Wire-record groups (unscoped) | 2 | 0 | — |
| Duplicate Wire-record groups (scoped) | 2 | 0 | Yes |
| Topology nodes (unscoped) | 686 | 686 | unchanged |
| Topology edges (unscoped) | 876 | 876 | unchanged |
| Components (unscoped) | 81 | 81 | unchanged |
| Electrical nets (unscoped) | 10 | 10 | unchanged |
| Validation errors | 0 | 0 | unchanged |
| Validation warnings | 32 | 32 | unchanged (same codes) |

Exact removed Wire IDs (both scopes): `wire-9da1927f1f6dbf98`,
`wire-7fa7c11e9eed226a`. No Wire ID was added; no surviving Wire's fields
were modified (`wire-49477c4a4828ab6e` and `wire-fd53d84a92e53bb4`, the
two retained members of each duplicate pair, are byte-identical to their
pre-fix records).

## 9. Whole-Diagram Regression

Direct field-by-field comparison of `artifacts/topology/topology.json`
between the pre-fix baseline (isolated `git worktree` build of `689d728`)
and the fixed extraction, both unscoped and scoped:

`component_candidates`, `component_symbol_recognitions`, `nodes`, `edges`,
`electrical_nets`, `endpoint_candidates`, `conductor_boundary_resolutions`,
and `rejected_geometry` are **byte-identical** (Python `==` equality on
the parsed JSON) between baseline and fixed, in both scopes. The only
fields that differ are:

- `wires`: shrinks from 37 to 35 entries; the 35 remaining entries are
  otherwise byte-identical to their pre-fix counterparts.
- `wire_semantics`: shrinks from 37 to 35 entries, exactly the two
  removed Wire IDs (`wire-9da1927f1f6dbf98`, `wire-7fa7c11e9eed226a`)
  disappearing; every remaining entry (by `wire_id`) is byte-identical to
  its pre-fix counterpart — this is the expected, fully-explained
  downstream consequence of removing two Wire records, not a new change
  in wire-semantic resolution logic.

This directly confirms:

- **ChassisGround**: all 6 `chassis_ground`-kind `component_symbol_recognitions`
  entries unchanged (identical objects, not just identical count).
- **False positives**: `component_candidates`/`component_symbol_recognitions`
  identical, so all 8 previously-confirmed-absent false positives remain
  absent (nothing was added back).
- **Ground-role nets**: `electrical_nets` byte-identical; all 4
  `ground`-role and 6 `unknown`-role nets unchanged.
- **Topology / net membership**: `nodes`/`edges`/`electrical_nets`
  byte-identical.
- **Terminal-attribution oddity** (`wire-ded6cca62fbe3cd9`): explicitly
  checked — this Wire record and its `wire_semantics` entry are
  byte-identical pre-fix vs. post-fix. Unaffected, as expected (not one
  of the two duplicate pairs), and not touched by this AP.

No unexplained delta exists anywhere in the whole-diagram inventory.

## 10. Determinism

The fixed `dx-extract` binary was run twice against the same unscoped
input with no other change:

```
diff -rq /tmp/fix004_det1 /tmp/fix004_det2   # empty output — fully identical trees
```

`sha256sum` of `artifacts/audit/extraction_audit.json` and
`output/wires.svg` matched exactly between both runs. Wire identities,
ordering, topology paths, conductor paths, and evidence IDs are all
reproduced byte-for-byte.

## 11. Shared-Conductor / Ambiguity Regression (Sections 15-16)

- `tests/test_wire_identity_key.cpp` directly asserts that two distinct
  endpoint-pair Wires sharing a conductor segment produce different
  canonical keys (Section 15, mandatory).
- `tests/test_physical_wire_identity_reconstructor.cpp` (unmodified)
  continues to pass, including its AP-WIRE-FIX-003 four-way-fork
  ambiguity-expansion scenarios (Section 16) — this reconstructor's own
  logic was not touched by this fix, and the pipeline-level canonical key
  only affects the separate merge boundary with `DistributionDecomposer`'s
  output.

## 12. Scope Verified as Not a Factor (Section 14)

Baseline and fixed wire counts, duplicate-group counts, and distinct
conductor counts were confirmed identical in both unscoped and scoped
(`fixtures/trx300/scope_production.json`) extractions (see §8 table) —
the fix is not scope-dependent.

## 13. Explicit Non-Goals (Unchanged By This AP)

- The MEDIUM ground-endpoint coverage gap (2 of 6 ChassisGround
  components with no traced ground endpoint, per AUDIT-003 §4) is
  **not** addressed here.
- The LOW terminal-attribution oddity (`wire-ded6cca62fbe3cd9`) is
  **not** addressed here — confirmed unchanged in §9 above.
- No unrelated cleanup was performed.

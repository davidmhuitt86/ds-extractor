# AP-WIRE-025 — After-Action Report

Wire Semantic Completion

Status: **complete and validated**, with two explicit, evidence-grounded
scope decisions documented rather than silently made (see §7/§9).

## 1. Commits

Baseline (AP-WIRE-024 validated) commit: `6e34a91`

Final commit at validation: see `git log` at push time — this AAR was
committed alongside the last implementation commit; the exact final SHA
is the one immediately following this file's commit in `git log --oneline`.

Implementation commits, in order:
1. `1d7128e` — model + `WireSemanticResolver` implementation + 20-case regression test
2. `51755bc` — export to `topology.json`/`extraction_audit.json`/`review_manifest.json`
3. `b480186` — regenerated `artifacts/extraction_review`
4. this AAR + design doc + status-doc sync commit

## 2. OpenCV

5.1.0, built from source at `/usr/local`
(`OpenCV_DIR=/usr/local/lib/cmake/opencv5`) — this environment's package
manager only offers 4.6, and the codebase requires OpenCV 5 (uses
`opencv2/geometry/2d.hpp`). Same situation as every prior AP's validation
in this environment; your Windows machine resolves this via its own
OpenCV 5.0.0 install.

## 3. Release build

Clean configure + build, 0 errors.

## 4. Complete Release CTest

**46/46 passed, 0 failed** (45 from the AP-WIRE-024 baseline +
`dx-wire-test-wire-semantic-resolver`). Total test time 0.23-0.26s across
repeated runs.

## 5. Fresh TRX300 extraction — structural invariance

| Metric | AP-WIRE-024 baseline | AP-WIRE-025 | Δ |
|---|---:|---:|---:|
| Conductor segments | 294 | 294 | 0 |
| Topology nodes | 692 | 692 | 0 |
| Topology edges | 877 | 877 | 0 |
| Endpoint candidates | 210 | 210 | 0 |
| Component candidates | 109 | 109 | 0 |
| Wires | 40 | 40 | 0 |
| Electrical nets | 10 | 10 | 0 |
| Validation errors | 0 | 0 | 0 |
| Validation warnings (histogram) | 30 (24 `WIRE-GEOMETRIC-ENDPOINTS`, 6 `NET-ROLE-UNRESOLVED`) | 30 (identical) | 0 |
| Coverage: real components with terminal evidence | 20 | 20 | 0 |
| Coverage: connectors total/terminals | 0/0 | 0/0 | 0 |
| Coverage: endpoints not in any net | 177 | 177 | 0 |

Every §16-required invariant held exactly. `WireSemanticResolver` adds
one new vector (`model.wire_semantics`) and touches nothing else -
verified both structurally (its signature takes everything by `const&`
and returns a new value) and empirically (this table).

## 6. Semantic resolution metrics (this AP's actual output)

Deterministic geometric extraction only (no `--recognition`/
`--vision-recognition` — this validation used the plain extraction path,
consistent with every prior AP's baseline runs):

| Category | Resolved | Conflicted | Unresolved | Total |
|---|---:|---:|---:|---:|
| Wire color | 0 | 0 | 40 | 40 |
| Function | 0 | 0 | 40 | 40 |
| Component association (either end) | 16 | 0* | 24 | 40 |
| Connector association (either end) | 0 | — | 40 | 40 |
| Electrical-net association | 5 | 0 | 35 | 40 |
| **Fully unresolved wires** (zero Resolved fields) | — | — | — | **24** |

\* See §6a — the aggregate "0 conflicted" for component association needs
a caveat; it is not the full picture.

**Wire color and function are 0/40 resolved.** This is correct, not a
gap: `EndpointCandidate.wire_color`/`function_label` are populated only
from recognized-text evidence (`TextRecognitionEvidence` →
`EngineeringObjectSemanticResolver` → `EngineeringObjectSemanticApplier`),
and this validation ran the deterministic path with no text-recognition
provider active (`NullTextRecognitionProvider`), matching every prior
AP's baseline. Zero text evidence in → zero wire-color/function
resolution out. This is the AP's own stated priority in action:
insufficient evidence → Unresolved, never a guess. A run with
`--vision-recognition` (validated end-to-end for the recognition
provider itself in an earlier session) would be expected to populate
some of these; validating that combination is out of this AP's scope
(§2 explicitly forbids requiring an API key for the normal extraction
path, and this AAR's baseline must match the other APs' baseline
methodology to be comparable).

**Connector association: 0/40.** Also correct, not a gap: TRX300 has 0
`ConnectorCandidate`/`ConnectorTerminal` objects at all (per the
AP-WIRE-022A AAR, the furniture-classifier fix removed the population
that used to satisfy connector heuristics). There is nothing to
associate. This is architecture working correctly against evidence that
does not exist on this fixture, not a defect in AP-WIRE-025.

### 6a. Component association: the aggregate hides a real per-side case

The audit-level `component_association` counter treats a wire as
"resolved" if **either** end is resolved, so it reports 0 conflicted even
though `docs/AP-WIRE-024_AAR.md` documented 4 endpoints with genuinely
conflicting component evidence. I checked directly (not inferred)
whether any of those 4 endpoints is actually a wire start/end: **exactly
one is** - `endpoint-candidate-b0e3d6bb622a227c` is the `end_endpoint` of
`wire-430f59213afaef3a`. Its full `wire_semantics` record:

```
start_component_id: "component-candidate-shape-region-845947b0afd7451a"
start_component_status: "resolved"
end_component_id: ""
end_component_status: "conflicted"
```

This is the correct outcome: the resolved start is reported as such, the
conflicted end is reported as such and its `component_id` is empty (not
fabricated), and the wire is counted in the "resolved" bucket at the
aggregate level *because its start side has independent evidence* — not
because the conflict was hidden. The full per-side detail survives in
`topology.json`'s `wire_semantics` array; only the coarse audit-level
aggregate collapses per-side detail into a single per-wire bucket. This
is a legible design tradeoff (documented here and in the design doc), not
an accidental loss of information — anyone needing the per-side
distinction reads `topology.json` directly rather than the audit summary.

## 7. AP-WIRE-024 conflict constraint — directly verified

§5 of the task required: "A conflicted endpoint/component/terminal
relationship MUST NOT be treated as authoritative wire semantic
evidence." Verified two ways:

1. **Structurally**: `component_status_for()` in `wire_semantic_resolver.cpp`
   maps `EndpointSemanticReconstructionStatus::Conflicted` directly to
   `WireSemanticStatus::Conflicted` and never reads `component_id` in
   that branch.
2. **Empirically**: §6a's `wire-430f59213afaef3a` is the one real case on
   this fixture, and it resolved exactly as designed.

## 8. Semantic failure clusters

None found. The only near-miss risk identified during implementation
(reading `EndpointCandidate.wire_color`/`function_label` without
realizing `EngineeringObjectSemanticApplier` silently drops later
conflicting values at the endpoint level) was designed around rather than
inherited: this AP independently compares the wire's *two* endpoints
against each other for wire-color/function conflict, rather than trusting
per-endpoint non-conflict as proof there was none (see design doc
"Implementation-scope assessment"). No test in
`tests/test_wire_semantic_resolver.cpp` exposed unexpected behavior.

## 9. Known limitations

1. **No wire color/function resolution on this baseline** (§6) - expected
   given no text-recognition evidence in the deterministic path, not a
   defect. Revisit once a `--vision-recognition` TRX300 run is validated
   against this same coverage metric.
2. **No connector association on this fixture** (§6) - there are no
   connectors to associate; the field is real and will activate on any
   fixture where `ConnectorCandidate`/`ConnectorTerminal` are populated.
3. **Source/destination distribution role intentionally not implemented**
   (design doc §"Deliberately omitted") - `CircuitRoleEvidence` is not
   currently retained on `WireModel`, and the only persisted role signal
   (`ElectricalNet.role`) is net-level, which §8 of the task explicitly
   warned against copying onto wires without justification. No
   defensible producer currently exists for a wire-scoped role field.
4. **Component-association aggregate collapses per-side conflict detail**
   (§6a) - a legible tradeoff, not a bug, but worth knowing before reading
   only the audit summary.

## 10. Recommended follow-on work

1. If/when AP-WIRE-026+ needs wire-scoped distribution role, first expose
   `CircuitRoleEvidence` on `WireModel` (small, independent change) rather
   than deriving role from `ElectricalNet.role` as a shortcut.
2. Validate this AP's wire-color/function resolution against a
   `--vision-recognition` TRX300 run as a follow-up measurement (not a
   blocking gap - the deterministic-path baseline is the correct
   comparison point for this AAR, matching every prior AP).
3. Consider exposing the per-side `start_component_status`/
   `end_component_status` split directly in `extraction_audit.json`
   (currently only in `topology.json`'s `wire_semantics` array) if
   AP-WIRE-026 needs it in the coarser audit view without a second read.

## Verdict

AP-WIRE-025 is **validated**. It adds real, evidence-grounded semantic
fields to 16/40 wires (component association) and correctly reports 0/40
for wire-color, function, and connector association given the absence of
supporting evidence on this fixture and baseline configuration - matching
the AP's own stated priority (§18): explicit evidence → resolution,
conflicting evidence → conflict (verified against the one real
AP-WIRE-024 conflict case that touches a wire endpoint), insufficient
evidence → Unresolved, never a guess. All required structural invariants
(§16) held exactly. AP-WIRE-026 was not started.

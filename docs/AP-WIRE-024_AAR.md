# AP-WIRE-024 — After-Action Report (Validation)

Terminal Recognition & Component-Terminal Association

Status: **validated, with a documented tuning recommendation** (see
§Verdict). Implementation was already on `main` when this validation
began (commits `40129f0`..`797b222`, "AP-WIRE-024: ..."); this report is
the required independent validation pass, not the implementation.

## 0. Git state

HEAD at validation: `071af714f44dbd466976f6216f0bb02dc13e8991`

Prior state had a real defect: `artifacts/extraction_review/review_manifest.json`
contained unresolved Git merge-conflict markers
(`<<<<<<< HEAD` / `=======` / `>>>>>>> 797b22231d142be62d6135dbc8a24a2e19048b2f`)
checked in from an unresolved merge. Fixed by regenerating the artifact
from current main rather than hand-editing the conflict (commit
`071af71`). Verified: `python3 -c "import json; json.load(open(...))"`
succeeds, and `grep -c` for conflict markers across the working tree
returns 0. Working tree is clean; `git status` reports nothing to
commit.

## 1. OpenCV

5.1.0, built from source in this environment at `/usr/local`
(`OpenCV_DIR=/usr/local/lib/cmake/opencv5`). The project requires
OpenCV 5 (it uses `opencv2/geometry/2d.hpp`, a real OpenCV-5-only
header); the platform package manager here only offers 4.6, so 5.1.0 was
built locally to run this validation. This mirrors what your Windows
machine already resolved via its own OpenCV 5.0.0 install.

## 2. Release build

Clean configure + build, 0 errors:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DOpenCV_DIR=/usr/local/lib/cmake/opencv5
cmake --build build -j
```

## 3. Complete Release CTest

**45/45 passed, 0 failed.** Full list (all `Passed`):
`dx-wire-test-geometry`, `dx-wire-test-ids`,
`dx-wire-test-topology-semantic-resolver`, `dx-wire-test-topology`,
`dx-wire-test-conductor-normalizer`, `dx-wire-test-conductor-evidence-evaluator`,
`dx-wire-test-rejected-geometry-classifier`, `dx-wire-test-geometry-ownership-classifier`,
`dx-wire-test-endpoint-reconstructor`, `dx-wire-test-wire-reconstructor`,
`dx-wire-test-distribution-decomposer`, `dx-wire-test-endpoint-semantic-reconstructor`,
`dx-wire-test-terminal-semantic-resolver`, `dx-wire-test-terminal-semantic-evidence-builder`,
`dx-wire-test-electrical-net-resolver`, `dx-wire-test-circuit-role-resolver`,
`dx-wire-test-circuit-role-evidence-builder`, `dx-wire-test-semantic-evidence-associator`,
`dx-wire-test-engineering-object-semantic-applier`, `dx-wire-test-component-identity-evidence-builder`,
`dx-wire-test-component-identity-resolver`, `dx-wire-test-component-identity-registry`,
`dx-wire-test-semantic-observation-resolver`, `dx-wire-test-engineering-object-semantic-resolver`,
`dx-wire-test-text-evidence-interpreter`, `dx-wire-test-json-text-recognition-provider`,
`dx-wire-test-text-recognition-provider`, `dx-wire-test-sidecar-text-recognition-provider`,
`dx-wire-test-recognition-observation-parser`, `dx-wire-test-anthropic-vision-text-recognition-provider`,
`dx-wire-test-base64`, `dx-wire-test-terminal-location-detector`,
**`dx-wire-test-terminal-recognizer`**, `dx-wire-test-connector-terminal-model`,
`dx-wire-test-component-symbol-recognizer`, `dx-wire-test-component-candidate-classifier`,
`dx-wire-test-diagram-furniture-classifier`, `dx-wire-test-gap-interpreter`,
`dx-wire-test-shape-detector`, `dx-wire-test-text-region-detector`,
`dx-wire-test-wire-model-validator`, `dx-wire-test-recognition-input-exporter`,
`dx-wire-test-extraction-audit`, `dx-wire-test-coverage-diagnostics`,
`dx-wire-test-symbol-geometry-extractor`.

Total test time 0.26s.

## 4. Fresh TRX300 extraction

`dx-extract extract samples/trx300ODG.png --output .` (current main,
HEAD `071af71`), then coverage diagnostics from
`artifacts/audit/extraction_audit.json`.

## 5. Required comparison (AP-WIRE-023 baseline → AP-WIRE-024)

| Metric | AP-WIRE-023 baseline | AP-WIRE-024 | Δ |
|---|---:|---:|---:|
| Real components with terminal evidence | 18 | 20 | +2 |
| Real components without terminal evidence | 41 | 39 | -2 |
| TerminalCandidate count | 48 | 56 | +8 |
| TerminalLead-derived associations | n/a (0) | 1 | +1 |
| Boundary/alignment-derived associations | n/a (0) | 7 | +7 |
| Ambiguous/rejected associations (surfaced, not discarded) | 0 | 4 endpoints, 9 candidates involved | +4 |
| Endpoint zero-wire count | 132 | 132 | 0 |
| Conductor segments owned by wires (normal+shared) | 50 | 50 | 0 |
| Topology edges owned by wires | 50 | 50 | 0 |
| Connector candidates | 0 | 0 | 0 |
| Connector terminals | 0 | 0 | 0 |
| Electrical-net endpoint membership (in a net) | 33 | 33 | 0 |
| Validation warnings (histogram) | 30 (24 `WIRE-GEOMETRIC-ENDPOINTS`, 6 `NET-ROLE-UNRESOLVED`) | 30 (identical) | 0 |
| Validation errors | 0 | 0 | 0 |
| Wires / topology nodes / edges / electrical nets | 40 / 692 / 877 / 10 | 40 / 692 / 877 / 10 | 0 |

Wire, topology, and electrical-net identity are **byte-for-byte
unchanged** — confirms requirement F (no endpoint, wire, topology, or
electrical-net identity invented). `TerminalCandidate` is the only object
population this AP adds to.

### TerminalLead vs boundary/alignment breakdown (worked out by hand)

`TerminalCandidate.id` is a content hash (`stable_id`), so the two
recognition paths are not distinguishable by string prefix alone; I
disambiguated by `distance_to_component` against each path's configured
maximum (lead: 6px; boundary/alignment: 16px). Of the 8 new candidates,
7 have `distance_to_component` in the 9–14.0089px range — impossible for
the lead path (max 6px) — so they are boundary/alignment associations.
Only 1 (`distance_to_component = 0`, component
`component-candidate-shape-region-c98f6566b0672535`, which AP-WIRE-023's
AAR identified as the one `circular_symbol` with 2 `TerminalLead`
primitives) is a genuine lead-based match. **The boundary/alignment
fallback, not TerminalLead evidence, produced 7 of this AP's 8 new
candidates on this fixture** — worth knowing before treating AP-WIRE-023's
internal-geometry work as the primary driver of this AP's yield.

## 6. Coverage diagnostic comparison

| Coverage metric | Baseline | AP-WIRE-024 | Δ |
|---|---:|---:|---:|
| `components.real_with_terminal_evidence` | 18 | 20 | +2 |
| `components.real_without_terminal_evidence` | 41 | 39 | -2 |
| `endpoints.zero_wire` | 132 | 132 | 0 |
| `conductor_segments.{normal,shared}` | 49/1 | 49/1 | 0 |
| `topology_edges.unowned_by_any_wire` | 827 | 827 | 0 |
| `connectors.total/terminals_total` | 0/0 | 0/0 | 0 |
| `electrical_nets.endpoints_not_in_any_net` | 177 | 177 | 0 |
| `findings.total` | 1424 | 1422 | -2 (matches the 2 fewer `COMPONENT-NO-TERMINAL-EVIDENCE` findings) |

## 7. Terminal recognition breakdown and false-positive/conflict inspection

### 7a. The 4 multi-component endpoints (directly checked, not inferred)

Four endpoints ended up with `TerminalCandidate` evidence pointing at
more than one distinct component after this AP ran:

| Endpoint | Components claimed | Source of the 2nd/3rd claim |
|---|---|---|
| `endpoint-candidate-1fd584e37a5c72b5` | `...432be0a200811b76`, `...4f1e5de1ef1830bd` | boundary/alignment, dist 14 |
| `endpoint-candidate-c033140e251b7b86` | `...432be0a200811b76`, `...4f1e5de1ef1830bd` | boundary/alignment, dist 14.0089 |
| `endpoint-candidate-b0e3d6bb622a227c` | `...845947b0afd7451a`, `...c98f6566b0672535` | lead-based, dist 0 |
| `endpoint-candidate-cde07f8718a2b9cd` | `...320c9114d1207736`, `...64afaa2a14134a2a`, `...6a7001c24fe4c252` | boundary/alignment, dist 9 and 10 |

I checked `endpoint_semantic_reconstructions` for all four directly (not
assumed): every one is `status: "conflicted"`, `endpoint_kind:
"unresolved"`, `component_id: ""`, `evidence_component_ids` listing all
claimed components. **This is correct behavior per AP-WIRE-019's existing
ambiguity policy, which AP-WIRE-024's own design doc explicitly names as
the intended outcome for this case** ("If separate evidence associates
one endpoint with multiple components, the candidates remain explicit
and AP-WIRE-019's existing conflict handling determines the endpoint
semantic result without selecting an arbitrary component."). No false
identity was assigned. This satisfies requirement D.

### 7b. The real cost: 4 endpoints regressed from resolved to conflicted

Diffing `endpoint_candidates[].kind` between the AP-WIRE-023 baseline and
this run (not inferred from aggregate counts) shows exactly 7 endpoints
changed kind:

| Endpoint | Before | After |
|---|---|---|
| `...7b1232ec85583772` | `geometric` | `component_terminal` (genuine new resolution) |
| `...8a7b6fb67ed4eeab` | `geometric` | `component_terminal` (genuine new resolution) |
| `...c68b526cba9cc6d3` | `geometric` | `component_terminal` (genuine new resolution) |
| `...1fd584e37a5c72b5` | `ground` | `geometric` (demoted — see 7a) |
| `...c033140e251b7b86` | `ground` | `geometric` (demoted — see 7a) |
| `...cde07f8718a2b9cd` | `ground` | `geometric` (demoted — see 7a) |
| `...b0e3d6bb622a227c` | `component_terminal` | `geometric` (demoted — see 7a) |

So the net "+2 real components with terminal evidence" in §5/§6 is not
the full picture at the endpoint level: **3 previously-confident `ground`
endpoints and 1 previously-confident `component_terminal` endpoint were
demoted to unresolved** because the boundary/alignment fallback
introduced a second, competing, lower-confidence claim on the same
endpoint from a different nearby component (all at 9–14px, near the
16px ceiling). This is the honest cost of this AP's boundary/alignment
heuristic on this fixture, and it is exactly the kind of thing the
validation task asked me to surface rather than let a net-positive
component count hide.

Whether the *original* ground/component_terminal classification or the
*new* competing claim is actually correct cannot be determined from the
model alone without inspecting the source image at those four locations
directly — which is the right outcome: the system is honestly reporting
"I don't know" rather than guessing, which is the AP's own stated
priority (§27 of the AP-WIRE-023/024 specs: "preserved uncertainty" over
"zero unresolved objects").

## 8. Risk-by-risk inspection (as specifically requested)

**A. `PrimitiveSymbol` → `ConnectorBoundary` mapping may be over-broad.**
Confirmed as a **latent design concern, not an active bug on this
fixture**: `shape_kinds.primitive` is 0 in both the baseline and this
run, so `terminal_kind()`'s `PrimitiveSymbol → ConnectorBoundary` branch
never executes against TRX300. The mapping itself is still an
unconditional, unqualified assumption ("every primitive-shaped component
is a connector boundary") with no supporting evidence check, unlike every
other path in this recognizer. **Recommendation:** either require
corroborating evidence (e.g. an adjacent `TerminalLead`/boundary
alignment, the same standard applied elsewhere in this file) before
mapping to `ConnectorBoundary`, or leave `PrimitiveSymbol` as `Unknown`
until a fixture that actually exercises it is available. Flag for
AP-WIRE-025 or a small follow-up; not blocking, since it is provably
inert here.

**B. Boundary/alignment fallback may create false component associations.**
**Confirmed as the dominant behavior on this fixture** (7 of 8 new
candidates) and **confirmed as the source of all 4 conflicts** in §7.
The fallback is not creating outright false *identity* (conflicts are
correctly surfaced, never silently resolved — satisfies D), but it is
demonstrably prone to producing a second, lower-confidence, longer-
distance (9–14px) claim on endpoints that already have a confident
resolution, which downgrades them to unresolved. This is the primary
finding of this validation. **Recommendation:** consider excluding a
candidate endpoint from the boundary/alignment path when it already has
*any* existing `TerminalCandidate` from `TerminalLocationDetector` (not
just an exact duplicate pair, which is all `existing_pairs` currently
checks) — i.e. don't contest an endpoint that's already spoken for by
independent evidence, only fill genuinely unclaimed endpoints.

**C. TerminalLead matching must not associate a lead with an unrelated
nearby endpoint.** On this fixture, only 1 of 9 AP-WIRE-023 `TerminalLead`
primitives produced an association at all (the rest found no endpoint
within 6px, or a tie was correctly rejected — the nearest-unique-match
logic is doing its job by producing *fewer* associations, not more, when
evidence is ambiguous). The one association that fired
(`b0e3d6bb622a227c` → `c98f6566b0672535`, dist 0) is well-supported: an
exact-position match to the specific component whose own internal
geometry produced the lead. No evidence of a lead being matched to an
unrelated endpoint.

**D. Existing associations must not hide contradictory evidence.**
Directly verified in §7a: all 4 conflicting cases are `status:
"conflicted"` with `component_id: ""`, not silently resolved to one
candidate. Satisfied.

**E. DiagramFurniture must remain excluded.** Verified: `diagram_furniture`
count unchanged at 50 both before and after;
`ComponentCandidateKind::DiagramFurniture` is filtered at the top of
`TerminalRecognizer::recognize`'s component loop before any evidence
path runs. Satisfied.

**F. No endpoint, wire, topology, or electrical-net identity invented.**
Verified in §5: `endpoint_candidates` count (210), `wires` (40),
`topology_nodes`/`topology_edges` (692/877), and `electrical_nets` (10)
are identical before and after, including the SVG/topology export byte
counts. `TerminalRecognizer`'s return type (`TerminalRecognitionArtifacts`
→ `std::vector<TerminalCandidate>` only) makes this structurally
enforced, not just empirically observed. Satisfied.

## 9. What AP-WIRE-024 should NOT be credited for

Per the validation task's explicit instruction: the +8 `TerminalCandidate`
count alone is not evidence of success. The defensible claim is narrower:
+3 endpoints gained a well-supported, unambiguous new classification;
+1 endpoint gained a well-supported lead-based classification but is
contested by a pre-existing weaker claim (net: conflicted, correctly);
3 more endpoints lost a previously-confident classification to a new,
geometrically weaker (9–14px, near-ceiling) competing claim. Net "+2
components with terminal evidence" is real but overstates the picture
without §7b's endpoint-level accounting.

## Verdict

**AP-WIRE-024 is validated as implemented and safe**: it never invents
identity, never touches wire/topology/net structures, and every conflict
it introduces is correctly surfaced rather than silently resolved
(satisfies its own design doc's ambiguity policy and all of risks D/E/F).
It is **not validated as unambiguously beneficial on this fixture**: the
boundary/alignment fallback's 9–14px associations are exactly as likely
to demote a correct prior classification to unresolved as to produce a
new correct one (3 demotions vs. 3 clean gains, plus 1 contested gain).

**Recommendation before AP-WIRE-025:** tighten the boundary/alignment
fallback per §8.B (skip endpoints that already carry independent
evidence) and reconsider §8.A's `PrimitiveSymbol → ConnectorBoundary`
mapping, since both are cheap, targeted changes to the same file and
avoid carrying an under-supported association pattern into terminal/wire
semantic work. Neither is a blocking defect - AP-WIRE-024's own conflict
handling already prevents them from corrupting the model - but leaving
them unaddressed means AP-WIRE-025 would inherit a small, known source of
avoidable ambiguity rather than a clean, minimal delta.

AP-WIRE-025 was **not started** as part of this validation, per
instruction.

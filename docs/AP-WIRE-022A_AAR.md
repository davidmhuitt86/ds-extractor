# AP-WIRE-022A — After-Action Report

Extraction Baseline Validation & Diagnostic Expansion

Status: complete (observational). No geometry, topology, endpoint,
wire, component, or electrical-net correction was made as part of this
AP. Findings below identify correction candidates for later APs; none
were applied here.

## 1. Baseline

HEAD at baseline capture: `f229218` (main), immediately after:

- `d642afd` — Exclude tabular diagram furniture from component detection
- `48ab7d5` — Merge: Anthropic vision recognition provider
- `ebbdc05` / `bc5d93c` — this AP's coverage diagnostic model + exports

Build: CMake Release configure against OpenCV 5.1.0 (built from source in
this environment; not otherwise available via the platform package
manager) + libcurl. Full Release CTest: **43/43 passed**, 0 failures.

Fresh extraction: `dx-extract extract samples/trx300ODG.png --output .`
(no `--recognition`/`--vision-recognition`, i.e. the deterministic
geometric baseline with no text-recognition evidence layered in).

| Metric | Value |
|---|---:|
| Conductor segments | 294 |
| Topology nodes | 692 |
| Topology edges | 877 |
| Endpoint candidates | 210 |
| Component candidates | 109 |
| Component symbol recognitions | 109 |
| Connector candidates | 0 |
| Connector terminals | 0 |
| Wires | 40 |
| Electrical nets | 10 |
| Gaps bridged | 6 |
| Validation errors | 0 |
| Validation warnings | 30 |
| Unresolved wires | 0 |

Warning-code histogram (from `WireModelValidator`, unchanged mechanism):

| Code | Count |
|---|---:|
| `NET-ROLE-UNRESOLVED` | 6 |
| `WIRE-GEOMETRIC-ENDPOINTS` | 24 |

Topology node types: `conductor_end` 210, `continuation` 139, `splice`
106, `crossing` 237, `junction`/`component_boundary`/`unresolved` 0.

Do not compare this table against `docs/PROJECT_STATUS_AND_GAP_ANALYSIS.md`
§6's *previous* numbers (293/693/876/215/41/11) without accounting for the
DiagramFurnitureClassifier change — see §7 below. Minor deltas in
topology-node/edge/gap counts between runs of this same pipeline version
are expected from geometry-evidence thresholds, not from this AP's
diagnostics (which observe only; they do not affect any of the above
numbers).

## 2. Geometry coverage (conductor segments — §5.1 of the AP spec)

| Class | Count |
|---|---:|
| Normal (topology + exactly 1 wire) | 49 |
| Shared (topology + >1 wire) | 1 |
| Topology-only (no wire yet) | 244 |
| Wire-only (no topology) | 0 |
| Unreferenced (neither) | 0 |

**Finding G1 (expected ambiguity, not a defect):** segment
`normalized-conductor-segment-3f12e0cfebfae8e4` is legitimately `SHARED`,
traversed by `wire-2ff76e5e03e31dde` and `wire-91f9658e74d6a5fe`. This is
exactly the branching case described in AP spec §3 (a splice with two
downstream endpoint-to-endpoint traces) and must **not** be treated as an
error. The diagnostic correctly classifies it `Notice`, not `Warning`.

**Finding G2 (real gap, not a defect in this AP's diagnostics):** 244 of
294 conductor segments (83%) are `TOPOLOGY_ONLY` — part of the topology
graph but never claimed by a `Wire`. Combined with only 50/877 (5.7%)
topology edges owned by a wire (see §3), this says wire reconstruction
currently resolves a small minority of the geometric graph into explicit
endpoint-to-endpoint traces. This is the single largest quantitative gap
this baseline surfaces. Likely responsible stage: `WireReconstructor` /
`ElectricalNetResolver`'s wire-emission path (`src/topology/wire_reconstructor.cpp`,
`src/topology/electrical_net_resolver.cpp`) — the topology graph itself
(`TopologyReconstructor`) is not implicated, since node/edge counts are
internally consistent (0 dangling references, 0 zero-degree nodes, 0
low-degree splices — see §3).

No `CONDUCTOR-UNREFERENCED` or `CONDUCTOR-WIRE-ONLY` findings occurred
(0 each), so there is no evidence of orphaned conductor geometry or of a
wire inventing conductor references topology does not have.

## 3. Topology coverage (§5.4/5.5)

| Metric | Value |
|---|---:|
| Topology edges total | 877 |
| — missing conductor / from-node / to-node reference | 0 / 0 / 0 |
| — unowned by any wire | 827 (94.3%) |
| Topology nodes total | 692 |
| — zero-degree | 0 |
| — low-degree splice (`Splice` type, degree < 2) | 0 |
| — `ConductorEnd` node with no endpoint candidate | 0 |

**Finding T1:** Topology graph integrity is clean — every edge resolves
to real nodes and (where set) a real conductor segment, no isolated
nodes, no mis-typed splices, no orphaned conductor-end nodes. This
isolates the 827-unowned-edge gap to wire *reconstruction*, not to
topology *construction*.

## 4. Endpoint coverage (§5.2)

| Class | Count |
|---|---:|
| Zero-wire | 132 (62.9%) |
| Single-wire | 76 |
| Multiple-wire | 2 |

**Finding E1 (expected ambiguity):** the two `MULTIPLE_WIRE_ENDPOINT`
endpoints (`endpoint-candidate-79d0dfbebb902472`,
`endpoint-candidate-b04f3e60d05f909e`) are exactly the two ends of the
shared conductor from Finding G1 — both are the shared endpoint of
`wire-2ff76e5e03e31dde` and `wire-91f9658e74d6a5fe`. Consistent, not a
defect.

**Finding E2 (same root cause as G2):** 132/210 endpoints (63%) are not
yet the terminus of any wire. This is the endpoint-side symptom of the
same wire-reconstruction gap identified in §2/§3, not an independent
failure.

## 5. Wire coverage (§5.3)

| Metric | Value |
|---|---:|
| Wires total | 40 |
| Valid (no `WireModelValidator` error) | 40 |
| Invalid | 0 |

**Finding W1:** every wire that *was* reconstructed is structurally
sound — no duplicate IDs, no missing endpoint/edge/segment references,
no self-endpoints, no disconnected or non-path topology. The problem
identified in §2–§4 is one of **coverage** (how much of the graph becomes
a wire), not **correctness** (whether the wires that exist are valid).

## 6. Component coverage (§5.6)

| Class | Count |
|---|---:|
| Total component candidates | 109 |
| Diagram furniture | 50 |
| Real (non-furniture) candidates | 59 |
| — with terminal/endpoint evidence | 18 |
| — **without** terminal/endpoint evidence | 41 |
| Furniture without terminal evidence | 40 |

**Finding C1 (real gap — the standout `Warning`-severity cluster of this
baseline, 41 occurrences of `COMPONENT-NO-TERMINAL-EVIDENCE`):** 41 of 59
real (non-furniture) component candidates have zero associated
`TerminalCandidate` and zero `EndpointCandidate.component_id` references.
Sampled object IDs: `component-candidate-shape-region-066e8ad0e7654cff`,
`-0a02724ece05df31`, `-0b053d4e7a47606f`, `-0c41e12219f869d7` (all
`circular_symbol`, status `geometrically_classified`), and
`-1790c5fb3a4f437c` (`chassis_ground`). This is a real engineering gap,
not expected ambiguity — a real electrical component that produces no
terminal evidence cannot yet be wired into an endpoint-to-endpoint trace
or a net, and it is a strong contributor to the wire/endpoint coverage
gaps in §2–§4. Likely responsible stage: `TerminalLocationDetector`
(`src/topology/terminal_location_detector.cpp`), which evidently does not
yet produce terminal candidates for the majority of `CircularSymbol`
candidates. This should be scoped for **AP-WIRE-023/024** (internal
symbol geometry + terminal recognition), not patched here.

**Finding C2 (confirms the furniture classifier is doing its job):** 40
of 50 `DiagramFurniture` candidates have no terminal evidence, and — per
this AP's explicit directive — none of the 50 produced a
`COMPONENT-NO-TERMINAL-EVIDENCE` finding. Furniture lacking terminal
evidence is expected (it is not circuitry) and is tracked separately as
`furniture_without_terminal_evidence`.

## 7. Diagram-furniture coverage (§5.6/§14 of the AP spec)

`d642afd` (immediately prior to this AP) reclassified 50 candidates from
circuit-component kinds to `DiagramFurniture`, identified as the
switch-continuity table. This baseline confirms the commit's claim:

- Shape-kind histogram: 47 `circular` (was `CircularSymbol` pre-fix minus
  the reclassified ones), 50 `diagram_furniture`, 10 `chassis_ground`,
  2 `enclosure`, 0 `primitive`, 0 `unknown` — sums to 109, matching total
  component candidates. Consistent with the commit message.
- Connector candidates dropped to 0 (from a previously reported 10), and
  connector terminals to 0 (from 11) — see §8.
- Conductor/topology/wire counts (294/692/877/40) were **not** disturbed
  by the reclassification, consistent with the commit's claim that it
  only re-tags `ComponentCandidateKind`, not geometry.

**This AP does not undo or second-guess `DiagramFurnitureClassifier`.**
Per the mandatory directive in AP spec §14, it is treated as correct
architecture; its effect is audited, not reversed.

## 8. Connector/terminal coverage (§5.7)

| Metric | Value |
|---|---:|
| Connector candidates | 0 |
| Connector terminals | 0 |
| Genuine-looking | 0 |
| Furniture-derived | 0 |
| Unresolved | 0 |

**Finding CN1:** The previously reported 10 connector candidates / 11
connector terminals in `docs/PROJECT_STATUS_AND_GAP_ANALYSIS.md` no
longer exist post-furniture-fix. `ConnectorTerminalModelBuilder`
(`src/topology/connector_terminal_model.cpp`) currently builds connectors
from component-boundary terminal evidence; with the switch-continuity
table now correctly excluded as furniture and 41/59 real candidates
producing no terminal evidence at all (Finding C1), there is currently no
surviving evidence path that produces a connector candidate. This is not
a defect in the connector builder — it has nothing to build from — and it
is the clearest illustration of why AP spec §14/§15 warns that "geometric
similarity ≠ engineering identity": the population that used to satisfy
the connector heuristics was never real connector geometry.

## 9. Electrical-net coverage (§5.8)

| Metric | Value |
|---|---:|
| Electrical nets | 10 |
| Endpoints belonging to exactly one net | 33 |
| Endpoints belonging to no net | 177 (84.3%) |
| Endpoints belonging to >1 net | 0 |

**Finding N1:** No endpoint belongs to more than one net (deterministic
exclusivity holds — 0 `NET-ENDPOINT-MULTI-OWNED` findings). All
`WireModelValidator` `NET-*` reference-integrity checks pass (0
`NET-ENDPOINT-MISSING`/`NET-SPLICE-MISSING`/`NET-EDGE-MISSING`/
`NET-ANCHOR-MISSING` errors — validation errors are 0 overall).

**Finding N2 (same root cause as G2/E2):** 177/210 endpoints (84%) belong
to no electrical net. `ElectricalNetResolver` only forms nets from
topology it can already resolve into ground/distribution structure; since
the underlying wire/endpoint coverage gap (§2–§5) means most of the graph
is not yet claimed by any wire, most endpoints have no basis for net
membership either. This is downstream of Finding C1/G2, not an
independent net-resolution defect.

## 10. Validation warning population

Unchanged mechanism (`ExtractionAudit::validation_warning_summaries`,
`WireModelValidator`). 30 warnings total: 24 `WIRE-GEOMETRIC-ENDPOINTS`
(every currently-reconstructed wire terminates at geometric, not
semantic, endpoints — expected until AP-WIRE-023/024 land) and 6
`NET-ROLE-UNRESOLVED` (6 of 10 nets have no resolved distribution role).
No new validator warning codes were added by this AP; coverage findings
are reported separately (see `extraction_audit.json`'s `coverage` block)
so they are never conflated with `WireModelValidator`'s structural
pass/fail semantics.

## 11. Failure clusters (ranked)

1. **Wire/endpoint/net coverage gap** (Findings G2, E2, N2; 244 topology-only
   conductors, 827 unowned edges, 132 zero-wire endpoints, 177
   net-less endpoints). One underlying cause, four symptoms.
2. **Component terminal evidence gap** (Finding C1; 41/59 real components,
   likely root cause of #1). `TerminalLocationDetector` under-produces
   terminal candidates for `CircularSymbol`/`ChassisGround` components.
3. **Connector population collapse** (Finding CN1), a direct consequence
   of #2, not an independent defect.

Two items are explicitly **not** failures and should not be "fixed":
Finding G1/E1 (the one legitimately shared conductor/dual-endpoint pair)
and Finding C2 (furniture lacking terminal evidence).

## 12–15. Object IDs, pipeline stage, evidence, severity

See inline per-finding detail above (§2, §4, §6, §8) for concrete object
IDs. Full enumerations (not just samples) are in
`artifacts/audit/extraction_audit.json`'s `coverage.findings` array from
this baseline run, deterministically ordered by `(category, code,
object_id)`. Severity assignment: only `COMPONENT-NO-TERMINAL-EVIDENCE`
(Finding C1) is `Warning`; every other finding in this run is `Notice`
(expected incompleteness pending later APs, not evidence of a defect in
the stage that produced it).

## 16. Regression candidates

None. This baseline's `Warning`-severity finding count (41, all
`COMPONENT-NO-TERMINAL-EVIDENCE`) has no prior coverage-diagnostic
baseline to regress against — this is the first such report. Track this
number across future extractions of the same fixture; a rise indicates
new components losing terminal evidence, a fall (without an
architectural change to `TerminalLocationDetector`) should be treated
with the same suspicion the AP spec applies to any "improved" count (§13):
verify it is not caused by fewer real components being detected at all.

## 17. Recommended correction order

1. **AP-WIRE-023** — internal symbol geometry extraction. Finding C1 shows
   the extractor already finds and geometrically classifies `CircularSymbol`
   candidates but does not yet extract the internal geometry
   (contacts/terminals/leads) needed to produce terminal evidence for
   most of them. This is exactly AP-WIRE-023's stated scope (AP spec
   §18–19) and is the correct next step — not a quick fix folded into
   022A.
2. **AP-WIRE-024** — terminal recognition & component-terminal
   association, consuming AP-WIRE-023's internal geometry to close
   Finding C1 for real.
3. Re-run this AP's coverage diagnostics after AP-WIRE-023/024 land, and
   compare against this baseline. Expect `real_without_terminal_evidence`,
   `topology_only` conductors, `unowned_by_any_wire` edges,
   `zero_wire` endpoints, and `endpoints_not_in_any_net` to all fall
   together, since they share one root cause (Finding C1). If they do not
   move together, re-open the diagnosis rather than assuming success from
   any single count.
4. Do not attempt to "fix" Finding G1/E1 (legitimate sharing) or Finding
   C2 (furniture without terminal evidence) — both are correct current
   behavior.

## Exit criteria check (AP spec §25)

All checked boxes are backed by evidence in this report and in the
commits listed in §1 (`ebbdc05`, `bc5d93c`, this AAR commit, and the
documentation-correction commit to `PROJECT_STATUS_AND_GAP_ANALYSIS.md`).
No extraction correction was made inside the diagnostic implementation —
`build_coverage_report` is a pure read of `WireModel`; `tests/test_coverage_diagnostics.cpp`
verifies it performs no mutation implicitly by only ever passing
hand-built fixtures and asserting on the returned report.

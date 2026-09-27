# AP-DIAG-AUDIT-007 — Post-Ground-Recovery Whole-Diagram Re-Audit

This is an audit document. **No production classifier, extractor,
topology, terminal-resolver, Wire, ground, scope, or model code was
modified to produce it.** All work was read-only inspection of the
current committed model output, direct pixel inspection of
`samples/trx300ODG.png`, and the codebase's own non-mutating
`CoverageDiagnostics` observer (`src/core/coverage_diagnostics.cpp`,
AP-WIRE-022A) already embedded in `artifacts/audit/extraction_audit.json`.
`git status --short` was empty before this document and its JSON
artifact were added.

## 1. Executive Summary

The post-AP-DIAG-FIX-007 engineering model is **internally consistent**
and every FIX-007-attributed change is fully explained. The clean
Release rebuild passes 60/60 tests with exactly the pre-existing 8
compiler warnings (0 new). Fresh canonical extraction reproduces the
reported post-fix metrics exactly (37 Wires, 12 Electrical Nets, 6
genuine `ChassisGround` components, 6 resolved Ground endpoints, 0
validation errors, **34** runtime warnings — confirmed unchanged, not
assumed). Determinism holds across 2 unscoped and 2 scoped runs
(byte-identical `topology.json`/`engineering_diagram.json`, confirmed by
SHA-256).

Direct raster inspection (upscaled crops of both recovery ROIs)
independently confirms genuine ink for both recovered ground-approach
conductors — a diagonal jog into a vertical run for
`5aa211846bb7891d`, and an L-bend into a vertical run for
`6b6ccc2d59afe578` — matching AP-DIAG-FIX-007's description exactly. The
codebase's own `CoverageDiagnostics` observer independently confirms the
FIX-007 delta: `component_symbol_recognitions`/`symbol_primitives`
byte-identical, `conductor_segments.topology_only` unchanged at 251,
`conductor_segments.normal` +2 (39→41), and the
`COMPONENT-NO-TERMINAL-EVIDENCE` warning list shrinking from 25→23
entries with **exactly** `5aa211846bb7891d` and `6b6ccc2d59afe578`
removed and nothing else touched.

**No new production defect was found.** Two items are worth carrying
forward, neither attributable to FIX-007:

1. A **pre-existing, already-known** false-positive shape classification
   (2 components: `c2335cc575d107b1`, `d1aefcaca5503ad9` — first
   identified in AP-DIAG-AUDIT-004) remains present at the
   shape-classification level, but its downstream harm remains fully
   mitigated by AP-DIAG-FIX-005's ownership-evidence gate (both own zero
   endpoints, confirmed again in this audit).
2. A **suspected, unconfirmed** class of up to 21 additional small
   `circular_symbol`/`enclosure` components with no terminal evidence
   (the same `COMPONENT-NO-TERMINAL-EVIDENCE` list, minus the 2 confirmed
   false positives above) — direct visual spot-check of 4 of these 21
   found a mix of clearly genuine repeated engineering ring symbols and
   ambiguous small dot/loop marks that may be junction dots or
   crossing-hop conventions rather than components. This is reported as
   **suspected**, not confirmed, and currently causes **zero** downstream
   Wire/topology/terminal defect (the same ownership gate that protects
   against the confirmed false positives also protects against these).

Every other audited category — terminal attribution ownership evidence,
Wire identity uniqueness, shared-conductor accounting, ElectricalNet
role/anchor evidence, wire_semantics resolution correctness, and
scoped/unscoped consistency — is **clean** within the evidence examined.

## 2. Starting Repository State

- `git rev-parse HEAD`: `b8f53a2d8c9ee30c0c47a8a83f953535c2fa4dea`
- `git branch --show-current`: `main`
- `git status --short`: empty (clean)
- `git log -1 --oneline`: `b8f53a2 AP-DIAG-FIX-007: record final commit SHA in audit artifact`

All four starting conditions required by this AP's governing instructions
are satisfied; no discrepancy found, no branch created.

### Governing evidence reviewed

`docs/AP-DIAG-AUDIT-001` through `-006`, `AP-DIAG-FIX-001` through `-007`,
`AP-WIRE-029`, `AP-WIRE-030`, `AP-WIRE-031`, `AP-INGEST-001`,
`AP-INGEST-002` all exist under their exact stated filenames in `docs/`.

One discrepancy in the task's own file list: **`AP-WIRE-FIX-003` has no
standalone document file** under that name anywhere in `docs/`. It is
referenced only by title ("Deduplicate equivalent physical wire
discoveries" / "Fix the AP-WIRE-031 multi-way-fork over-discovery
defect") inside `docs/AP-DIAG-FIX-004_Physical_Wire_Record_Deduplication.md`
and `docs/AP-BASELINE-001_Verified_Baseline.md`, and its content
(four-way-fork/ambiguity-expansion handling) is exercised indirectly via
tests referenced from `AP-DIAG-FIX-004`. This audit treats
`AP-DIAG-FIX-004` plus those references as the authoritative record for
that prior work, per this AP's own instruction to verify actual
filenames before use rather than assume.

## 3. Canonical Input and Configuration

- Source raster: `samples/trx300ODG.png`,
  SHA-256 `a145fffeb4cc9261152930936737048e39e92b030de5c9427b8f60e427d5da6`
- Scope file (production-scoped runs):
  `fixtures/trx300/scope_production.json`,
  SHA-256 `8dd478483a26c93caef65a586c054d420cadb2032b370ad6f2e14c874021b491`
- CLI invocation (unscoped): `dx-extract extract samples/trx300ODG.png --output <dir>`
- CLI invocation (scoped): `dx-extract extract samples/trx300ODG.png --output <dir> --scope fixtures/trx300/scope_production.json`
- `source_id` for both: the literal string `samples/trx300ODG.png` (the
  CLI's `image_path` argument, per `stable_id()`'s established
  convention — unchanged from AP-DIAG-FIX-006/007).
- `ExtractionConfig`: default-constructed (no injected
  `text_recognition_provider`, `component_identity_registry`, or
  `symbol_recognition_provider` — all three remain no-op in every
  canonical run in this audit, confirmed by `text_recognition_evidence`,
  `component_identity_evidence`, and `engineering_object_semantics` all
  being empty (0 entries) in the fresh `topology.json`).

## 4. Build and Test Verification

Clean Release build from an empty `/tmp/audit007_build` directory
(`cmake -DCMAKE_BUILD_TYPE=Release` + full rebuild):

- Build result: **0 errors**.
- Compiler warnings: **8** (all pre-existing `-Wunused-function`/
  `-Wunused-result` warnings already catalogued in
  AP-DIAG-FIX-007 §17; 0 new).
- `ctest`: **60/60 passing** (matches the count established at the end of
  AP-DIAG-FIX-007 — 58 baseline + 2 new ground-recovery tests).
- Assertions active: confirmed via `dx-wire-test-assertions-enabled`
  passing (this test specifically fails if `NDEBUG` were set).

No test-count discrepancy to explain — 60/60 as expected.

## 5. Deterministic Baseline

Two independent unscoped extractions and two independent
production-scoped extractions were run against the freshly built,
clean-rebuilt `dx-extract` binary.

- Unscoped run 1 vs. run 2: `topology.json` and
  `engineering_diagram.json` deep-equal (`==`) in Python — **byte
  identical** except no volatile fields were present to exclude (no
  `generated_at` or similar timestamp field exists in either artifact).
- Scoped run 1 vs. run 2: same result — deep-equal.
- SHA-256 (unscoped `topology.json`):
  `99c24f99d2f59e1f502a8d66d5e2b55a714673cfd59bd31fbcdbd4547d329ee9`
- SHA-256 (unscoped `engineering_diagram.json`):
  `7e7d46da8f2d2dbdfeb359ea177cced0bd97e5ec447bfc86d7ccbbe9d93634d0`
- SHA-256 (scoped `topology.json`):
  `94b65c3addd0f1dfdb05b73cd379f72e86183bb1ea807700351fbe9d95030aa8`
- SHA-256 (scoped `engineering_diagram.json`):
  `91775ffdd28dfa133a04fa1949787f5b368072f4cd33d423b7c907061c793157`

### Unscoped canonical metrics (observed, from `topology.json`)

| Metric | Value |
|---|---:|
| `component_candidates` | 81 |
| `component_symbol_recognitions` | 81 |
| `symbol_primitives` | 34 |
| `connector_candidates` | 0 |
| `connector_terminals` | 0 |
| `endpoint_candidates` | 207 |
| `rejected_geometry` | 13 |
| `conductor_segments` (coverage total) | 292 |
| `nodes` (topology) | 690 |
| `edges` (topology) | 878 |
| `wires` | 37 |
| `wire_semantics` | 37 |
| `electrical_nets` | 12 |
| `conductor_boundary_resolutions` | 207 |
| Validation errors | 0 |
| Validation warnings | 34 |

Component classification distribution (via `component_symbol_recognitions.symbol_kind`):
`diagram_furniture`=47, `circular_symbol`=26, `enclosure`=2,
`chassis_ground`=6.

Endpoint-kind distribution: `geometric`=189, `component_terminal`=12,
`ground`=6.

Wire identity-status distribution: `resolved`=37, `unresolved`=0,
`conflicted`=0.

ElectricalNet role distribution: `unknown`=6, `ground`=6.

### Production-scoped canonical metrics (observed)

| Metric | Value |
|---|---:|
| `component_candidates` | 34 |
| `component_symbol_recognitions` | 34 |
| `symbol_primitives` | 34 |
| `connector_candidates` | 0 |
| `connector_terminals` | 0 |
| `endpoint_candidates` | 189 |
| `rejected_geometry` | 12 |
| `nodes` (topology) | 532 |
| `edges` (topology) | 643 |
| `wires` | 37 |
| `wire_semantics` | 37 |
| `electrical_nets` | 12 |
| `conductor_boundary_resolutions` | 189 |
| Validation errors | 0 |
| Validation warnings | 34 |

Component classification distribution: `circular_symbol`=26,
`enclosure`=2, `chassis_ground`=6 (`diagram_furniture` correctly excluded
by scope). Endpoint-kind: `geometric`=171, `component_terminal`=12,
`ground`=6. Wire identity-status: `resolved`=37 (all). ElectricalNet role:
`unknown`=6, `ground`=6.

All values above are observed directly from generated artifacts, not
assumed from prior documentation.

## 6. Complete Model Inventory

Per the codebase's own `CoverageDiagnostics::build_coverage_report()`
(non-mutating, embedded in `extraction_audit.json`), unscoped:

| Category | Total | Detail |
|---|---:|---|
| `conductor_segments` | 292 | unreferenced=0, topology_only=251, wire_only=0, normal=41, shared=0 |
| `endpoints` | 207 | zero_wire=133, single_wire=74, multiple_wire=0 |
| `wires` | 37 | valid=37, invalid=0 |
| `topology_edges` | 878 | missing_conductor=0, missing_from/to_node=0, unowned_by_any_wire=837 |
| `topology_nodes` | 690 | zero_degree=0, low_degree_splice=0, conductor_end_without_endpoint=0 |
| `components` | 81 | diagram_furniture=47, real_candidates=34, real_with_terminal_evidence=11, real_without_terminal_evidence=23, furniture_without_terminal_evidence=47 |
| `connectors` | 0 | (all sub-counts 0) |
| `electrical_nets` | 12 | endpoints_in_nets_total=37, endpoints_not_in_any_net=170, endpoints_in_multiple_nets=0 |

Total coverage findings: **1414**, of which **1391 are `notice`**
(expected, documented ambiguity per the tool's own design — e.g. the
251 `topology_only` conductor segments reflect `WireReconstructor`'s
documented degree-2-continuation-only scope boundary, AP-WIRE-029 §19,
unrelated to FIX-007) and **23 are `warning`**, all of a single code,
`COMPONENT-NO-TERMINAL-EVIDENCE` (Section 7 below).

`unowned_by_any_wire`=837 of 878 topology edges is consistent with only
37 Wires having been reconstructed by the conservative, never-guess
`WireReconstructor` walk — this matches the pre-existing, documented
architecture limitation (AP-WIRE-028/029), not a defect.

## 7. Component Audit

Per Section 6 of this AP's governing task, components are classified:

**A. Confirmed engineering components**: all 26 `circular_symbol` + 2
`enclosure` + 6 `chassis_ground` (34 total, matching the scoped
component count exactly) that are not flagged false-positive below.

**B. Confirmed non-component diagram furniture**: 47 components, all
classified `diagram_furniture`, all correctly excluded from the
production scope (scoped `component_candidates`=34=81-47).

**C. Confirmed false-positive components**: **2** —
`component-candidate-shape-region-c2335cc575d107b1` (874,430,7,7,
`circular_symbol`) and `component-candidate-shape-region-d1aefcaca5503ad9`
(629,543,8,10, `circular_symbol`). Both were identified in
AP-DIAG-AUDIT-004 as, respectively, the pointer/dot glyph of a "SUB FUSE
15A" annotation leader line and an ambiguous non-symbol mark. Both remain
present as shape-detection outputs (shape classification itself was
never in scope for correction) but both own **zero** `SymbolPrimitive`s
and **zero** `EndpointCandidate`s in the current model — confirmed again
in this audit — so AP-DIAG-FIX-005's ownership-evidence requirement
continues to fully suppress their downstream harm.

**D. Suspected false-positive components**: up to **21** additional
small `circular_symbol`/`enclosure` components appear in the
`COMPONENT-NO-TERMINAL-EVIDENCE` coverage-warning list (Section 6) with
no confirmed engineering-symbol identity. Four were visually
cross-checked by cropping and upscaling the source raster at their exact
bounds:

- `component-candidate-shape-region-d82ce67010a762de` (412,144,9,8) and
  `component-candidate-shape-region-1acff69cbd97d3ef` (425,144,9,9): a
  crop of this area shows **three repeated ring symbols in a row** on a
  wire run — visually consistent with a genuine, repeated small
  engineering symbol (e.g. an indicator/relay/connector ring), not a
  fabrication or a text/annotation artifact. Classified **not a false
  positive** — likely Category G (correct geometry, incomplete semantic
  recognition).
- `component-candidate-shape-region-25e5f8f85abff207` (780,509,7,6) and
  `component-candidate-shape-region-066e8ad0e7654cff` (911,509,8,9):
  crops show a small filled/ringed dot sitting directly on a wire lead
  entering an enclosure — consistent with a genuine small
  junction/splice-style mark, but its correct engineering representation
  (a component vs. a `Splice` topology node) could not be established
  from geometry alone. Classified **suspected, unconfirmed** — requires
  a targeted follow-up to determine whether this a component or splice
  under AP-WIRE-029 §9's rules.
- `component-candidate-shape-region-1975579e8330101e` (545,379,7,9): a
  crop shows this candidate sitting at a 4-way conductor crossing with a
  small loop/arc drawn at the intersection — visually consistent with a
  "hop"/"jump" no-connection crossing convention rather than an
  engineering component. This resembles, but does not exactly match by
  coordinates, the three false-positive circles AP-WIRE-FIX-002 already
  corrected (`(869,342)`, `(584,439)`, `(575,439)` — none match
  `(545,379)`). Classified **suspected false positive**, not confirmed —
  requires the same debug-instrumentation methodology AP-WIRE-FIX-002
  used (circularity/edge-support/interior-density dump) to confirm.

The remaining 17 of 21 were not individually visually verified in this
audit; per this AP's explicit instruction not to promote suspicion to
confirmation, they are listed as **Suspected (Category D/E), unconfirmed
either way** in Section 23, not asserted as defects.

**E. Components with insufficient evidence**: the same 23-entry
`COMPONENT-NO-TERMINAL-EVIDENCE` list, by construction — these are
exactly the "real" (non-furniture) components the model currently has no
terminal/endpoint evidence for at all, C and D above being that list's
full membership.

**F. Correct classification, incorrect terminal attribution**: **none
found**. Every `component_terminal`/`ground` endpoint (18 total) in the
current model has verified ownership evidence — either an owned
`SymbolPrimitive` or an associated `RejectedGeometryEvidence` entry
(`classification: component_associated`) — confirmed by an exhaustive
check across all 18 such endpoints, 0 violations. This directly
re-verifies that AP-DIAG-FIX-005's ownership gate still holds for the
complete current model, ground-recovery endpoints included.

**G. Correct geometry, incomplete semantic recognition**: the ring-symbol
pair above (`d82ce6`/`1acff6`) and, more generally, all 21 "suspected"
entries in D that visual inspection does not clearly show to be
fabricated ink — these most likely have real, correctly-detected
geometry but lack a symbol-family/engineering-role classification beyond
generic `circular_symbol`. This matches the pre-existing "incomplete
component semantic classification" known limitation (Section 21) rather
than being a new finding.

**Zero-`SymbolPrimitive` components are not per se false positives**: 68
of 81 components own zero `SymbolPrimitive`s; of these, only the 2 in C
are confirmed false positives, one (`845947b0afd7451a`) is a fully
legitimate component whose sole terminal is backed by
`RejectedGeometryEvidence` rather than `SymbolPrimitive`s (see Section 8),
and 47 are furniture (correctly zero-primitive by definition).

## 8. SymbolPrimitive Audit

`symbol_primitives`: **34**, byte-identical to the pre-FIX-007 baseline
(confirmed by ID-indexed diff in AP-DIAG-FIX-007's own verification,
re-confirmed here by direct inspection — no primitive's `component_id`,
`kind`, geometry, or confidence changed).

All 6 genuine `ChassisGround` components (including both recovered ones)
retain exactly **3** owned primitives each
(`terminal_lead`+`rectangle`+`line`, or an equivalent 3-primitive
pattern), matching AP-DIAG-AUDIT-006's comparative analysis exactly.
`component-candidate-shape-region-5aa211846bb7891d`'s primitives, for
example, are unchanged: a `terminal_lead` at (711,549,17,2), a
`rectangle` at (717,556,5,2), and a `line` at (715,553,10,1) — none of
these were touched by `GroundApproachConductorRecovery`, which reads only
the binary raster and existing `ConductorSegment`/exclusion-mask state,
never `SymbolPrimitive`s.

No duplicated conductor ink, no fabricated internal symbol geometry, and
no missing significant primitive was found for any of the 6 genuine
`ChassisGround` symbols. The ground-approach recovery introduced **no**
new primitive-ownership anomaly — `symbol_primitives` is a completely
untouched collection post-FIX-007 (0 added, 0 removed, 0 changed, per
the ID-indexed diff independently re-run in this audit).

## 9. Terminal Attribution Audit

All 18 `component_terminal`/`ground` endpoints in the current model were
checked against AP-DIAG-FIX-005's ownership rule: each must have either
an owned `SymbolPrimitive` or an associated `component_associated`
`RejectedGeometryEvidence` entry. **0 violations found** — every single
one has qualifying evidence.

The two confirmed AP-DIAG-AUDIT-004 false-positive components
(`c2335cc575d107b1`, `d1aefcaca5503ad9`) each own **zero** endpoints,
confirming FIX-005's gate continues to suppress them completely — no
regression, no bypass.

The AP-DIAG-FIX-006 corrected endpoint
(`endpoint-candidate-9da9c73ab52013a3`, component
`845947b0afd7451a`) remains `component_terminal`/`high` confidence,
backed by exactly the same 2 `RejectedGeometryEvidence` entries
documented in FIX-006 — unchanged.

`TerminalLocationDetector`, `TerminalRecognizer`, `ConductorBoundaryResolver`,
and `EndpointSemanticReconstructor` outputs were not independently
re-implemented for this audit, but their aggregate output (the
`endpoint_candidates` and `conductor_boundary_resolutions` collections)
was fully re-verified as internally consistent: no endpoint is attributed
to a component purely by geometric proximity without qualifying
ownership evidence, and no dangling terminal candidate was found
attributed to more than one component. `connector_candidates`/
`connector_terminals` remain 0/0 in both scoped and unscoped runs — the
rejected_geometry handoff correctness this depends on
(AP-DIAG-FIX-006) was re-confirmed by the ownership check above, since
that handoff is exactly what supplies `845947b0afd7451a`'s evidence.

Dangling terminal candidates (endpoints with `kind: geometric` not
attached to any Wire): **133** of 207 (`coverage.endpoints.zero_wire`),
consistent with the pre-existing, documented `WireReconstructor` scope
boundary (Section 6) — these are not attributed to any component at all
(`component_id` empty), so they carry no false-attribution risk.

## 10. Ground Endpoint Audit

All 6 genuine `ChassisGround` components were traced end-to-end
(`ShapeDetector` → `ComponentCandidate` → `SymbolPrimitives` →
`ConductorSegment`/topology → `EndpointCandidate` →
`ConductorBoundaryResolver` → Ground endpoint):

| Component | Primitives | Ground Endpoint | Confidence |
|---|---:|---|---|
| `0edf5ce35037fec3` | 3 | `endpoint-candidate-bfae64deea64562f` | high |
| `1acbaeb7ac6ac87a` | 3 | `endpoint-candidate-9d7e7957c56e3777` | high |
| `5aa211846bb7891d` | 3 | `endpoint-candidate-489024c295007fa9` | high |
| `6b6ccc2d59afe578` | 3 | `endpoint-candidate-ee69f47db68cb6bf` | high |
| `791276441805b2e0` | 3 | `endpoint-candidate-94c3703334d041e1` | high |
| `a434a925670e9b65` | 3 | `endpoint-candidate-682d9fd165bc9fcf` | medium |

All 6 resolve exactly one Ground endpoint each; none resolve zero or more
than one.

**Raster verification of the two recovered components**: upscaled crops
(6-8×, nearest-neighbor) of both recovery ROIs were generated directly
from `samples/trx300ODG.png` and visually reviewed:

- `5aa211846bb7891d` (ROI ~700-735, 520-550): shows a clear diagonal wire
  entering from the upper-left, a junction/bend point, and a short
  vertical run down into the ground bar — exactly the diagonal-jog shape
  AP-DIAG-FIX-007 describes. Independent adaptive-threshold ink count in
  this exact ROI (same 31/7 parameters `MorphologyWireDetector` uses):
  **109/500 px** — substantial, unambiguous ink, not noise.
- `6b6ccc2d59afe578` (ROI ~740-775, 520-550): shows a clear L-bend
  (horizontal run then a corner into a vertical run) down into the ground
  bar — exactly the L-bend shape described. Independent ink count:
  **108/500 px**.

**The complete recovered path** was checked against the model's own
topology, not just the endpoint: each recovered `Wire`
(`wire-5f577a77a7d7f3d1` for `5aa`, `wire-cc1a42c6ede95cf1` for `6b6`)
resolves to exactly **one** topology edge and **one** accepted
`ConductorSegment`, running from the Ground endpoint to a `geometric`
`conductor_end` a short distance away — `(719,547)`→`(719,534)` for
`5aa`, `(759,547)`→`(758,532)` for `6b6`. The raw recovery's earlier,
farther-out polyline segments (the diagonal-jog and L-bend-top portions
themselves) were correctly **rejected** by the unmodified
`ConductorEvidenceEvaluator` as `below_minimum_continuous_length` (see
Section 11) — the same evidence-length rule applied to every other
conductor in the diagram, not something FIX-007 introduced or weakened.

This means the final accepted geometry for both recovered Wires is the
*last straight run* into the ground bar, not the full bend — a
conservative, evidence-driven truncation, not a straight-line
oversimplification of a *shorter* physical run that genuinely does bend
(the discarded diagonal/L segments are represented as
`RejectedGeometryEvidence`, preserving their provenance rather than
silently disappearing). **Both new Wires' far endpoints are dangling
`GeometricConductorEnd`s (topology node degree 1)** — they do not
connect onward to any other topology. This is not unique to the
recovered components: all **4** previously-successful ground Wires
exhibit the identical pattern (each terminates at a degree-1 geometric
end a short distance from the ground bar:
`(935,488)`, `(656,522)`, `(629.5,556)`, `(585.5,557)`). This is
consistent, pre-existing model behavior, not a regression — see Section
19 for its status as a known limitation rather than a defect.

The 4 previously-successful Ground endpoints
(`0edf5ce35037fec3`, `1acbaeb7ac6ac87a`, `a434a925670e9b65`,
`791276441805b2e0`) are confirmed **unchanged**: same coordinates
(585.5/629.5/935/656, 585/585/542/546), same confidence
(high/high/medium/high), same component attribution, re-verified by
direct inspection in this audit (not merely trusted from FIX-007's own
report).

Per this AP's explicit caution, six resolved Ground endpoints is **not**
treated as proof that all ground-related geometry is otherwise correct —
Section 11 and the dangling-far-endpoint observation above are the
result of taking that caution seriously.

## 11. Recovered-Conductor Provenance

Both recovered components' full segment provenance (accepted +
rejected) was reconstructed from `rejected_geometry` and the accepted
topology edges:

**`5aa211846bb7891d`** — 3 raw segments from the recovery's simplified
polyline:
1. `(707.5,527)→(708,533)` — rejected, `below_minimum_continuous_length`,
   `unresolved` (length 6.02px).
2. `(708,533)→(719,534)` — rejected, `below_minimum_continuous_length`,
   `unresolved` (length 11.05px).
3. `(719,534)→(719,547)` — **accepted**, forms
   `topology-edge-0192426398ea0a68`, member of `wire-5f577a77a7d7f3d1`.

**`6b6ccc2d59afe578`** — 2 raw segments:
1. `(768,527)→(758,532)` — rejected, `below_minimum_continuous_length`,
   classified `text_associated` against `text-region-51` (length
   11.18px) — this is the exact overlap AP-DIAG-FIX-007 documented as
   the reason recovered segments bypass
   `GeometryOwnershipClassifier`'s generic overlap check; here,
   downstream, `ConductorEvidenceEvaluator` independently rejects it
   anyway for insufficient length, which is a *different*, legitimate
   reason unrelated to the ownership bypass.
2. `(758,532)→(759,547)` — **accepted**, forms
   `topology-edge-3f1a3ddcfe22fc0b`, member of `wire-cc1a42c6ede95cf1`.

No duplicate conductor segments, no artificial connections to unrelated
geometry, and no unintended crossings were found: both accepted edges
connect exactly 2 brand-new topology nodes each (4 new nodes total,
matching the Section 5/FIX-007 delta), neither of which coincides with
or was merged into any pre-existing node. Recovery does not bypass
normal conductor evidence evaluation — both examples above show
`ConductorEvidenceEvaluator` actively rejecting weaker recovered
sub-segments exactly as it would for morphology-detected geometry.

The raster ROI values independently computed in Section 10 (109 and 108
ink pixels respectively) directly support the recovered geometry's
existence; no coordinate in either final accepted segment falls outside
the ink-containing ROI.

## 12. Conductor Geometry Audit

`conductor_segments` (coverage total): 292 unscoped (290 pre-FIX-007 +2
from recovery). Ownership classes: `unreferenced`=0 (no orphaned
segments), `topology_only`=251 (unchanged from pre-fix — the documented
`WireReconstructor` scope boundary, Section 6), `wire_only`=0,
`normal`=41 (39 pre-fix +2 recovered), `shared`=0.

No fabricated conductor segment was found: every segment in `normal` or
`topology_only` traces to either `MorphologyWireDetector`'s morphological
extraction (unchanged, untouched by FIX-007) or, for exactly 2 segments,
`GroundApproachConductorRecovery`'s raster-verified local trace (Section
11). `shared`=0 means no conductor segment is currently referenced by
more than one Wire — the AP-WIRE-028-era single shared-conductor
observation no longer applies to the current, much-revised
reconstruction (37 Wires today vs. 50 at that much earlier baseline); this
is not evidence of a defect, simply a different point in the pipeline's
evolution.

No missing-junction or missing-splice defect was identified beyond the
already-documented, unchanged limitation that 0 `Junction`-type nodes
currently exist in the model (AP-WIRE-029 §10 — no extraction stage
currently produces them; unrelated to FIX-007). No conductor segment was
found associated with text or annotations that survived into an accepted
Wire — the one case that came close (`6b6`'s rejected `text_associated`
segment, Section 11) was correctly excluded from the final model.

A full pixel-level sweep of every one of the 292 conductor segments
against the source raster was not performed (out of proportion to this
AP's scope); the audit instead prioritized the two FIX-007-recovered
segments (fully raster-verified, Section 10) and the previously
documented failure modes (AP-DIAG-AUDIT-004/005/006), consistent with
this AP's instruction to prioritize suspicious/previously-documented
regions while not limiting the audit to them. No additional suspicious
region was surfaced by the `CoverageDiagnostics` warning-severity
findings beyond the `COMPONENT-NO-TERMINAL-EVIDENCE` class already
covered in Section 7.

## 13. Topology Audit

Topology node-type distribution (unscoped): `crossing`=239,
`conductor_end`=207, `continuation`=139, `splice`=105 (sum 690, matches
node count exactly). All 4 nodes introduced by FIX-007 are
`conductor_end` type, `electrically_connective: true`, at the exact
coordinates documented (Section 10/11) — no `splice`, `junction`, or
`crossing` node was fabricated by the recovery.

Both new topology edges connect exactly the 2 new nodes for their
respective component pair, each referencing exactly one
`normalized-conductor-segment` — no fan-out, no incidental connection to
any pre-existing node or edge. `topology_edges.missing_conductor`,
`missing_from_node`, and `missing_to_node` are all 0 — full referential
integrity across all 878 edges, not only the 2 new ones.

No unrelated topology was modified: the ID-indexed diff (re-run
independently in this audit against the same isolated pre-fix baseline
methodology FIX-007 itself used) confirms 0 changes to any of the 686
pre-existing nodes or 876 pre-existing edges.

## 14. Physical Wire Identity Audit

All 37 Wires were checked for duplicate endpoint pairs (0 found), for
identical start/end endpoints (self-loops; 0 found), and for shared
`conductor_segments` across more than one Wire (0 found — consistent
with `coverage.conductor_segments.shared`=0). `wire_identity_status`
distribution: **37 `resolved`, 0 `unresolved`, 0 `conflicted`** — every
currently-reconstructed Wire is a clean, unambiguous
degree-1-endpoint → degree-2-continuation* → degree-1-endpoint walk
(the only shape `WireReconstructor` currently attempts, per AP-WIRE-029
§19); this is not evidence that "Resolved" was applied to an ambiguous
case, since the reconstructor's conservative scope boundary means it
never reaches a splice/junction/crossing in the first place for any of
these 37.

The two newly recovered Wires
(`wire-5f577a77a7d7f3d1`, `wire-cc1a42c6ede95cf1`) were independently
re-verified against source geometry in Sections 10-11: both endpoints,
both topology edges, and both conductor segments are backed by real ink
and real topology, with no shorter/straighter/closer-path guess involved
(the governing §18 never-guess rule from AP-WIRE-029 was not implicated
by this fix — the recovery either finds a continuous, bounded, raster-
justified path to the ground symbol or produces nothing; it never chose
between plausible alternatives).

No Wire in the current model is `Conflicted`; none is `Resolved` without
supporting `identity_evidence_ids` (spot-checked across both new Wires
and the 4 previously-successful ground Wires — all reference concrete
`conductor-boundary-resolution-*` IDs).

## 15. ElectricalNet Audit

12 nets total: 6 `unknown` role (`confidence: unresolved`, unchanged from
before FIX-007 — the same 6 net IDs, same endpoint memberships,
byte-identical), 6 `ground` role (`confidence: high`) — 4 pre-existing +
2 new.

The 2 new nets (`electrical-net-ad8ebe90235978f9`,
`electrical-net-647be111f38562cc`) were specifically inspected for
whether the source diagram supports merging either into an existing net
rather than treating it as distinct: each new net's only conductor
evidence is its single new topology edge, and that edge's far endpoint
is a degree-1 dangling `GeometricConductorEnd` with **zero** further
topology connection (Section 10) — there is no topological path from
either new net to any other net's endpoints. Treating them as
genuinely distinct nets is therefore the only evidence-supported outcome;
merging them with an existing net (including each other, or any of the 4
pre-existing ground nets) would have required fabricating connectivity
the model does not have. This is not "common chassis ground implies one
net" reasoning — it is the absence of any physical conductor path, which
is the correct basis for treating these as physically separate ground
paths whether or not they are electrically the same chassis rail on the
physical PCB/harness.

`endpoints_in_nets_total` rose from 33→37 (+4, exactly the 4 new
endpoints across the 2 new nets), `endpoints_not_in_any_net` stayed at
170 (unchanged — arithmetic is internally consistent: 33+170=203 before,
37+170=207 after). `endpoints_in_multiple_nets`=0 in both — no endpoint
is double-counted across nets. `ElectricalNetResolver` itself was
confirmed unmodified (Section 27).

## 16. Wire Semantics Audit

37 `wire_semantics` records (35 pre-existing +2 new), all byte-identical
for the 35 pre-existing ones. The 2 new records
(`wire-semantic-resolution-a7a1edecf3bcb7d3` for `5aa`,
`wire-semantic-resolution-61c4bcb649414c9a` for `6b6`) correctly resolve:
the ground-side `start_component_id`/`end_component_id` (whichever end is
the ground symbol) as `resolved`, the far/geometric side left
`unresolved` (accurately — no component evidence exists there), and
`electrical_net_id`/`electrical_net_status`/`electrical_net_confidence`
resolved to the matching new net at `high` confidence. `wire_color` and
`function_label` remain correctly `unresolved` for both (no OCR ran).

Aggregate distribution: `start_component_status`/`end_component_status`
`resolved`=5/6 of 37 (exactly the ground-anchored side of each of the 6
ground Wires), all others `unresolved` — consistent, no
resolved-without-evidence record found. AP-DIAG-FIX-005 and FIX-006
corrections remain intact: `845947b0afd7451a`'s wire-semantic record is
unchanged, and no `wire_semantics` record references either of the 2
confirmed false-positive components (`c2335…`, `d1aef…`) as a start/end
component.

## 17. Validation Warning Audit

Exactly 2 warning codes account for all 34 runtime warnings, both
unscoped and scoped:

| Code | Count | Trigger | Legitimate? | Defect? |
|---|---:|---|---|---|
| `NET-ROLE-UNRESOLVED` | 6 | An `ElectricalNet` has `role: unknown` | Yes — these 6 nets' role genuinely cannot be determined from current evidence (no circuit-role text/label evidence reaches them; OCR is off in the canonical baseline) | No — known, unchanged, pre-existing limitation (AP-WIRE-022 era) |
| `WIRE-GEOMETRIC-ENDPOINTS` | 28 | Both of a Wire's endpoints are `GeometricConductorEnd` | Yes — these are exactly the Wires `WireReconstructor`'s conservative walk produced without reaching any semantically-classified boundary | No — same known limitation, unrelated to ground endpoints (which have exactly one `ground`, not `geometric`, endpoint, so none of the 6 ground Wires — old or new — ever triggers this code; confirmed by direct inspection of the validator's condition, `wire_model_validator.cpp:279-289`, which requires *both* endpoints to be geometric) |

This exactly explains why the warning count stayed at 34 despite Wires
rising 35→37: neither new Wire has two geometric endpoints (each has one
`ground` + one `geometric`), so neither can trigger
`WIRE-GEOMETRIC-ENDPOINTS`, and neither's net is `unknown`-role, so
neither triggers `NET-ROLE-UNRESOLVED`. Zero validation errors continues
to not mean the model is fully correct — Section 7's suspected
false-positive/incomplete-classification components produce no
validation warning of any kind (validation only checks internal
referential/logical consistency, not raster-truth), which is exactly why
this audit performed independent raster-level checks in Sections 7 and
10 rather than relying on validation output alone.

## 18. Scoped/Unscoped Comparison

Every scoped vs. unscoped difference is explained by legitimate scope
exclusion of `diagram_furniture` components:

| Collection | Unscoped | Scoped | Difference explained |
|---|---:|---:|---|
| `component_candidates` | 81 | 34 | -47 `diagram_furniture` excluded |
| `symbol_primitives` | 34 | 34 | furniture owns 0 primitives — no change |
| `rejected_geometry` | 13 | 12 | -1, a pre-existing (pre-FIX-007) single border-region entry near image edge x=1, unrelated to furniture exclusion — same 1-item gap documented in AP-DIAG-FIX-006 §9 (9 vs 10 there), reproduced identically here (12 vs 13) |
| `endpoint_candidates` | 207 | 189 | -18 geometric endpoints that only existed near excluded furniture regions |
| `wires`/`wire_semantics`/`electrical_nets` | 37/37/12 | 37/37/12 | **identical** — no Wire, wire-semantic record, or net is scope-dependent |
| Ground endpoints resolved | 6/6 | 6/6 | **identical** |
| Validation errors/warnings | 0/34 | 0/34 | **identical** |

Both recovered Ground endpoints (`5aa211846bb7891d`, `6b6ccc2d59afe578`)
are present and resolved in both scopes, confirmed by direct inspection
of `endpoint_candidates` in the scoped `topology.json`. The
`COMPONENT-NO-TERMINAL-EVIDENCE` coverage-warning list is **identical**
(same 23 IDs) in both scopes, since it only concerns non-furniture
components. No unexpected scope-dependent difference was found.

## 19. False-Positive Sweep

**Confirmed false positives**: 2 (`c2335cc575d107b1`,
`d1aefcaca5503ad9`) — pre-existing, fully mitigated by FIX-005's
ownership gate (Sections 7, 9). No new confirmed false positive was
found: no fabricated component, no fabricated conductor geometry beyond
what Sections 11-12 already fully accounted for, no false terminal
(Section 9, 0 violations), no false junction (0 `Junction` nodes exist at
all), no false splice (105 splices, all pre-existing/unchanged), no
false Ground symbol (all 6 `ChassisGround` recognitions independently
re-verified against 3 owned primitives each), no false Ground endpoint
(both new ones raster-verified in Section 10), no duplicate physical Wire
(Section 14, 0 duplicate endpoint pairs), and no unsupported electrical
connectivity (Section 15 — both new nets' isolation was verified, not
assumed).

**Suspected false positives**: up to 21 additional small
`circular_symbol` components (Section 7, Category D) — explicitly not
promoted to confirmed.

## 20. False-Negative Sweep

No missing genuine component, missing terminal, missing junction, missing
splice, or missing physical Wire was identified as **confirmed** by this
audit — this AP's instruction is explicit that a missing object must be
supported by identifiable source evidence, not inferred from an expected
count, and no such evidence was found during the checks performed
(raster verification of both recovery regions, the `CoverageDiagnostics`
warning-severity sweep, and the scoped/unscoped comparison).

**Missing connector evidence** remains a **known, unchanged limitation**:
`connector_candidates`/`connector_terminals` are 0/0 in every run in this
audit (both scopes) — this was already established as a standing,
documented gap (AP-WIRE-028, "connector-recognition path is currently
unreachable") and is unrelated to FIX-007.

**Missing conductor sections**: the two segments FIX-007 recovered were
themselves missing conductor sections that are now present; whether
*further* missing conductor sections exist elsewhere in the diagram
(beyond the 251 `topology_only` segments, which are present but not
Wire-connected, not literally missing) was not exhaustively re-verified
pixel-by-pixel across the whole 1056×816 raster in this audit — this
would require the same kind of targeted, hypothesis-driven forensic
process AP-DIAG-AUDIT-006 used for the ground case, and no specific
trigger (an unresolved endpoint cluster, a validation warning, a user
report) currently points at a specific unexamined region the way the
ground endpoint gap did. This is recorded as an **audit-scope
limitation**, not a finding of absence.

## 21. Known Limitations

Re-verified against the current implementation (not copied from the
prior list without checking):

- **OCR/text recognition**: confirmed still not enabled in the canonical
  baseline — `main.cpp`'s default `extract` path only wires a
  `TextRecognitionProvider` when `--recognition` or
  `--vision-recognition` is passed; neither was used for any run in this
  audit. `text_recognition_evidence`=0 in every canonical run confirms
  this directly rather than by code inspection alone.
- **Connector recognition coverage**: still 0/0
  `connector_candidates`/`connector_terminals` in both scopes — unchanged
  standing gap.
- **Incomplete component semantic classification**: re-confirmed and
  narrowed by this audit — specifically the 21 unconfirmed
  `circular_symbol`/`enclosure` candidates in Section 7 Category D/G.
- **Unresolved electrical circuit roles**: 6 nets remain `role: unknown`
  — unchanged, `NET-ROLE-UNRESOLVED` warning code, Section 17.
- **`WireReconstructor` scope boundary**: 251/292 conductor segments
  remain `topology_only` (not part of any Wire) and 837/878 topology
  edges remain `unowned_by_any_wire` — this is the same conservative,
  documented, never-guess-compliant boundary from AP-WIRE-029 §19,
  unaffected by FIX-007 (which only ever adds `normal`-class segments,
  never converts existing `topology_only` segments).
- **No `Junction`-type topology nodes**: still 0, per AP-WIRE-029 §10 —
  unaffected.
- **Dangling geometric far-endpoints on ground Wires**: newly documented
  by this audit (Section 10) as a *consistent*, not `FIX-007`-specific,
  characteristic of every one of the 6 ground Wires — none of the 6
  connects onward past its immediate approach conductor to the rest of
  its circuit. This is a coverage limitation of the underlying
  morphology/topology extraction, not a ground-recovery defect.
- **Component identity registry / symbol recognition provider**: both
  remain no-op (0 `component_identity_evidence`, 0
  `engineering_object_semantics`) in the canonical baseline — unchanged.

## 22. Confirmed Findings

**AP-DIAG-AUDIT-007-F1** (INFORMATIONAL, pre-existing, not a regression):
2 confirmed false-positive shape classifications
(`component-candidate-shape-region-c2335cc575d107b1`,
`component-candidate-shape-region-d1aefcaca5503ad9`) remain present at
the `ShapeDetector`/`component_symbol_recognitions` level. Downstream
impact: **none** — both own 0 `SymbolPrimitive`s and 0
`EndpointCandidate`s, fully suppressed by AP-DIAG-FIX-005's ownership
gate. Earliest defective stage: `ShapeDetector`'s circle-acceptance path
(same class of issue AP-WIRE-FIX-002 partially addressed for a different,
non-overlapping set of 3 coordinates). Root cause: not independently
re-established in this audit (already established in AP-DIAG-AUDIT-004).
Scoped/unscoped: identical in both. Deterministic: yes. Suggested
corrective boundary: `ShapeDetector`'s circle/annotation-glyph
disambiguation logic only — must not touch `TerminalLocationDetector`,
`TerminalRecognizer`, or any FIX-005/007 ownership logic, since those are
already proven correct and sufification. Recommended next AP: an
`AP-DIAG-FIX` scoped exclusively to `ShapeDetector` circle
classification, using the same debug-instrumentation-then-revert
methodology AP-WIRE-FIX-002 used, generalized to also catch
non-bus-crossing annotation-leader dots.

No other confirmed defect was found in this audit.

## 23. Suspected Findings

**AP-DIAG-AUDIT-007-S1** (LOW, unconfirmed): up to 21 additional small
`circular_symbol`/`enclosure` components
(full ID list in `artifacts/audit/post_ground_recovery_reaudit.json`,
`suspected_findings`) may include further false-positive shape
classifications (crossing-hop/junction-dot conventions misclassified as
components) alongside genuine, correctly-detected-but-semantically-
incomplete symbols (e.g. the confirmed-genuine repeated ring symbols at
`(412,144)`/`(425,144)`). **Missing evidence to confirm or dismiss**: a
per-candidate visual/forensic pass using AP-WIRE-FIX-002's
circularity/edge-support/interior-density methodology (or equivalent),
applied to all 21, to separate genuine small engineering symbols from
crossing/junction drawing conventions. Current downstream impact: **zero**
(all 21 own 0 endpoints; the FIX-005 ownership gate protects against
harm regardless of the outcome of this classification). Not promoted to
confirmed.

**AP-DIAG-AUDIT-007-S2** (INFORMATIONAL, unconfirmed): whether any
further genuinely missing conductor section exists beyond the two
FIX-007 recovered was not exhaustively verified pixel-by-pixel across the
whole diagram (Section 20). **Missing evidence to confirm or dismiss**: a
systematic raster-coverage comparison (all significant dark-ink
components vs. all extracted `ConductorSegment`/`SymbolPrimitive`
geometry) across the full 1056×816 image, which no specific existing
signal (warning, unresolved-endpoint cluster) currently points at a
specific unexamined region for.

## 24. Clean Categories

Within the evidence examined in this audit, the following are reported
**clean**:

- Terminal-attribution ownership evidence (Section 9): 0 violations
  across all 18 `component_terminal`/`ground` endpoints.
- Ground endpoint coverage and provenance (Section 10): all 6 genuine
  `ChassisGround` components resolve exactly one Ground endpoint each,
  both recovered ones raster-verified.
- Recovered-conductor provenance (Section 11): no duplicate, artificial
  connection, unintended crossing, or unjustified source-pixel
  interpretation found in either recovered path.
- Topology referential integrity (Section 13): 0 missing-conductor/
  missing-node edges across all 878 edges; 0 zero-degree nodes across
  all 690 nodes.
- Physical Wire identity (Section 14): 0 duplicate endpoint pairs, 0
  self-loops, 0 shared conductor segments across all 37 Wires; 37/37
  `resolved`, 0 `unresolved`/`conflicted`.
- ElectricalNet integrity (Section 15): 0 endpoints in multiple nets; the
  2 new nets' distinctness is evidence-supported, not guessed.
- Wire semantics (Section 16): 0 resolved-without-evidence records; both
  new records correctly mixed resolved/unresolved per available
  evidence.
- Scoped/unscoped consistency (Section 18): every difference fully
  explained by furniture exclusion or a pre-existing, already-documented
  1-item border-region gap.
- Determinism (Section 5): byte-identical across 2 unscoped + 2 scoped
  runs, confirmed by SHA-256.
- `SymbolPrimitive` integrity for all 6 genuine `ChassisGround` symbols
  (Section 8): unchanged, 3 primitives each, no fabricated internal
  geometry.

## 25. Recommended Next AP

No further production defect was confirmed by this audit that rises
above the pre-existing, already-fully-mitigated
AP-DIAG-AUDIT-004-class false positive (Section 22, F1). Per this AP's
own instruction ("if no further production defect is confirmed,
recommend baseline consolidation instead of inventing additional work"),
the recommendation is:

**Primary recommendation: baseline consolidation.** Treat commit
`b8f53a2` plus this audit as the new verified baseline (analogous to
`docs/AP-BASELINE-001_Verified_Baseline.md`) rather than opening a new
fix AP purely on the strength of this audit's findings, since F1 has
zero current downstream impact and S1/S2 are explicitly unconfirmed.

**If further work is prioritized**, in order of confirmed-evidence
strength:

1. A narrowly-scoped `ShapeDetector` circle/annotation-glyph
   disambiguation AP-DIAG-FIX addressing F1 and investigating S1,
   generalizing AP-WIRE-FIX-002's methodology.
2. A connector-recognition AP addressing the standing 0/0
   `connector_candidates`/`connector_terminals` gap (Section 21),
   independent of anything found in this audit.
3. A `WireReconstructor` decomposition-stage AP (the AP-WIRE-029 §20
   "proposed conceptual pipeline") to reduce the 251/292
   `topology_only` conductor-segment backlog — a large, well-understood,
   pre-existing architecture gap, not a bug.

## 26. Canonical Artifact Hashes

| Artifact | Scope | SHA-256 |
|---|---|---|
| `topology.json` | unscoped | `99c24f99d2f59e1f502a8d66d5e2b55a714673cfd59bd31fbcdbd4547d329ee9` |
| `engineering_diagram.json` | unscoped | `7e7d46da8f2d2dbdfeb359ea177cced0bd97e5ec447bfc86d7ccbbe9d93634d0` |
| `topology.json` | scoped | `94b65c3addd0f1dfdb05b73cd379f72e86183bb1ea807700351fbe9d95030aa8` |
| `engineering_diagram.json` | scoped | `91775ffdd28dfa133a04fa1949787f5b368072f4cd33d423b7c907061c793157` |
| `samples/trx300ODG.png` | (source) | `a145fffeb4cc9261152930936737048e39e92b030de5c9427b8f60e427d5da6` |
| `fixtures/trx300/scope_production.json` | (scope config) | `8dd478483a26c93caef65a586c054d420cadb2032b370ad6f2e14c874021b491` |

All four model-output hashes were confirmed reproducible across 2
independent runs each (Section 5).

## 27. Explicit Non-Changes

No production code was modified during this audit. Confirmed unmodified
by direct diff/inspection: `MorphologyWireDetector`, `ShapeDetector`,
`GroundApproachConductorRecovery`, `TerminalLocationDetector`,
`TerminalRecognizer`, `ConductorBoundaryResolver`,
`EndpointSemanticReconstructor`, `GeometryOwnershipClassifier`,
`ConductorEvidenceEvaluator`, `WireReconstructor`, `WireSemanticResolver`,
`ElectricalNetResolver`, `ExtractionPipeline`, all model definitions
(`include/eke_dx_wire/core/model.hpp`), and the global morphology/scope
configuration defaults. No extraction heuristic, parameter, or threshold
was tuned. No ambiguity encountered during this audit was resolved by
guessing — Sections 22-23 explicitly separate confirmed from suspected
findings rather than asserting suspected ones as fact.

## 28. Final Commit and Push Status

Commit: see Final AAR below (this document and its companion JSON
artifact were the only files changed by this AP). Pushed directly to
`origin/main`; no branch created.

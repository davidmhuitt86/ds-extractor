# AP-DIAG-AUDIT-002 — Post-Scoping Whole-Diagram Re-Audit

This is an audit document. No production extraction/classification code,
Wire identity logic, scope semantics, or topology algorithm was modified to
produce it. Every count below was pulled from a fresh extraction on this
AP's starting commit; every visual claim was checked by cropping and
viewing the actual source raster (`samples/trx300ODG.png`), not inferred
from names, IDs, or the prior audit's own conclusions.

## 1. Baseline Verification

- `git rev-parse HEAD` before any work: `d3591b4f8130f4e65f5747e35e67258815ee1c82`
  — matches this AP's required starting SHA.
- `git status --short`: empty (clean).
- Clean Release rebuild (`rm -rf build`, fresh `cmake`/`build`): build
  succeeded with 0 errors.
- Full assertion-enabled test suite: **55/55 passing**.
  `dx-wire-test-assertions-enabled` passing confirms assertions are active
  (per AP-TEST-FIX-001's `-UNDEBUG` convention).
- Compiler warnings: the same 8 pre-existing, unrelated warnings as every
  prior AP (2 `[[nodiscard]]` test-file warnings, 6 unused
  function/anonymous-namespace warnings in production `.cpp` files) — no
  new warning introduced.

## 2. Two Extraction Baselines

Both run fresh from the rebuilt binary, neither fixture modified:

- **A. Unscoped**: `dx-extract extract samples/trx300ODG.png --output <dir>`
- **B. Scoped**: `dx-extract extract samples/trx300ODG.png --output <dir> --scope fixtures/trx300/scope_production.json`

Reproducibility: unscoped `extraction_audit.json`/`wires.svg` SHA-256
(`c4e99f52bedfe3e4083f4f5c5df6bd283696e1365671c7f8c5a5cd6da212432f` /
`3f89de85415273f4546adab7e2f0d2f6b8eb3edf013feacb1ffa41935a2cfd6b`) are
**byte-identical** to the hash table every prior AP (AP-BASELINE-001
through AP-INGEST-002) has recorded. Wire-identity determinism for both
the unscoped and scoped configurations was proven directly by
AP-INGEST-002 (independent A/B runs, `diff -rq`, zero differences except
the documented `review_manifest.json` timestamp); nothing in the pipeline
that determinism proof depends on has changed since, so it is treated as
still valid rather than re-run from scratch.

## 3. Whole-Diagram Inventory

Full detail in `artifacts/audit/post_scoping_inventory.json`. Pulled from
`artifacts/topology/topology.json` (the authoritative structured-object
export), not from aggregate-only summaries.

| Category | Unscoped | Scoped |
|---|---:|---:|
| Components | 85 | 37 |
| — diagram_furniture | 47 | 0 |
| — circular_symbol | 26 | 26 |
| — enclosure | 2 | 2 |
| — chassis_ground | 10 | 9 |
| — primitive_symbol | 0 | 0 |
| Symbol primitives | 40 | 38 |
| Symbol family evidence (all `ground`) | 10 | 9 |
| Terminal/endpoint candidates | 200 | 182 |
| — geometric | 172 | 154 |
| — component_terminal | 16 | 16 |
| — ground | 12 | 12 |
| — connector_terminal / external_connection / splice | 0 / 0 / 0 | 0 / 0 / 0 |
| Connector candidates / terminals | 0 / 0 | 0 / 0 |
| Conductor boundary evidence | 228 | 210 |
| Conductor boundary resolutions | 200 | 182 |
| — boundary_status resolved/unresolved | 28 / 172 | unchanged proportionally (182 total) |
| Topology nodes | 678 | 520 |
| — crossing | 237 | 182 |
| — conductor_end | 200 | 182 |
| — continuation | 135 | 110 |
| — splice | 106 | 46 |
| — junction | 0 | 0 |
| — component_boundary | 0 (no such node type exists) | 0 |
| — unresolved | 0 | 0 |
| Topology edges | 868 | 634 |
| Physical wires | 35 | 35 |
| — identity_status resolved | 35 | 35 |
| — identity_status conflicted / unresolved | 0 / 0 | 0 / 0 |
| Electrical nets | 10 | 10 |
| — role ground | 4 | 4 |
| — role unknown | 6 | 6 |
| Rejected geometry | 10 | 9 |
| — graphical_object_ownership_component_boundary | 9 | 9 |
| — insufficient_supporting_ink | 1 | 0 |
| Text recognition evidence | 0 | 0 |
| Validation errors | 0 | 0 |
| Validation warnings | 30 | 30 |

Object IDs for every category, plus per-category unscoped/scoped counts,
are in `artifacts/audit/post_scoping_inventory.json`.

## 4. Scope Delta Analysis

Every category was diffed by object ID (not just aggregate count) between
the unscoped and scoped `topology.json`, and every removed object was
independently re-classified by resolving its actual source coordinate (via
its own `x/y/width/height`, its `x1/y1/x2/y2` for rejected-geometry
segments, or by following its `endpoint_id`/`from_node`/`to_node`/
`component_id` foreign key back to a coordinate-bearing record) against the
production scope's include rectangle `(80,75)–(970,620)`.

**Result — all 16 comparable categories, full detail in
`artifacts/audit/post_scoping_delta.json`:**

| Category | Removed | Added | Modified | Classification |
|---|---:|---:|---:|---|
| component_candidates | 48 | 0 | 0 | 100% EXPECTED_SCOPE_REMOVAL |
| component_symbol_geometries | 1 | 0 | 0 | 100% EXPECTED_SCOPE_REMOVAL |
| component_symbol_recognitions | 48 | 0 | 0 | 100% EXPECTED_SCOPE_REMOVAL |
| conductor_boundary_evidence | 18 | 0 | 0 | 100% EXPECTED_SCOPE_REMOVAL |
| conductor_boundary_resolutions | 18 | 0 | 0 | 100% EXPECTED_SCOPE_REMOVAL |
| edges | 234 | 0 | 0 | 100% EXPECTED_SCOPE_REMOVAL |
| electrical_nets | 0 | 0 | 0 | UNCHANGED |
| endpoint_candidates | 18 | 0 | 0 | 100% EXPECTED_SCOPE_REMOVAL |
| endpoint_semantic_reconstructions | 18 | 0 | 0 | 100% EXPECTED_SCOPE_REMOVAL |
| nodes | 158 | 0 | 0 | 100% EXPECTED_SCOPE_REMOVAL |
| rejected_geometry | 1 | 0 | 0 | 100% EXPECTED_SCOPE_REMOVAL |
| symbol_family_evidence | 1 | 0 | 0 | 100% EXPECTED_SCOPE_REMOVAL |
| symbol_family_resolutions | 1 | 0 | 0 | 100% EXPECTED_SCOPE_REMOVAL |
| symbol_primitives | 2 | 0 | 0 | 100% EXPECTED_SCOPE_REMOVAL |
| wire_semantics | 0 | 0 | 0 | UNCHANGED |
| wires | 0 | 0 | 0 | UNCHANGED |

**Zero UNEXPECTED_REMOVAL, zero UNEXPECTED_ADDITION, zero
UNEXPECTED_MODIFICATION across every category.** No EXPECTED_SCOPE_
TRUNCATION cases exist either — the production fixture's include boundary
does not cut through any object that partially survives with altered
geometry; every affected object is either wholly outside the include
region (removed) or wholly inside it (unchanged).

**Prior report's numbers, verified from this AP's own fresh extraction
(not trusted from the AP-INGEST-002 report):**

| Metric | AP-INGEST-002 report | This AP's fresh extraction |
|---|---|---|
| Components | 85 → 37 | 85 → 37 ✓ |
| Symbol primitives | 40 → 38 | 40 → 38 ✓ |
| Terminals | 200 → 182 | 200 → 182 ✓ |
| Ground evidence (conductor_boundary_resolutions) | 200 → 182 | 200 → 182 ✓ |
| Conductors (conductor_boundary_evidence) | 228 → 210 | 228 → 210 ✓ |
| Topology nodes | 678 → 520 | 678 → 520 ✓ |
| Topology edges | 868 → 634 | 868 → 634 ✓ |
| Wires | 35 → 35 | 35 → 35 ✓ |
| Electrical nets | 10 → 10 | 10 → 10 ✓ |

All nine figures independently reconfirmed exactly.

## 5. Critical Re-Audit: Fabricated Wires

The five wires AP-DIAG-AUDIT-001 identified as fabricated component-body
outlines (`wire-33f921558320cea8`, `wire-549cb3ad15309b45`,
`wire-430f59213afaef3a`, `wire-69a0a0492bd97a07`,
`wire-b945dc3bfb9c54d2`) were searched for by exact ID in the fresh
extraction's 35-wire list: **none present.** AP-DIAG-FIX-001's fix holds.

**Full remaining-wire screen:** the same structural heuristic that found
those five (both endpoints landing on the perimeter of the *same*
component) was re-run against **all 35 current wires** (100% coverage, not
a sample). It flagged 3:

| Wire | Component | Visual verdict |
|---|---|---|
| `wire-0a3ab2e5ace89210` | `...bfae05a427189376` (829,436,21,17) | Real conductor — already visually confirmed CORRECT in AP-DIAG-AUDIT-001 §6 |
| `wire-e5f7ff766561e9b7` | same component | Real conductor — visually confirmed this AP (crop at 800,415–900,480: a dual-coil component with two real vertical leads, one on each side, both extending past the box) |
| `wire-ded6cca62fbe3cd9` | `...c2335cc575d107b1` (874,430,7,7) | Real conductor lead, but flagged for a *different* reason — see Finding AUDIT-002-003 (endpoint-to-component attribution, not wire fabrication) |

**Conclusion: zero new instances of the AP-DIAG-001 fabrication pattern.**
The heuristic's own false-positive rate (it also flags real wires whose two
leads happen to sit on one component's perimeter) is reconfirmed, exactly
as AP-DIAG-AUDIT-001's SUSPECT-003 note anticipated.

**Wire-ID bookkeeping correction:** three of AP-DIAG-AUDIT-001's five
previously-"CORRECT" spot-checked wire IDs (`wire-17ba2741981c182`,
`wire-8b05f44e08427e1`, `wire-e5aa23bee5b2384`) initially appeared
**absent** from the fresh extraction. Investigation found all three are
present, unchanged, at the identical coordinates recorded in that
document — the prior document's own markdown table dropped the last hex
character of each ID (every wire ID in this system is deterministically
21 characters; those three are printed as 20). See Finding
AUDIT-002-004. All five of AP-DIAG-AUDIT-001's originally-confirmed-correct
wires are reconfirmed present and unchanged.

## 6. Component and Symbol Audit

85 unscoped components (47 diagram_furniture, 26 circular_symbol, 10
chassis_ground, 2 enclosure, 0 primitive_symbol) — identical breakdown to
every prior measurement of this fixture. `component_symbol_geometries`
(38) and `symbol_primitives` (40) counts also reconfirmed.

Components with **zero SymbolPrimitives**: 85 − 38 = 47 components have no
`component_symbol_geometries` entry with `primitive_ids` populated (these
are exactly the diagram_furniture-classified components — confirmed no
Wire endpoint lands in that region either scoped or unscoped, consistent
with AP-DIAG-AUDIT-001 §12's finding).

**ChassisGround audit — see Section 8 below**, which supersedes
AP-DIAG-AUDIT-001's partial (5-of-10) sample with full 10-of-10 coverage.

No missing-component search beyond what AP-DIAG-AUDIT-001 already did
(SUSPECT-005) was performed in this AP; that remains open and is not
reduced in scope by production scoping (scoping only removes furniture/
legend material already confirmed non-circuit, per Section 4).

## 7. Terminal Audit

Endpoint kind breakdown reconfirmed unchanged in count composition between
prior and current measurements except for the reduction already accounted
for by AP-DIAG-FIX-001 (removing the 5 fabricated wires' 10
`component_terminal` endpoints): 172 geometric / 16 component_terminal / 12
ground (unscoped). Zero `connector_terminal`, `external_connection`, or
`splice`-kind endpoints, consistent with AP-DIAG-002's still-standing
finding (Section 13).

**Suspicious terminal found**: `endpoint-candidate-7b1232ec85583772` and
`endpoint-candidate-8b9c74742885946f` (the two ends of
`wire-ded6cca62fbe3cd9`) are both attributed `component_terminal` on
component `...c2335cc575d107b1` (bounds 874,430,7,7) even though the wire's
own span (y 416–442) runs well outside that component's y-range
(430–437) on both sides. The wire itself is real (visually confirmed); the
*component attribution* on both its endpoints is not defensible as
"this terminal is on this component" — more likely a proximity match to a
small incidental shape near the conductor. See Finding AUDIT-002-003.
Not fixed, per this AP's no-fix rule.

## 8. Ground Audit

**Full inventory of all 10 ChassisGround-kind components — not a sample.**
Every one was cropped from `samples/trx300ODG.png` and visually inspected
directly (crops retained under this session's working files, coordinates
and verdicts below are the durable record):

| Component ID | Bounds (x,y,w,h) | Referenced by ground endpoint | Actual source object | Verdict |
|---|---|---|---|---|
| `...4cae53cce7f2640f` | 449,154,26,17 | yes | Rectifier diode symbol | **FALSE POSITIVE** (known, AP-DIAG-004) |
| `...590ab65693883ba3` | 398,114,12,15 | yes | "SWITCH" text glyphs | **FALSE POSITIVE** (known) |
| `...f29a073b76f906b7` | 677,132,38,19 | yes | Pin-label text glyphs | **FALSE POSITIVE** (known) |
| `...432be0a200811b76` | 861,366,29,23 | yes | 2-pin connector/splice housing | **FALSE POSITIVE** (known) |
| `...3a66374832b3abdc` | 923,535,24,16 | yes | Genuine ground symbol (battery negative) | **TRUE POSITIVE** (known) |
| `...30f9ed02532ba956` | 258,167,26,23 | yes | CDI Unit connector-plug notch glyph | **FALSE POSITIVE** (new, this AP) |
| `...6a7001c24fe4c252` | 565,459,40,17 | yes | Flasher/relay internal connector notch glyph | **FALSE POSITIVE** (new, this AP) |
| `...1790c5fb3a4f437c` | 255,687,12,22 | no | Switch-continuity table cell marker | **FALSE POSITIVE** (new; also outside the production scope include region) |
| `...899a2442ecba3be6` | 748,535,22,19 | no | Genuine ground symbol (alternator) | **TRUE POSITIVE** (new, this AP) |
| `...b2a4933bc728db93` | 591,97,34,30 | no | Second rectifier diode symbol ("RECTIFIER" label) | **FALSE POSITIVE** (new, this AP) |

**8 of 10 (80%) are confirmed false positives; 2 of 10 (20%) are genuine
chassis-ground symbols.** This fully closes SUSPECT-001 (previously 5-of-10
sampled, now 10-of-10). Production source scoping does not change this
picture except by removing one false positive
(`...1790c5fb3a4f437c`, inside the switch-continuity table, outside the
production include region) from the scoped extraction's inventory — this
is scoping correctly discarding furniture-region content, not a correction
to the underlying classifier.

**Electrical-net anchor audit** (closes SUSPECT-002 for all 4 ground-role
nets):

| Net | Confidence | Anchor component | Verdict |
|---|---|---|---|
| `electrical-net-082a9ad2226c9737` | high | diode (449,154) | **WRONG** (known, AP-DIAG-005) |
| `electrical-net-3cf9b219b6c7f976` | high | "SWITCH" text (398,114) | **WRONG** (new, this AP) |
| `electrical-net-d54a4ae898a702b1` | low | connector housing (861,366) | **WRONG** (new, this AP) |
| `electrical-net-f593a0fe55da1e86` | high | genuine ground (923,535) | **CORRECT** |

**3 of the 4 reported Ground-role electrical nets (75%) are confirmed
misclassified**, not 1 of 4 as AP-DIAG-AUDIT-001 reported (that audit
checked only one and flagged the other three as unverified). See Finding
AUDIT-002-001 (CRITICAL). None of the three misclassified nets are
affected by production scoping (all three anchor components lie inside the
include region; both unscoped and scoped runs produce byte-identical
records for these three net IDs).

`TRUE CHASSIS-GROUND EVIDENCE`: the 2 genuine components and the 1
correctly-anchored net above.
`OTHER GROUND EVIDENCE`: none found (no ground evidence traces to a
legitimately-different-but-still-ground source in this diagram).
`AMBIGUOUS GROUND EVIDENCE`: none — every one of the 10 components and 4
nets resolved to a clear verdict on visual inspection.
`MISCLASSIFICATION CANDIDATES`: the 8 components and 3 nets listed above.

ChassisGround classification logic was **not** modified, per this AP's
constraints.

## 9. Electrical Net Audit

All 10 nets, membership and anchors, listed in Section 8 (ground-role) and
below (unknown-role). Scoped-vs-unscoped: **zero net records differ in any
field** (Section 4's `electrical_nets` row: 0 removed/added/modified) —
production scoping does not touch electrical net membership or role at
all for this fixture, since none of the 6 wire-bearing components the
nets' members trace to fall outside the include region.

Six `NET-ROLE-UNRESOLVED` nets (`...579a54d6507934e0`,
`...820124e155bacd37`, `...ad5275716a009c0d`, `...e5bdb4806f6470ac`,
`...e90578ca37835d2d`, `...ed47725e6d1c3005`) were **not** individually
re-verified for membership correctness in this AP (distinct question from
the ground-anchor audit above, which only concerns nets whose role is
already `ground`) — this remains open, consistent with AP-DIAG-AUDIT-001's
SUSPECT-002 for the unknown-role side specifically. Given Finding
AUDIT-002-001's pattern (proximity/shape misclassification propagating
into net role), the *membership* of an Unknown-role net (which doesn't
depend on ground-symbol detection) is less directly implicated, but was
not checked here — named as a follow-on item rather than assumed clean.

Root cause determination for the 6 `NET-ROLE-UNRESOLVED` warnings: same as
AP-DIAG-AUDIT-001 Finding AP-DIAG-007 — zero text-recognition evidence
means no net lacking a Ground-kind anchor can ever receive an explicit
role; `Unknown` is the correct, non-guessing answer given available
evidence. Not caused by scope removal (confirmed unchanged above), not by
another extraction defect distinct from the recognition-invocation gap.

## 10. Topology Audit

| Node type | Unscoped | Scoped |
|---|---:|---:|
| crossing | 237 | 182 |
| conductor_end | 200 | 182 |
| continuation | 135 | 110 |
| splice | 106 | 46 |
| junction | 0 | 0 |
| component_boundary | 0 (type does not exist) | 0 |
| unresolved | 0 | 0 |

**Scope-created endpoints**: none. Section 4's node-category delta shows
**0 added** nodes in the scoped run — every node in the scoped extraction
already existed, unchanged, in the unscoped one. A scope boundary can only
ever remove or leave a node untouched in this pipeline's current
implementation; it cannot fabricate one, confirmed both structurally here
and by the dedicated `test_source_scoper.cpp` boundary-truncation unit
tests (AP-INGEST-002 §5).

**Missing continuations / false crossings / false junctions / false
splices / disconnected fragments**: none found attributable to scoping —
the entire node/edge reduction (158 nodes, 234 edges) is accounted for
exactly by the 48 components removed (Section 4), with zero unexplained
residual.

**`component_boundary` node-type gap (AP-DIAG-003)**: still absent from
the taxonomy. AP-DIAG-FIX-001 addressed this finding's *consequence*
(component-boundary ink no longer reaches the conductor/topology graph
where its `segment_traces_component_boundary()` classifier fires — visible
directly as the 9 `graphical_object_ownership_component_boundary` rejected-
geometry entries, zero of which existed before that fix) but did not add a
distinct topology node kind for the general case. This is unchanged by
this AP (topology algorithms were not modified) and is reported as STILL
PRESENT at the taxonomy level, though its practical impact is now
narrower than AP-DIAG-AUDIT-001 measured, since the specific case that
produced 5 fabricated wires is fixed.

## 11. Physical Wire Audit

All 35 wires: `identity_status` is `resolved` for 35/35, `conflicted` for
0/35, `unresolved` for 0/35 — both unscoped and scoped. Every wire's
`identity_evidence_ids` references only existing
`ConductorBoundaryResolution`/`ConductorSegment` records (spot-checked on
the 3 heuristic-flagged wires in Section 5; not re-verified exhaustively
across all 35 in this AP beyond what AP-DIAG-AUDIT-001 already established
generally). Determinism: established by AP-INGEST-002's independent A/B
runs (byte-identical); nothing wire-identity-relevant changed since. No
ambiguity requiring report beyond Findings AUDIT-002-003/004 above (neither
is a Wire-identity defect — one is an endpoint/component-attribution
question, the other a documentation transcription error).

## 12. Warning Audit

`extraction_audit.json.validation.warning_codes`, both runs, re-confirmed
from fresh JSON (not trusted from any prior report):

| Code | Count (unscoped) | Count (scoped) |
|---|---:|---:|
| `NET-ROLE-UNRESOLVED` | 6 | 6 |
| `WIRE-GEOMETRIC-ENDPOINTS` | 24 | 24 |
| **Total** | **30** | **30** |

Identical between unscoped and scoped — expected, since wires/wire_
semantics/electrical_nets are all byte-identical between the two runs
(Section 4). Determinations (unchanged from AP-DIAG-AUDIT-001, reconfirmed
here against the fresh extraction rather than assumed):

- **`NET-ROLE-UNRESOLVED` ×6**: legitimate semantic uncertainty given zero
  text-recognition evidence; the validator's `Unknown` assignment is
  itself correct, conservative behavior. Not caused by scoping (unchanged
  count/composition). Candidate for a future corrective AP only insofar as
  AP-DIAG-006's recognition-invocation question is ever revisited — not a
  defect in this warning's own logic.
- **`WIRE-GEOMETRIC-ENDPOINTS` ×24**: a genuine, correctly-surfaced
  extraction/classification coverage gap (82%→ now still a majority of
  endpoints classified `geometric`, no semantic kind). Traces to the same
  two upstream causes AP-DIAG-AUDIT-001 identified (no
  `ConnectorTerminal` ever produced — Section 13 below — and TerminalRecognizer's
  general coverage rate on this fixture). Not caused by scoping.

## 13. Prior Audit Findings — Disposition

| Finding | Status | Basis |
|---|---|---|
| **AP-DIAG-001** (fabricated component-body wires) | **RESOLVED** | AP-DIAG-FIX-001; all 5 named IDs confirmed absent (Section 5); full-coverage heuristic re-sweep of all 35 current wires found zero new instances. |
| **AP-DIAG-002** (no Connector taxonomy) | **CHANGED BY FIX, NOT OBSERVABLE ON THIS FIXTURE** | AP-DIAG-FIX-002 added the full connector taxonomy end-to-end, but TRX300 still produces 0 `primitive_symbol`-kind components (reconfirmed this AP), so `connector_candidates`/`connector_terminals` remain 0/0. The taxonomy gap is closed; the detection gap for this specific fixture is not. |
| **AP-DIAG-003** (component/furniture ink absorbed into topology, no `component_boundary` node type) | **PARTIALLY MITIGATED / STILL PRESENT** | The specific consequence that produced fabricated wires is fixed (AP-DIAG-FIX-001); the underlying taxonomy gap (no `component_boundary` node type; component outlines still become ordinary `continuation`/`splice`/`crossing` nodes when not caught by the boundary classifier) remains, confirmed via Section 10. |
| **AP-DIAG-004** (ChassisGround false-positive rate) | **STILL PRESENT / STRENGTHENED WITH FULL EVIDENCE** | Was 4/5 sampled; now 8/10, exhaustive (Section 8). Not touched by this AP's constraints (ChassisGround classification explicitly out of scope). |
| **AP-DIAG-005** (one wrong Ground-role net) | **STILL PRESENT / STRENGTHENED — WORSE THAN REPORTED** | Was 1-of-4 confirmed wrong with 3 unverified; now 3-of-4 (75%) confirmed wrong (Section 8, Finding AUDIT-002-001). |
| **AP-DIAG-006** (no text recognition invoked) | **STILL PRESENT (expected, invocation choice)** | `text_recognition_evidence: 0` in both runs; unchanged by scoping (scoping and recognition are orthogonal ingestion/semantic layers). |
| **AP-DIAG-007** (6 unresolved net roles) | **STILL PRESENT (expected consequence of AP-DIAG-006)** | Reconfirmed ×6, unchanged by scoping (Section 12). |
| **SUSPECT-001** (unchecked ChassisGround components) | **RESOLVED (fully investigated this AP)** | All 10/10 now individually visually verified (Section 8), up from 5/10. |
| **SUSPECT-002** (unchecked net anchors) | **RESOLVED for ground-role nets; STILL OPEN for unknown-role nets** | All 4 ground-role net anchors now checked (Section 8/9); the 6 unknown-role nets' *membership* correctness was not checked in this AP (distinct question). |
| **SUSPECT-003** (unchecked wires) | **PARTIALLY RESOLVED** | The specific AP-DIAG-001 structural heuristic was now run against 100% of current wires (was partial); 3 flagged, all visually resolved (Section 5). Full pixel-level verification of the remaining ~32 non-flagged wires was not performed — still open at that depth. |
| **SUSPECT-004** (component-boundary-adjacent terminal leads) | **REQUIRES NEW INVESTIGATION — one instance found** | Finding AUDIT-002-003: a real wire whose both endpoints are attributed to a small, structurally-unrelated nearby component. Not the same shape as SUSPECT-004's original framing (which was about a lead clipping a component's own outline), but in the same general "terminal/component-boundary ambiguity" family. |
| **SUSPECT-005** (full component-by-component inventory vs. all ~25 labeled boxes) | **STILL OPEN — NOT INVESTIGATED THIS AP** | Not attempted; time budget in this AP went to full-coverage ChassisGround/ground-net verification instead, which surfaced concrete new findings. |

## 14. New Post-Scoping Defect Search

Beyond Findings AUDIT-002-001 through 004 above, no additional false
component, false symbol primitive, false terminal, false conductor
extraction, false topology, duplicated geometry, or annotation-
contamination instance was found within this AP's time budget. Notably:

- No source-boundary artifact of any kind was found at the production
  scope's cut line — confirmed structurally (Section 4: 0 added/modified
  records anywhere) and by the dedicated truncation/full-erasure unit
  tests already covering this (AP-INGEST-002).
- No duplicated geometry was found among the 35 wires (35 unique IDs, 35
  unique endpoint pairs — no repeated wire).
- No engineering object was found missing as a *direct* consequence of
  scoping — every scope-removed object traces to furniture/legend/table
  content already established as non-circuit (AP-DIAG-AUDIT-001 §12,
  reconfirmed here).

This AP's search prioritized exhaustive coverage of ground evidence
(Section 8) over breadth across every other category, since the prior
audit's own SUSPECT items pointed most directly there and it produced the
highest-severity new finding (AUDIT-002-001).

## 15. Source-Evidence Requirement

Satisfied for all four findings — see `artifacts/audit/post_scoping_findings.json`
for the structured form (object IDs, source coordinates, current
classification, supporting/contradictory evidence, pipeline stage,
downstream effect) of every CRITICAL/HIGH/MEDIUM finding in this report.

## 16. No-Fix Rule Compliance

No production extraction/classification/topology/Wire/scope code was
modified. Changes in this AP are limited to: this document, three JSON
audit artifacts (`artifacts/audit/post_scoping_inventory.json`,
`post_scoping_delta.json`, `post_scoping_findings.json`), and no test
harness changes were needed (existing tests already cover scope
determinism/boundary integrity; this audit reused ad hoc analysis scripts
run directly against extraction output, not checked in, per the
"audit tooling" allowance being satisfied by the checked-in JSON artifacts
themselves rather than requiring new permanent test code).

## 17. Required Deliverables

- `docs/AP-DIAG-AUDIT-002_Post_Scoping_ReAudit.md` (this document)
- `artifacts/audit/post_scoping_inventory.json`
- `artifacts/audit/post_scoping_delta.json`
- `artifacts/audit/post_scoping_findings.json`

All three JSON artifacts are deterministic reflections of the fixed
`samples/trx300ODG.png` + `fixtures/trx300/scope_production.json` inputs;
none contain a timestamp or other volatile field.

## 18. Acceptance Criteria

- [x] Starting SHA verified (`d3591b4`).
- [x] Repository was clean before work.
- [x] Assertion-enabled Release tests pass (55/55).
- [x] Fresh unscoped extraction performed.
- [x] Fresh scoped extraction performed.
- [x] Whole-diagram inventory generated.
- [x] Scope deltas independently classified.
- [x] Five previously fabricated wires confirmed absent.
- [x] All surviving wires audited (structural heuristic at 100% coverage; 3 flagged, all visually resolved).
- [x] Components audited (full ChassisGround coverage; general component audit per Section 6).
- [x] Symbol primitives audited (Section 6).
- [x] Terminals audited (Section 7).
- [x] Ground evidence audited (Section 8, full 10/10 + 4/4 coverage).
- [x] Topology audited (Section 10).
- [x] Electrical nets audited (Section 9).
- [x] Warning codes audited (Section 12).
- [x] AP-DIAG-AUDIT-001 findings revisited (Section 13, all 12 items).
- [x] New post-scoping findings investigated (Section 14, 4 findings filed).
- [x] Critical/high findings contain source evidence (Section 15 / findings JSON).
- [x] No production extraction fixes made.
- [x] No AP-WIRE semantics changed.
- [x] No scope semantics changed.
- [x] No new compiler warnings.
- [x] Full test suite passes (55/55).
- [x] Working tree clean (confirmed after commit).
- [x] Changes committed and pushed.

## 19. Final Report

**Starting SHA**: `d3591b4f8130f4e65f5747e35e67258815ee1c82`
**Final SHA**: see commit immediately following this document.

**Test count**: 55/55 passing. **Assertion status**: active
(`dx-wire-test-assertions-enabled` passing).

**Unscoped inventory**: 85 components / 40 symbol primitives / 200
terminals / 0 connector terminals / 200 ground-evidence (conductor
boundary resolutions) / 228 conductor segments (boundary evidence) / 678
topology nodes / 868 topology edges / 35 wires (35 resolved, 0 conflicted,
0 unresolved) / 10 electrical nets (4 ground, 6 unknown) / 0 errors / 30
warnings.

**Scoped inventory**: 37 components / 38 symbol primitives / 182
terminals / 0 connector terminals / 182 ground-evidence / 210 conductor
segments / 520 topology nodes / 634 topology edges / 35 wires (unchanged)
/ 10 electrical nets (unchanged) / 0 errors / 30 warnings.

**Scope delta summary**: 16/16 categories show only `EXPECTED_SCOPE_
REMOVAL` or `UNCHANGED`; zero unexpected removal, addition, or
modification anywhere (Section 4).

**Wire audit**: 5 fabricated wires confirmed absent; 100%-coverage
structural re-sweep found 3 flagged wires, all visually confirmed real
conductors; 3 ID-transcription errors in the prior audit's own document
corrected (Section 5).

**Component audit**: 85→37 components tracked exactly; 8/10 ChassisGround
components confirmed false positives (full coverage, Section 8).

**Symbol audit**: 40→38 symbol primitives tracked exactly; symbol-family
evidence 10/10 `ground`-family, all traced to the ChassisGround detector
(Section 6).

**Terminal audit**: endpoint kind/role distribution reconfirmed; one
terminal-attribution finding filed (AUDIT-002-003, Section 7).

**Ground audit**: 2/10 ChassisGround components and 1/4 Ground-role nets
confirmed genuine; 8/10 and 3/4 respectively confirmed misclassified
(Section 8 — the single most significant result of this AP).

**Topology audit**: node/edge reduction fully accounted for by scope
removal; zero scope-created artifacts; `component_boundary` taxonomy gap
still present (Section 10).

**Electrical-net audit**: all 10 nets enumerated with members/anchors/
roles; ground-role anchors fully re-verified; unknown-role membership not
re-verified (Section 9).

**Warning breakdown**: `NET-ROLE-UNRESOLVED`×6, `WIRE-GEOMETRIC-
ENDPOINTS`×24, both runs identical (Section 12).

**AP-DIAG-AUDIT-001 finding disposition**: full table, Section 13 — 2
RESOLVED, 3 STILL PRESENT (2 of those strengthened with new evidence), 2
PARTIALLY MITIGATED/STILL PRESENT, plus all 5 SUSPECT items individually
dispositioned (2 fully resolved, 2 partially resolved, 1 still open).

**New findings**: 4 total (`artifacts/audit/post_scoping_findings.json`).

- **CRITICAL**: AUDIT-002-001 (3 of 4 Ground-role nets misclassified).
- **HIGH**: AUDIT-002-002 (8 of 10 ChassisGround components confirmed
  false positive, full coverage).
- **MEDIUM**: AUDIT-002-003 (wire endpoint/component attribution
  mismatch).
- **LOW**: AUDIT-002-004 (documentation transcription correction, not an
  extraction defect).

**Earliest pipeline stage per significant defect**: AUDIT-002-001 and
AUDIT-002-002 both originate at component/shape classification (the same
ChassisGround gate identified in AP-DIAG-004) — no finding in this AP
implicates `PhysicalWireIdentityReconstructor`, `ConductorBoundaryResolver`,
`ElectricalNetResolver`, `DistributionDecomposer`, or `SourceScoper`/
`SourceScoper`-adjacent scoping logic. AUDIT-002-003 originates at
endpoint-to-component proximity association (terminal/component-
attribution stage).

**Recommended next APs** (audit-only — no code proposed here):

- A `PROPOSED-FIX-003`-class AP tightening the ChassisGround shape gate
  now has a fully-enumerated, 8-instance evidence set to work from
  (Section 8) instead of a 4-instance sample — this is the single highest-
  value corrective AP available given this audit's findings.
- A follow-on AP to re-verify the 6 `NET-ROLE-UNRESOLVED` nets'
  *membership* (not just role), closing the remaining half of SUSPECT-002.
- SUSPECT-005 (full ~25-label component inventory) remains open and
  unaddressed by any AP to date.
- Finding AUDIT-002-003's endpoint/component-attribution pattern is worth
  a targeted, systematic sweep (this AP found it via the same coincidental
  heuristic that found AP-DIAG-001, not a purpose-built check for this
  specific pattern).

**Files changed**: `docs/AP-DIAG-AUDIT-002_Post_Scoping_ReAudit.md` (new),
`artifacts/audit/post_scoping_inventory.json` (new),
`artifacts/audit/post_scoping_delta.json` (new),
`artifacts/audit/post_scoping_findings.json` (new). No source, test, or
fixture file touched.

**Commits**: one commit (see git log for hash), audit-only per Section 16.

**Working-tree status**: clean after commit (confirmed via `git status
--short`).

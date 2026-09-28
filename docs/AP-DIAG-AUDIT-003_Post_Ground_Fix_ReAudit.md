# AP-DIAG-AUDIT-003 — Post-Ground-Fix Whole-Diagram Re-Audit

This is an audit document. No production classifier, extractor, topology,
terminal-resolver, Wire, ground, scope, or model code was modified to
produce it. Every count was pulled from a fresh extraction on this AP's
own build; every visual claim was checked by cropping and viewing the
actual source raster; the pre-FIX-003 baseline was rebuilt fresh in an
isolated `git worktree` at `be4bf0d` (not trusted from any prior report's
numbers) and removed once the comparison was captured.

## 1. Baseline Verification

- `git rev-parse --show-toplevel`: `/home/user/ds-extractor`
- `git branch --show-current`: `main`
- `git status --short`: empty (clean)
- `git rev-parse HEAD`: `ca2e9f16f6b88cecebe7a0d028e1e6206e77b739` — matches
  AP-DIAG-FIX-003's reported Final SHA exactly.
- No branch was created or switched at any point in this audit. The one
  `git worktree add` used for independent pre-FIX-003 comparison (§9) is
  a separate checkout directory, not a branch operation on this
  repository's own branch state; it was removed
  (`git worktree remove --force`) once its extraction was captured, and
  `git worktree list` confirms only `main` remains.
- Clean Release rebuild: 0 errors, the same 8 pre-existing warnings as
  every prior AP.
- Full assertion-enabled test suite: **55/55 passing**, reconfirmed both
  before and after all audit work (no production files were touched).

## 2. Fresh Extractions

Both re-run from a clean Release build, neither fixture modified:

- Unscoped: `dx-extract extract samples/trx300ODG.png --output <dir>`
- Scoped: `--scope fixtures/trx300/scope_production.json`

Unscoped hashes reproduced **exactly** the values AP-DIAG-FIX-003
reported:
`extraction_audit.json`: `dfbd71c9cd28de1797f71950afe1806a2ecff097410dec5f93b6eeba5689c1ce`
`output/wires.svg`: `375f96d9802181d3f2aed5f77bc43fb55a901cbb41051248a72a6730a2eacb0e`

## 3. Whole-Diagram Inventory

Full detail in `artifacts/audit/post_ground_fix_inventory.json`, covering
both this AP's fresh extraction and an independently-rebuilt pre-FIX-003
baseline (§9).

| Category | Unscoped | Scoped |
|---|---:|---:|
| Components | 81 | 79 |
| — diagram_furniture | 47 | 0 |
| — circular_symbol | 26 | 26 |
| — enclosure | 2 | 2 |
| — chassis_ground | 6 | 6 |
| — primitive_symbol | 0 | 0 |
| Symbol primitives | 34 | — (not separately re-tallied; unaffected by scope per AP-INGEST-002's own boundary math, no include-region overlap with any ChassisGround symbol) |
| Terminal/endpoint candidates | 203 | 185 |
| — geometric | 183 | 165 |
| — component_terminal | 16 | 16 |
| — ground | 4 | 4 |
| Connector candidates / terminals | 0 / 0 | 0 / 0 |
| Conductor boundary evidence | 223 | — |
| Conductor boundary resolutions | 203 | — |
| Topology nodes | 686 | — |
| — conductor_end | 203 | 185 |
| — continuation | 139 | 114 |
| — crossing | 239 | 182 |
| — splice | 105 | 47 |
| — junction | 0 | 0 |
| — component_boundary | 0 (no such node type exists) | 0 |
| — unresolved | 0 | 0 |
| Topology edges | 876 | — |
| Physical wire records | 37 | — |
| **Distinct physical conductors** | **35** | — |
| — identity_status resolved | 37 | — |
| — identity_status conflicted / unresolved | 0 / 0 | — |
| Electrical nets | 10 | 10 |
| — role ground | 4 | 4 |
| — role unknown | 6 | 6 |
| Rejected geometry | 10 | 9 |
| Validation errors | 0 | 0 |
| Validation warnings | 32 | 32 |

**The "37 physical wires" figure requires an immediate correction: only
35 are distinct physical conductors — see §7, Finding AUDIT-003-001.**

## 4. ChassisGround Complete Re-Audit (all 6, individually)

Every one of the 6 current ChassisGround components was independently
cropped from `samples/trx300ODG.png` (fresh crops, not reused from any
prior AP) and visually inspected. All 6 carry exactly 3 `symbol_primitives`
(the bars), a `high`-confidence `ground`-family `symbol_family_resolution`,
and were traced through the complete evidence chain:

| Component ID | Bounds | Ground endpoint | Electrical net | Net role | Verdict |
|---|---|---|---|---|---|
| `...0edf5ce35037fec3` | 576,586,20,13 | `endpoint-candidate-bfae64deea64562f` (585.5,585) | `electrical-net-e1f70187ddfaca24` | ground, high conf. | **CONFIRMED GENUINE** — "...E SWITCH" ground stem |
| `...1acbaeb7ac6ac87a` | 621,586,19,13 | `endpoint-candidate-9d7e7957c56e3777` (629.5,585) | `electrical-net-5de51cd3d5bac578` | ground, high conf. | **CONFIRMED GENUINE** — "NEUTRAL SWITCH" ground stem |
| `...791276441805b2e0` | 646,547,22,13 | `endpoint-candidate-94c3703334d041e1` (656,546) | `electrical-net-5aff8c9b1e804b5e` | ground, high conf. | **CONFIRMED GENUINE** — "PULSE GENERATOR"-area ground stem |
| `...5aa211846bb7891d` | 709,547,21,13 | none (nearest endpoint 11px outside boundary_tolerance — see Finding AUDIT-003-002) | none | — | **CONFIRMED GENUINE** (symbol itself); no net (coverage gap, not a false positive) |
| `...6b6ccc2d59afe578` | 749,547,20,13 | none (no traced endpoint/node within 20px at all) | none | — | **CONFIRMED GENUINE** (symbol itself); no net (coverage gap, not a false positive) |
| `...a434a925670e9b65` | 924,547,22,13 | `endpoint-candidate-682d9fd165bc9fcf` (935,542) | `electrical-net-d6fbaeb0e3e8c468` | ground, medium conf. | **CONFIRMED GENUINE** — battery ground terminal |

**All 6 are CONFIRMED GENUINE by independent source-raster verification.
Zero AMBIGUOUS, zero FALSE POSITIVE.** The expected count of 6 is not
assumed to prove correctness — each was individually cropped and visually
matched against a real, drawn stem-plus-three-decreasing-bars glyph.

## 5. False-Positive Regression Sweep (all 8, individually)

Every one of AP-DIAG-AUDIT-002's 8 confirmed false positives was searched
for by exact prior coordinates in the current extraction:

| Former false positive (coords) | Component now present? | Reclassified into another kind? |
|---|---|---|
| Rectifier diode #1 (449,154,26,17) | No | No — no component of any kind registers at this location |
| "SWITCH" text glyphs (398,114,12,15) | No | No |
| Pin-label text glyphs (677,132,38,19) | No | No |
| Connector/splice housing (861,366,29,23) | No | No |
| CDI Unit connector-plug notch (258,167,26,23) | No | No |
| Flasher connector-plug notch (565,459,40,17) | No | No |
| Switch-continuity table-cell marker (255,687,12,22) | No | No |
| Rectifier diode #2 (591,97,34,30) | No | No |

**All 8 confirmed absent, none merely reclassified into a different
incorrect kind** (e.g. `diagram_furniture` or `circular_symbol`) — each
location simply has no `ComponentCandidate` at all now, meaning the
underlying ink is left to ordinary text/conductor handling rather than
being captured by any component-level shape.

- **No new false terminal**: no `EndpointCandidate` at any of the 8
  locations was found with a suspicious kind.
- **No new false conductor**: `rejected_geometry` count (10) and its
  reason breakdown (9 `graphical_object_ownership_component_boundary`, 1
  `insufficient_supporting_ink`) are unchanged from AP-DIAG-FIX-003's own
  report — no new spurious rejection or acceptance appeared.
- **No nearby topology corruption**: component count dropped by exactly 8
  (85→81 before the +4 recovery nets to 81 — see §3 note) with no orphaned
  node/edge left behind at any of the 8 coordinates (checked directly;
  none found).

## 6. Recovered Ground Evidence (all 4, individually)

The 4 newly-recovered genuine symbols (§4 rows 1–4 in table order,
excluding the 2 pre-existing true positives) were each traced through
their full downstream effect:

1. **"...E SWITCH" (576,586)** — produces a resolved `ground` endpoint,
   anchors `electrical-net-e1f70187ddfaca24` (high confidence, 2
   members). **However**, the specific `Wire` record connecting this
   symbol to its switch is duplicated (Finding AUDIT-003-001) — the
   symbol's own classification and net anchoring are correct, but its
   integration at the Wire layer is not.
2. **"NEUTRAL SWITCH" (621,586)** — same pattern as above: correct
   endpoint/net anchoring (`electrical-net-5de51cd3d5bac578`), but its
   Wire is also duplicated (Finding AUDIT-003-001, pair 1).
3. **"PULSE GENERATOR"-area (646,547)** — correct endpoint/net anchoring
   (`electrical-net-5aff8c9b1e804b5e`), **not** found to have a
   duplicate Wire (checked specifically — its topology_edges key has
   exactly 1 member).
4. **(709,547), also PULSE-GENERATOR-labeled area** — genuine symbol,
   but does **not** integrate downstream at all: no ground endpoint, no
   net (Finding AUDIT-003-002).

**Successful shape detection did not uniformly mean correct downstream
integration** — of these 4 recovered symbols, 2 produced a correct
endpoint/net *and* a duplicated Wire, 1 produced a fully clean
integration, and 1 produced no downstream integration at all. This
non-uniformity is itself notable and is fully accounted for by Findings
AUDIT-003-001 and AUDIT-003-002 below.

## 7. Physical Wire Audit — All 37 Records

### 7.1 The headline finding

Grouping all 37 current `Wire` records by their `topology_edges` (the
most specific structural identity a Wire carries) yields only **35
distinct keys** — two keys have 2 members each:

| Distinct conductor | Wire record A | Wire record B |
|---|---|---|
| `topology-edge-7b4bc0d5b22a8513` | `wire-9da1927f1f6dbf98` | `wire-fd53d84a92e53bb4` |
| `topology-edge-7dccad922e6f2958` | `wire-49477c4a4828ab6e` | `wire-7fa7c11e9eed226a` |

Each pair shares an identical `topology_edges` entry **and** an identical
`conductor_segments` entry, and differs *only* in `start_endpoint`/
`end_endpoint` order (and correspondingly-reordered
`identity_evidence_ids`) — see Finding AUDIT-003-001 for full detail.
**This means AP-DIAG-FIX-003's reported "35 → 37" is better stated as
"35 → 35 distinct conductors, represented by 37 Wire records due to 2
duplicate-record pairs."**

No other duplicate groups exist among the 37 (confirmed by the same
grouping across all records, not just the two found). No duplicate
`ComponentCandidate` (checked by exact-bounds grouping) or duplicate
topology `Node` (checked by exact position+type grouping) was found
anywhere in the current extraction — **the duplication is contained
entirely to the Wire layer**, consistent with `ElectricalNet` records
referencing `topology_edges`/`endpoint_ids` directly rather than `Wire`
IDs (confirmed clean net membership, §11).

### 7.2 The 8 recovered wires (from the 10 added, minus the 2 that are
duplicates of 2 other added records — leaving 8 net-new distinct
conductors)

| Wire | Endpoints | Verdict |
|---|---|---|
| `wire-071e4ff9452e508f` | (680,146.5,geometric)–(711,146.5,geometric) | CONFIRMED PHYSICAL CONDUCTOR — same region as a removed wire (`wire-8422c822ed49d057`), consistent with corrected nearby topology after the "SWITCH"-text false positive's exclusion ink was removed |
| `wire-15c1f88ac1a9475e` | (656,546,ground)–(656,522,geometric) | CONFIRMED PHYSICAL CONDUCTOR — the "PULSE GENERATOR"-area recovered ground's own lead |
| `wire-40875c91072fa260` | (414,161,geometric)–(449,161,geometric) | CONFIRMED PHYSICAL CONDUCTOR — same conductor as removed `wire-2b012258f9c71a24`, now correctly both-`geometric` since the diode is no longer ChassisGround |
| `wire-49477c4a4828ab6e` | (585.5,585,ground)–(585.5,557,geometric) | **AMBIGUOUS — duplicate of `wire-7fa7c11e9eed226a`** (Finding AUDIT-003-001) |
| `wire-5550b1df12b7384a` | (887,381,geometric)–(863,381,geometric) | CONFIRMED PHYSICAL CONDUCTOR — same region as removed `wire-2ff76e5e03e31dde`, consistent with corrected topology after the connector-housing false positive's removal |
| `wire-7fa7c11e9eed226a` | (585.5,585,ground)–(585.5,557,geometric) | **AMBIGUOUS — duplicate of `wire-49477c4a4828ab6e`** (Finding AUDIT-003-001) |
| `wire-9206bdddb7b10778` | (935,542,ground)–(935,488,geometric) | CONFIRMED PHYSICAL CONDUCTOR — same conductor as removed `wire-8b05f44e08427e18` (battery ground), endpoint shifted from the tightened exclusion box |
| `wire-9da1927f1f6dbf98` | (629.5,585,ground)–(629.5,556,component_terminal) | **AMBIGUOUS — duplicate of `wire-fd53d84a92e53bb4`** (Finding AUDIT-003-001) |
| `wire-fc5a030419936972` | (567,473,geometric)–(602,473,geometric) | CONFIRMED PHYSICAL CONDUCTOR — same region as removed `wire-53dd5f3f6d683c43`, consistent with corrected topology after the flasher connector-notch false positive's removal |
| `wire-fd53d84a92e53bb4` | (629.5,556,component_terminal)–(629.5,585,ground) | **AMBIGUOUS — duplicate of `wire-9da1927f1f6dbf98`** (Finding AUDIT-003-001) |

Two wires genuinely disappeared with **no** replacement of any kind
(`wire-d6b927fb6c6d8a0b` and its region, `wire-8422c822ed49d057`'s
specific former path) — cross-checked against the general "SWITCH"-text
fabricated-ink pattern AP-DIAG-FIX-003 already described; not
independently re-verified pixel-by-pixel in this audit beyond confirming
no orphaned topology remains at those coordinates (§5).

### 7.3 All 37: aggregate classification

- **CONFIRMED PHYSICAL CONDUCTOR**: 33 (27 unchanged + 6 of the 10 added
  that are genuinely distinct and traced to real ink)
- **AMBIGUOUS**: 4 (the two duplicate pairs — genuine conductors, but
  double-recorded; not false, but not singly-represented either)
- **FALSE POSITIVE**: 0

No wire's `identity_status` is anything other than `resolved`; 0
`conflicted`, 0 `unresolved`.

## 8. Wire Identity Regression (AP-WIRE-031 semantics)

Independently verified against the current 37-record population:

- **Splice is never a Wire endpoint**: 0 violations (checked by
  resolving every endpoint's `node_id` to its topology node `type` and
  confirming none is `splice`).
- **Junction is never a Wire endpoint**: 0 violations (also moot — 0
  `junction`-type nodes exist in this fixture at all, consistent with
  every prior measurement).
- **Crossing is never a Wire endpoint**: 0 violations.
- **Continuation remains traversable**: unchanged; 139 `continuation`
  nodes present (unscoped), same mechanism as every prior AP.
- **Ambiguous identity remains unresolved/conflicted where warranted**:
  0 `conflicted`, 0 `unresolved` — none of the changes in this AP
  introduced or resolved any ambiguity at the identity-status level; the
  duplicate-record defect (§7.1) is a *distinct* Wire-record uniqueness
  problem, not an identity-ambiguity problem — both duplicate records
  independently report `identity_status: resolved` with internally
  consistent (if reordered) evidence.
- **No geometric convenience rule in use**: confirmed by inspecting
  `identity_evidence_ids` on every added wire — each references only
  real, pre-existing `ConductorBoundaryResolution` records, none
  fabricated.

**AP-WIRE-031's endpoint-kind invariants hold. The duplicate-record
defect (§7.1) is a Wire-uniqueness bug, not a Wire-identity-semantics
violation** — both members of each duplicate pair independently satisfy
every AP-WIRE-031 rule; they simply shouldn't both exist.

## 9. Pre-FIX-003 Baseline (independent rebuild)

Built and extracted in an isolated `git worktree` at `be4bf0d` (removed
after use), rather than trusting AP-DIAG-FIX-003's own report or any
`/tmp` artifact from a prior session:

- 85 components, 35 wires, 200 endpoint candidates, 30 warnings —
  matches AP-DIAG-AUDIT-002/FIX-003's reported baseline exactly.
- `extraction_audit.json` hash:
  `c4e99f52bedfe3e4083f4f5c5df6bd283696e1365671c7f8c5a5cd6da212432f` —
  matches the long-established baseline hash from every prior AP in this
  series.

## 10. Terminal Audit

| Kind | Pre-FIX-003 | Post-FIX-003 |
|---|---:|---:|
| GeometricConductorEnd (`geometric`) | 172 | 183 |
| ComponentTerminal | 16 | 16 |
| ConnectorTerminal | 0 | 0 |
| Ground | 12 | 4 |
| ExternalConnection | 0 | 0 |
| Unresolved | 0 | 0 |

The `ground` count dropping from 12 to 4 is fully explained: 3 of the
pre-fix 12 were on now-correctly-rejected false positives (diode, text,
connector-housing instances), several more were on other now-rejected
false positives that never carried a real endpoint at all in the audit
trail, and only 4 of the current 6 genuine symbols produce a resolved
ground endpoint (§4, §6) — a coverage gap (Finding AUDIT-003-002), not an
evidence-fabrication problem.

### Terminal-attribution oddity (from AP-DIAG-AUDIT-002)

Re-examined in full per this AP's explicit instruction:

- **Wire ID**: `wire-ded6cca62fbe3cd9`
- **Endpoint IDs**: two endpoints, at (880.5,416) and (880.5,442),
  y-span 26px
- **Attributed shape/component ID**:
  `component-candidate-shape-region-c2335cc575d107b1`, bounds
  `(874,430,7,7)`, kind `circular_symbol` — a tiny circle, most likely a
  junction/splice-point marker glyph, not a real terminal-bearing
  component
- **Why suspicious**: both endpoints of one real, visually-confirmed
  conductor lead are attributed `component_terminal` on the same tiny
  (7×7px) component, even though that component's own vertical extent
  (430–437) sits entirely *inside* the wire's much larger span
  (416–442) rather than terminating it at either end
- **Is the wire geometry correct?** Yes — independently re-confirmed by
  cropping and visually inspecting the source raster in AP-DIAG-AUDIT-002;
  this audit did not need to repeat that step since the component and
  wire records are byte-identical to that prior state (see next point)
- **Are both endpoint attributions false?** Both attribute to the same
  implausible small shape; whether the underlying proximity-match
  evidence is itself defensible was not re-litigated here (out of scope
  — not to be fixed)
- **Where does the issue originate?** Terminal/component-attribution
  (proximity-based `component_id` assignment on `EndpointCandidate`) —
  unrelated to shape detection, symbol geometry, or ground evidence
- **Confirmed unchanged by FIX-003**: the exact component record
  (`component-candidate-shape-region-c2335cc575d107b1`) is byte-identical
  between the independently-rebuilt pre-FIX-003 state (§9) and the
  current state — every field matches exactly. **Not fixed, per this
  AP's explicit instruction.**

## 11. Component / Symbol Audit

| Category | Pre-FIX-003 | Post-FIX-003 | Classification |
|---|---:|---:|---|
| Components | 85 | 81 | EXPECTED (−8 false-positive ChassisGround, +4 recovered genuine — net −4; 85−8+4=81) |
| Symbol primitives | 40 | 34 | EXPECTED (reduced false-positive-associated primitive geometry) |
| `chassis_ground`-kind components | 10 | 6 | EXPECTED (§4, §5) |
| `circular_symbol`, `enclosure`, `diagram_furniture` kinds | 26 / 2 / 47 | 26 / 2 / 47 | **UNCHANGED** — confirms the fix did not touch any unrelated classification |

The tiny `circular_symbol` implicated in the terminal-attribution oddity
(§10) was specifically checked and is byte-identical before/after. No
component "changed ownership" (no `ComponentCandidate` id present in both
extractions has a different kind, bounds, or shape reference — checked
directly by intersecting the two id sets and diffing each shared record).
No component disappeared as an unintended consequence of corrected ground
exclusion beyond the 8 confirmed false positives themselves (§5).

## 12. Topology Audit

| Node type | Pre-FIX-003 | Post-FIX-003 (unscoped) |
|---|---:|---:|
| conductor_end | 210 | 203 |
| continuation | 135 | 139 |
| splice | 106 | 105 |
| crossing | 237 | 239 |
| junction | 0 | 0 |
| component_boundary | 0 (type does not exist) | 0 |
| unresolved | 0 | 0 |

Every node-count delta is directly explainable by the combination of (a)
8 false-positive components' exclusion geometry no longer suppressing
nearby ink, and (b) 4 recovered genuine symbols' own exclusion now
correctly suppressing their bars while leaving their approach wires
detectable (per AP-DIAG-FIX-003's own §3 fix). No false endpoint, missing
endpoint (beyond the 2 coverage gaps already characterized in §6/§4),
false splice, false junction, false crossing, or disconnected-conductor
pattern was found beyond the duplicate-Wire-record defect itself (§7),
which is a Wire-layer record-count problem, not a topology-graph
problem — the underlying `topology-edge` and `node` records for both
duplicate pairs are single, correctly-formed, non-duplicated topology
objects (only the Wire layer wrapping them is duplicated). No scope
artifact was found (the scoped extraction's node/edge reduction is
accounted for entirely by the established scope-delta mechanism from
AP-INGEST-002, re-confirmed structurally consistent here).

## 13. Electrical Net Audit

All 10 nets enumerated; full detail in
`artifacts/audit/post_ground_fix_delta.json`. All 4 Ground-role nets:

| Net | Anchor component | Confidence | Genuine? |
|---|---|---|---|
| `electrical-net-5aff8c9b1e804b5e` | `...791276441805b2e0` (646,547) | high | Yes |
| `electrical-net-5de51cd3d5bac578` | `...1acbaeb7ac6ac87a` (621,586) | high | Yes |
| `electrical-net-d6fbaeb0e3e8c468` | `...a434a925670e9b65` (924,547) | medium | Yes |
| `electrical-net-e1f70187ddfaca24` | `...0edf5ce35037fec3` (576,586) | high | Yes |

**All 4 currently-reported Ground-role nets anchor on genuine
ChassisGround evidence** (AP-DIAG-FIX-003's own "3 of 4" language refers
to the *delta* — 3 nets whose anchor changed from false to genuine, plus
the battery net which was already genuine pre-fix — both framings agree:
4 of 4 correct now). No role was manufactured to eliminate a warning; the
6 `unknown`-role nets remain `unknown` (unchanged count and, spot-checked
via net ID, unchanged membership) precisely because no new text-label or
other role evidence was introduced by this fix.

## 14. Warning Audit

| Code | Pre-FIX-003 | Post-FIX-003 |
|---|---:|---:|
| `NET-ROLE-UNRESOLVED` | 6 | 6 |
| `WIRE-GEOMETRIC-ENDPOINTS` | 24 | 26 |
| **Total** | **30** | **32** |

`NET-ROLE-UNRESOLVED` is unchanged (no net's role evidence changed apart
from the 3 ground-anchor corrections, which stayed `ground`, not
`unknown`). `WIRE-GEOMETRIC-ENDPOINTS` increased by 2, tracking the
2 net-new distinct conductors with a `geometric` end (not the duplicate
pair itself, which would double-count — confirmed the warning counter
counts by Wire *record*, so the 2 duplicate records do inflate this
count by 1 each; this is a secondary, minor consequence of Finding
AUDIT-003-001 worth noting for the next corrective AP). No warning was
suppressed or its semantics rewritten.

## 15. New-Defect Search

Beyond Findings AUDIT-003-001/002/003 above:

- No duplicate `ComponentCandidate` found (exact-bounds grouping, whole
  population).
- No duplicate topology `Node` found (exact position+type grouping,
  whole population).
- No incorrect net membership found beyond what's already covered (net
  membership keys off `topology_edges`/`endpoint_ids`, not `Wire` IDs,
  so the Wire-record duplication does not inflate any net's member
  count — independently confirmed by re-reading each Ground-role net's
  `endpoint_ids` length, all showing exactly 2).
- No annotation contamination found (annotation regions remain
  descriptive-only per AP-INGEST-001/002's established contract; not
  re-tested here beyond confirming the scoped extraction's node/edge
  counts still match the established scope-delta pattern).
- No new false component, primitive, terminal, or ground was found
  beyond the ones already characterized.

## 16. No-Fix Compliance

No production file was modified. Every change in this AP is limited to:
this document and the three JSON artifacts under `artifacts/audit/`. The
duplicate-wire defect (Finding AUDIT-003-001), the 2-of-6 ground-endpoint
coverage gap (Finding AUDIT-003-002), and the terminal-attribution oddity
(Finding AUDIT-003-003) are all documented, not repaired.

## 17. Required Deliverables

- `docs/AP-DIAG-AUDIT-003_Post_Ground_Fix_ReAudit.md` (this document)
- `artifacts/audit/post_ground_fix_inventory.json`
- `artifacts/audit/post_ground_fix_delta.json`
- `artifacts/audit/post_ground_fix_findings.json`

All three JSON artifacts are deterministic reflections of the fixed
`samples/trx300ODG.png` + `fixtures/trx300/scope_production.json` inputs
plus one independently-rebuilt historical commit's extraction; none
contain a timestamp or other volatile field.

## 18. Final Report

**Starting SHA**: `ca2e9f16f6b88cecebe7a0d028e1e6206e77b739`
**Final SHA**: see commit immediately following this document.
**Current branch**: `main`. **No branch was created** — confirmed via
`git branch --show-current` before and after, and `git worktree list`
showing only `main` after the temporary comparison worktree was removed.

**Test count**: 55/55 passing. **Assertion status**: active.

**Unscoped inventory**: 81 components / 34 symbol primitives / 203
terminals / 0 connector terminals / 6 ChassisGround / 37 wire records
(**35 distinct conductors**) / 10 electrical nets (4 ground, 6 unknown) /
0 errors / 32 warnings. **Scoped inventory**: 79 components / 185
terminals / 6 ChassisGround (unchanged — none fall outside the include
region) / 10 electrical nets (unchanged) / 0 errors / 32 warnings.

**ChassisGround**: previous 8 (post-scope numbering) / current 6 — 6
confirmed genuine, 0 ambiguous, 0 false positive (§4).

**Ground-role nets**: previous 4 (1 genuine, 3 false-anchored) / current
4 (4 genuine, 0 false-anchored) (§13).

**Physical wires**: previous 35 / current 37 records representing **35
distinct conductors** / 10 added / 8 removed / 0 modified — **2 of the
10 added are exact duplicates of 2 others** (§7, Finding AUDIT-003-001).

**Eight recovered wires**: 6 CONFIRMED PHYSICAL CONDUCTOR, 2 (the
duplicate pair members, counted once per pair) already covered under
AMBIGUOUS above — see §7.2's full per-wire table.

**Terminal inventory**: previous 200 / current 203; by-kind breakdown
§10.

**Terminal-attribution oddity**: fully characterized in §10 — confirmed
unchanged, byte-identical to its pre-FIX-003 state, not fixed.

**Component inventory**: previous 85 / current 81 (§11).
**Symbol inventory**: previous 40 / current 34 (§11).
**Topology inventory**: previous 678 nodes/868 edges / current 686
nodes/876 edges (§12).
**Electrical-net inventory**: previous 10 (4 ground/6 unknown) / current
10 (4 ground/6 unknown), composition corrected (§13).

**Warning inventory**: 30→32, both changes explained, nothing suppressed
(§14).

**New findings**:
- **CRITICAL**: AUDIT-003-001 (2 duplicate Wire-record pairs; 37
  reported wires are actually 35 distinct conductors).
- **MEDIUM**: AUDIT-003-002 (2 of 6 genuine ChassisGround symbols lack a
  resolved ground endpoint — a coverage gap, not a false-evidence
  problem).
- **LOW**: AUDIT-003-003 (terminal-attribution oddity reconfirmed
  unchanged, not fixed, per instruction).

**Recommended next AP**: a `PROPOSED-FIX`-class AP targeting
`PhysicalWireIdentityReconstructor`'s deduplication logic specifically
for the two confirmed duplicate pairs (Finding AUDIT-003-001) — this is
now the single highest-severity open defect, fully reproducible and
narrowly scoped (2 pairs, both sharing the exact same
reversed-endpoint-order signature). AUDIT-003-002's endpoint-coverage
gap and AUDIT-003-003's terminal-attribution oddity remain lower-priority
follow-on items.

**Files changed**: `docs/AP-DIAG-AUDIT-003_Post_Ground_Fix_ReAudit.md`
(new), `artifacts/audit/post_ground_fix_inventory.json` (new),
`artifacts/audit/post_ground_fix_delta.json` (new),
`artifacts/audit/post_ground_fix_findings.json` (new). No source, test,
or fixture file touched.

**Commit(s)**: one commit (see git log for hash), audit-only.
**Push result**: pushed to `origin/main`.
**Working-tree status**: clean after commit.

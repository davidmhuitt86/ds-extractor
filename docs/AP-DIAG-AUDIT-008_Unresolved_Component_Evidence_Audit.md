# AP-DIAG-AUDIT-008 — Unresolved Electrical Component Evidence Audit

This is an audit document. **No production classifier, resolver, extractor,
topology, terminal, ground, connector, scope, or model code was modified
to produce it.** All work reads the unmodified `WireModel` produced by
the unmodified `ExtractionPipeline`, plus direct pixel inspection of
`samples/trx300ODG.png`. The one new artifact is
`tools/component_semantic_audit.cpp`, a standalone, audit-only
diagnostic executable (`dx-audit-component-semantic`) that dumps
already-computed model collections to JSON without altering how any of
them are computed. `git status --short` was empty before this document,
its companion JSON artifact, and that tool were added.

## 1. Starting Repository State

- `git rev-parse HEAD`: `62131871ae3fd233d46d3772fc12f6d3ecb9ebde`
- `git branch --show-current`: `main`
- `git status --short`: empty (clean)
- Baseline re-verified via clean Release rebuild before any audit work:
  61/61 tests passing, assertions active, 8 compiler warnings, 34 runtime
  warnings, 37 physical wires, 12 electrical nets, 6/6 ChassisGround
  endpoints, 81 `ComponentCandidate`s, 0 resolved `ElectricalComponent`s,
  28 unresolved, 53 rejected (47 furniture + 6 chassis-ground
  references), 0 validation errors. All match the task's expected
  baseline exactly.

## 2. How the AP-DIAG-FIX-008 Implementation Actually Works

Read directly from `include/eke_dx_wire/core/model.hpp`,
`include/eke_dx_wire/topology/electrical_component_resolver.hpp`,
`src/topology/electrical_component_resolver.cpp`, and
`tests/test_electrical_component_resolver.cpp`:

- **`ElectricalComponentResolver::resolve()`** consumes exactly four
  inputs: `component_candidates`, `symbol_family_resolutions`,
  `connector_candidates`, `endpoint_candidates`. It does **not** consult
  `symbol_primitives`, `terminal_candidates` (the separate, earlier
  `TerminalCandidate` model type - see below), `rejected_geometry`,
  `conductor_segments`, `nodes`/`edges`, `wires`, `electrical_nets`,
  `semantic_associations`, or `text_recognition_evidence` at all. This
  is a deliberate, narrow design (Section 5 of AP-DIAG-FIX-008's own
  doc), but it means a great deal of evidence that already exists
  elsewhere in the model is currently invisible to the resolution
  decision.
- A candidate becomes **Resolved** only if it has (a) at least one
  `EndpointCandidate` of `kind == ComponentTerminal` attributed to it,
  AND (b) a `SymbolFamilyResolution` with `status == Resolved` and
  `family` neither `Unknown` nor `Ground`.
- **Rejected** happens structurally, before any evidence check, for
  `DiagramFurniture` kind, `ChassisGround` kind, or `ConnectorCandidate`
  ownership - these three are never even evaluated against the
  evidence rule.
- Everything else that fails the Resolved test becomes **Unresolved**.
  There are exactly 28 such candidates in the current model.

### Evidence that exists in the model but is not consulted by the resolver

- **`TerminalCandidate`** (`model.terminal_candidates`, distinct from
  `EndpointCandidate`): produced by `TerminalRecognizer`/
  `TerminalLocationDetector` earlier in the pipeline, carrying
  `kind` (`ComponentBoundary`/`ConnectorBoundary`/`GroundConnection`),
  `distance_to_component`, and its own `confidence`. **Not exposed in
  `topology.json` at all** (no existing exporter serializes it) - this
  audit's new tool is the first thing to surface it outside the C++
  model. There are 18 `TerminalCandidate`s in the whole canonical model.
- **`RejectedGeometryEvidence`** with `classification: component_associated`
  - already used elsewhere (it is exactly what backs
  `endpoint-candidate-9da9c73ab52013a3`'s ownership per AP-DIAG-FIX-006)
  but not consulted by `ElectricalComponentResolver`.
- **`SemanticAssociation`** (`LabelToComponent` relation): built purely
  from geometric distance between a `TextRegion`'s bounding box and a
  component's bounding box, **independent of whether the text has ever
  been recognized**. 1729 exist in the current model, many attached to
  the 28 unresolved candidates (Section 4). This is proximity evidence
  only, never content.
- **`TextRecognitionEvidence`** (the actual OCR'd string): **0 entries**
  in the canonical baseline. `main.cpp`'s default `extract` path only
  wires a `TextRecognitionProvider` when `--recognition` or
  `--vision-recognition` is passed; neither was used here. This is the
  single most consequential absence found in this audit (Section 8).
- **Physical `Wire`/`ElectricalNet` membership**: fully computed
  elsewhere in the pipeline (`PhysicalWireIdentityReconstructor`,
  `ElectricalNetResolver`) but never consulted by
  `ElectricalComponentResolver` - a candidate can be electrically
  connected to a real, reconstructed Wire and the resolver still has no
  way to use that as evidence.

### Architectural gap: `ComponentCandidate` has no `Provenance` field

Unlike `SymbolPrimitive`, `ConductorSegment`, and `RejectedGeometryEvidence`
(all of which carry a `Provenance{source_id, page, source_region, stage}`),
`ComponentCandidate` itself has no such field. Source provenance for a
candidate must be reconstructed indirectly, via its owned
`SymbolPrimitive`s' provenance or any `RejectedGeometryEvidence`
associated with it - and for the 19 of 28 unresolved candidates with
zero owned primitives and zero associated rejected geometry, **no
provenance is recoverable at all**. Every candidate entry in this
audit's JSON artifact records this explicitly as
`"provenance": "NOT_PRESENT_IN_CURRENT_MODEL"`.

## 3. The 28 Unresolved Candidates — Full Inventory

Produced deterministically by `dx-audit-component-semantic` (new,
audit-only tool) against the unmodified pipeline. Complete per-candidate
evidence (bounds, symbol primitives, terminal candidates, endpoint
candidates, topology nodes/edges, conductor segments, physical wires,
electrical nets, rejected geometry, semantic associations) is recorded
in `artifacts/trx300/component_semantic_audit.json`. Summary:

| ID (suffix) | Kind | Primitives | Terminals | Endpoints | Wires | Nets | Semantic assoc. |
|---|---|---:|---:|---:|---:|---:|---:|
| 066e8ad0e7654cff | circular_symbol | 0 | 0 | 0 | 0 | 0 | 6 |
| 0771bb11fe6c347d | enclosure | 6 | 0 | 0 | 0 | 0 | 8 |
| 0a02724ece05df31 | circular_symbol | 1 | 0 | 0 | 0 | 0 | 8 |
| 0b053d4e7a47606f | circular_symbol | 0 | 0 | 0 | 0 | 0 | 0 |
| 16df4d7df3b94a27 | enclosure | 3 | 0 | 0 | 0 | 0 | 13 |
| 1975579e8330101e | circular_symbol | 0 | 0 | 0 | 0 | 0 | 0 |
| 1acff69cbd97d3ef | circular_symbol | 0 | 0 | 0 | 0 | 0 | 11 |
| 25e5f8f85abff207 | circular_symbol | 0 | 0 | 0 | 0 | 0 | 8 |
| 3272c9e5f76ee915 | circular_symbol | 0 | 0 | 0 | 0 | 0 | 0 |
| 3bf0724f52f58c73 | circular_symbol | 0 | 0 | 0 | 0 | 0 | 1 |
| 41ff861aeda09fb7 | circular_symbol | 0 | 0 | 0 | 0 | 0 | 0 |
| 428e96f28d67cd74 | circular_symbol | 0 | 0 | 0 | 0 | 0 | 1 |
| 4903417d6d8a60bb | circular_symbol | 0 | 0 | 0 | 0 | 0 | 1 |
| 558bc00c87382a7d | circular_symbol | 0 | 0 | 0 | 0 | 0 | 0 |
| 62af50129e3fe173 | circular_symbol | 0 | 0 | 0 | 0 | 0 | 0 |
| 68f8e8348812d438 | circular_symbol | 0 | 0 | 0 | 0 | 0 | 1 |
| 710412374a750fc8 | circular_symbol | 0 | 0 | 0 | 0 | 0 | 7 |
| 763aea0b9f4edcf6 | circular_symbol | 1 | 1 | 1 | 0 | 0 | 9 |
| 845947b0afd7451a | circular_symbol | 0 | 1 | 1 | 0 | 0 | 12 |
| 9aef4bd701b22c45 | circular_symbol | 0 | 0 | 0 | 0 | 0 | 1 |
| 9c4d423986244e04 | circular_symbol | 1 | 1 | 1 | 0 | **1** | 9 |
| b061e4623d55459e | circular_symbol | 0 | 0 | 0 | 0 | 0 | 2 |
| b9c802997a4ef73a | circular_symbol | 0 | 0 | 0 | 0 | 0 | 1 |
| bfae05a427189376 | circular_symbol | 2 | **7** | **7** | **2** | 0 | 6 |
| c2335cc575d107b1 | circular_symbol | 0 | 0 | 0 | 0 | 0 | 4 |
| c98f6566b0672535 | circular_symbol | 2 | 2 | 2 | **1** | 0 | 12 |
| d1aefcaca5503ad9 | circular_symbol | 0 | 0 | 0 | 0 | 0 | 9 |
| d82ce67010a762de | circular_symbol | 0 | 0 | 0 | 0 | 0 | 9 |

`symbol_family` is `unknown`/`unresolved` for **all 28**, confirmed
directly. All 12 non-ground `TerminalCandidate`s in the entire model
(`terminal_candidates_count: 18` globally, minus 6 already attached to
the genuine `ChassisGround` symbols) are attributed to exactly **5** of
these 28 candidates - the other 23 have zero terminal evidence anywhere
in the model, not merely zero *resolved* terminal evidence.

## 4. Evidence Classification (by category, per Section 3 of the task)

| Category | Code | Count | Meaning |
|---|---|---:|---|
| GEOMETRY | - | 28/28 Present | every candidate has a bounding box and a shape-level `ComponentSymbolRecognition` entry |
| SYMBOL_PRIMITIVE | - | 9/28 Present, 19/28 Absent | `symbol_primitives` per candidate ranges 0-6 |
| SYMBOL_FAMILY | - | 0/28 Present (all Absent) | every `SymbolFamilyResolution` for these 28 is `Unresolved`/`Unknown` |
| TERMINAL | - | 5/28 Present, 23/28 Absent | see Section 3 table |
| CONDUCTOR / TOPOLOGY | - | 5/28 Present (same 5), 23/28 Absent | terminal presence and topology-edge presence coincide exactly in the current model |
| WIRE | - | 2/28 Present, 26/28 Absent | `bfae05a427189376` (2 wires), `c98f6566b0672535` (1 wire) |
| ELECTRICAL_NET | - | 1/28 Present, 27/28 Absent | `9c4d423986244e04` only |
| LABEL/TEXT | - | 22/28 Ambiguous, 6/28 Absent, 0/28 Present | geometric text-region *proximity* exists for 22 (`semantic_associations`), but **recognized string content is Absent for all 28** (`text_recognition_evidence` is empty globally) - proximity without content is reported as Ambiguous, never treated as identity evidence |
| PROVENANCE | - | 0/28 Present (structurally), partial recoverable via constituent evidence for 9/28 | `ComponentCandidate` itself carries no `Provenance` field (Section 2) |
| OWNERSHIP | - | 6/28 Present (5 via TerminalCandidate + 1 additional via RejectedGeometryEvidence for `845947b0afd7451a`), 22/28 Absent | AP-DIAG-FIX-005's ownership-evidence rule, re-verified: every terminal-bearing candidate here has qualifying ownership evidence, never mere proximity |

**Absence is never read as negative evidence beyond what the pipeline
itself establishes.** In particular: `TERMINAL = absent` for 23
candidates means no `TerminalCandidate`/`EndpointCandidate` currently
exists for them - it does **not** mean "these objects have no
terminals," since terminal-lead extraction is independently known to be
incomplete for at least 2 of them (Section 6).

## 5. Semantic Evidence Level (audit-only classification)

| Level | Count | Candidates |
|---|---:|---|
| E0 — no meaningful electrical evidence | 14 | see Section 7 (all 14 are the crossing-artifact/confirmed-false-positive group) |
| E1 — geometric/symbol evidence only | 9 | `066e8ad0e7654cff`, `0771bb11fe6c347d`, `0a02724ece05df31`, `16df4d7df3b94a27`, `1acff69cbd97d3ef`, `25e5f8f85abff207`, `68f8e8348812d438`, `710412374a750fc8`, `d82ce67010a762de` |
| E2 — symbol + structural (terminal/topology) evidence | 3 | `763aea0b9f4edcf6`, `845947b0afd7451a`, `9c4d423986244e04` |
| E3 — symbol + terminal + physical Wire evidence | 2 | `bfae05a427189376`, `c98f6566b0672535` |
| E4 — identity evidence sufficient for semantic resolution | 0 | none - confirms `resolved_electrical_components: 0` is not an implementation gap but a correct reflection of current evidence |
| EX — contradictory evidence | 0 | none found |

No candidate was assigned E4: even `bfae05a427189376` (the strongest,
E3) has zero family/identity evidence, only structural/connectivity
evidence that it is *some* real electrical object.

## 6. Do Not Guess — What This Audit Observed vs. Asserts

Two enclosure candidates carry source-raster text that a human reader
can read directly:

- **`0771bb11fe6c347d`** (870,517,52,29): visually a battery symbol -
  two terminal leads topped with circular `+`/`−` marks - with adjacent
  raster text that appears to read **"BATTERY"** and **"12V12AH"**.
- **`16df4d7df3b94a27`** (216,128,68,39): visually an enclosure with
  raster text that appears to read **"CDI UNIT"** directly inside the
  box outline.

**These are audit observations, not production facts.**
`text_recognition_evidence` is empty for the entire canonical baseline
(OCR is not connected), so the pipeline itself has no access to this
reading - it is not "identity unresolved because the resolver missed
obvious evidence," it is "identity unresolved because the evidence
channel that would carry this information (OCR → `TextRecognitionEvidence`
→ `SemanticAssociation` content) is architecturally not wired into the
canonical baseline," exactly as expected given this AP's explicit "do
not add OCR" constraint. No component type was asserted for either
candidate in the audit artifact; both are recorded with `category: B`
and a non-authoritative `note`, per the task's own required phrasing
("candidate resembles X" only as a marked, non-authoritative
observation).

No other candidate in the 28 was assigned a specific component-type
guess anywhere in this audit.

## 7. Candidate Relationships — Fragments, Duplicates, False Positives

**Confirmed false positives (2)** — carried forward unchanged from
AP-DIAG-AUDIT-004/007, re-verified still present and still fully
suppressed by AP-DIAG-FIX-005's ownership gate:
`c2335cc575d107b1` (annotation-leader dot), `d1aefcaca5503ad9`
(ambiguous non-symbol mark).

**Suspected false positives — crossing-gap artifact (12)**: direct
raster inspection (cropped, upscaled, and bounding-box-annotated against
`samples/trx300ODG.png`) of every remaining small `circular_symbol`
candidate with zero primitives found 11 **new** instances of the exact
false-positive pattern AP-WIRE-FIX-002 partially corrected — a bounding
box sitting at a plain bus/grid wire crossing with **no drawn circle at
all** — at coordinates distinct from the 3 AP-WIRE-FIX-002 already
fixed: `0b053d4e7a47606f`, `3272c9e5f76ee915`, `3bf0724f52f58c73`,
`41ff861aeda09fb7`, `428e96f28d67cd74`, `4903417d6d8a60bb`,
`558bc00c87382a7d`, `62af50129e3fe173`, `9aef4bd701b22c45`,
`b061e4623d55459e`, `b9c802997a4ef73a`. Together with the
already-known `1975579e8330101e` (AP-DIAG-AUDIT-007), that is **12 of
28** unresolved candidates - 43% - suspected to be this single
false-positive class. Three of them cluster tightly (`3272c9e5f76ee915`,
`41ff861aeda09fb7`, `558bc00c87382a7d`, all within ~10-15px of each
other at x≈584-585) at three consecutive crossings of the same vertical
bus wire. **These are reported as suspected, not confirmed** - no
production change was made, and confirming them would need the same
circularity/edge-support/interior-density instrumentation methodology
AP-WIRE-FIX-002 used.

**Genuine small connection marks, real ink, modeling-boundary question
(4)**: `066e8ad0e7654cff`, `25e5f8f85abff207`, `68f8e8348812d438`,
`710412374a750fc8` - each sits on real, drawn ink at a wire lead
entering an enclosure or at a deliberately-marked junction dot. These
are not false positives in the "no ink" sense, but whether they should
be represented as components at all, versus `Splice`/`Junction`
topology events (AP-WIRE-029 §9-10), is an open modeling question this
audit does not resolve.

**Genuine repeated symbols, correct geometry, unclassified identity
(2)**: `1acff69cbd97d3ef`, `d82ce67010a762de` - AP-DIAG-AUDIT-007's
already-confirmed pair of visually identical ring symbols on a wire run.

**Probable fragments of a larger symbol (1 clear + 2 noted)**:
`0a02724ece05df31` sits directly above a round symbol containing an
internal zigzag mark, as part of a row of three visually near-identical
bracket-over-circle groups; visual inspection found only **two** of
those three groups have any `ComponentCandidate` at all (`0a02724ece05df31`
and `9c4d423986244e04` - the third, visually identical, has none - see
Section 10). `9c4d423986244e04` and `c98f6566b0672535` are additionally
noted as *possible* fragments of a larger assembly (a round zigzag
symbol and a multi-position fuse/terminal-block strap respectively)
despite each carrying its own real `TerminalCandidate`/topology/(net or
Wire) evidence - fragment suspicion and structural evidence are not
mutually exclusive here.

**28 candidates are therefore very likely not 28 independent potential
electrical objects.** At most 28 minus (12 suspected crossing artifacts)
minus (2 confirmed false positives) minus (1 clear fragment) = **13**
are plausibly independent real objects, and even that number is
uncertain (two more, `9c4d423986244e04`/`c98f6566b0672535`, carry
fragment suspicion themselves).

## 8. Text / Label Audit

- **OCR / text recognition: not invoked.** `text_recognition_evidence`
  has 0 entries in every canonical run in this audit (confirmed
  directly, not assumed). `main.cpp`'s default `extract` path only
  wires a provider when `--recognition`/`--vision-recognition` is
  explicitly passed.
- **Text *regions* (geometric bounding boxes) do exist** and are
  associated to components purely by distance via `SemanticAssociation`
  (1729 total in the model). 22 of the 28 unresolved candidates have at
  least one such association (up to 13 for `16df4d7df3b94a27`).
- **No recognized string content exists anywhere connected to a
  `ComponentCandidate`.** This is the exact distinction the governing
  task called out: "nearby labels/text, IF already represented" -
  proximity is represented; content is not.
- This is confirmed as an **architectural evidence gap**, not touched by
  this AP: the wiring exists (`SemanticAssociation` → target_kind
  `Component`) for content to flow to components the moment a
  `TextRecognitionProvider` is connected in the canonical baseline: no
  new plumbing would be needed, only a recognition source.

## 9. Symbol Family Audit

- `SymbolFamily::Fuse`, added by AP-DIAG-FIX-008, is confirmed
  **taxonomy-only**: `symbol_family_recognizer.cpp`'s keyword table
  (`label_rules()`) has no `Fuse` entry, and no candidate in the entire
  canonical model - resolved, unresolved, or rejected - has
  `family: fuse`. It has not been incorrectly treated as a production
  semantic resolver; it exists purely so `ElectricalComponentResolver`'s
  promotion rule could be unit-tested against a genuine
  protection-function family (already verified in AP-DIAG-FIX-008's own
  Test 6).
- Of the 28 unresolved candidates: **0 have a recognized `SymbolFamily`
  of any kind** (all are `unknown`/`unresolved`). This is expected: the
  only currently-productive family rule that does not require label
  text is the purpose-built `ChassisGround` classifier, and all 6
  genuine `ChassisGround` components are already `Rejected` (correctly,
  per Definition F), not counted among these 28. Every other family rule
  requires a label keyword match against already-recognized text, which
  requires OCR (Section 8) - unavailable in the canonical baseline.

## 10. Missing Objects — Candidate Exists vs. No Candidate At All

This audit found **one concrete, visually-confirmed example** of "no
candidate exists": the row of three visually near-identical bracket/
circle-with-zigzag symbol groups discussed in Section 7 has
`ComponentCandidate`s for only two of the three positions
(`0a02724ece05df31` at (600,523), `9c4d423986244e04` at (579,523), ~21px
apart) - a systematic search of `component_candidates` for any entry
centered near the third, visually identical position (further right,
~x 618-635, same y) found **none**. This is upstream shape-detection
incompleteness, not a semantic-resolution problem: no amount of symbol-
family or OCR work on the existing 81 candidates can produce
representation for an object that has no candidate at all.

A full, exhaustive correspondence between the 28 unresolved candidates
(or the 81 total) and the 24 expected TRX300 objects was **not**
attempted in this audit - doing so would require asserting specific
object identities for candidates that currently have no supporting
evidence, which this AP's own instructions and the project's standing
never-guess rule both forbid. The one missing-candidate example above
was found by targeted visual comparison during the false-positive/
fragment sweep (Section 7), not by a systematic 24-object walk.

## 11. Missing Evidence To Resolve the Legitimate Candidates

For the 7 candidates carrying real structural evidence (Section 5, E2/E3)
plus the 2 battery/CDI candidates (E1, but with human-legible identity
text present in the raster):

- **Text/OCR connection to component identity** is the single largest
  gap by volume of direct evidence found in this audit - two candidates
  have their identity apparently written directly in the diagram
  (Section 6), and 22 of 28 have at least one nearby text region whose
  content could plausibly disambiguate them once recognized.
- **Terminal-lead extraction completeness for enclosure-kind candidates**
  is a distinct, structural gap: `0771bb11fe6c347d`'s two visibly-drawn
  `+`/`−` terminal leads and `16df4d7df3b94a27`'s visible bottom leads
  produced **zero** `SymbolPrimitive`s of kind `TerminalLead` and zero
  `TerminalCandidate`s - this is upstream of symbol-family or OCR work
  entirely.
- **Connector-geometry recognition** may have a coverage gap:
  `bfae05a427189376`'s 7-terminal, multi-wire-converging body visually
  resembles a multi-pin connector, yet `connector_candidates` is 0
  globally in this model. If this object is in fact a connector, no
  amount of `ElectricalComponentResolver`/`SymbolFamilyRecognizer` work
  will ever correctly classify it, because Definition E requires
  connectors to be excluded from `ElectricalComponent` identity
  entirely - the correct fix would be upstream, in connector detection.

## 12. TRX300 24-Object Reference Comparison

Used only as a reference datum, per this AP's instructions - not as a
target the extractor was tuned toward:

- 6 of the 24 expected objects are very likely represented by the 6
  genuine `ChassisGround` components (already `Rejected` under
  Definition F, correctly excluded from "ordinary component" counting
  but real, resolved *reference* objects nonetheless).
- Some fraction of the remaining ~18 expected components/modules are
  very likely among the 34 "real" (non-furniture) `ComponentCandidate`s
  - both the 28 discussed here and the confirmed false positives among
  them.
- At least 1 expected symbol position has **no** candidate at all
  (Section 10) - a concrete, visually-confirmed representation gap.
- No attempt was made to assign specific names/identities to any of the
  24 from this audit - per the task's explicit instruction, only
  evidence that is "clearly documented in existing source evidence" may
  be recorded with provenance, and the two cases where raster text
  appears to name an object (`0771bb11fe6c347d`/"BATTERY",
  `16df4d7df3b94a27`/"CDI UNIT") are recorded as non-authoritative audit
  observations, not established identities, because the pipeline itself
  has no OCR evidence backing that reading.

## 13. Architectural Gaps Identified

1. `ElectricalComponentResolver` does not consult `TerminalCandidate`,
   `RejectedGeometryEvidence`, `ConductorSegment`/topology, `Wire`, or
   `ElectricalNet` evidence at all (Section 2) - a candidate can be
   provably, physically connected to a real reconstructed Wire and this
   still counts for nothing in the resolution decision.
2. `ComponentCandidate` has no `Provenance` field (Section 2) - unlike
   every other major evidence type in the model.
3. `TerminalCandidate` is never serialized into any existing export
   (`topology.json` included) - it was invisible to every audit before
   this one's new diagnostic tool.
4. Text-region geometric proximity (`SemanticAssociation`) and text
   *content* (`TextRecognitionEvidence`) are two independently
   absent-or-present evidence sources; conflating "a text region is
   nearby" with "we know what it says" would be a category error this
   audit deliberately avoided (Section 8).
5. Terminal-lead extraction (`SymbolPrimitiveKind::TerminalLead` →
   `TerminalCandidate`) appears incomplete for at least 2 enclosure-kind
   candidates with clearly-drawn leads (Section 11).
6. Connector-geometry recognition may have a coverage gap distinct from
   the already-known "0 `ConnectorCandidate`s" limitation - at least one
   unresolved candidate's shape is more consistent with a connector body
   than a simple component (Section 11).
7. At least one expected symbol position in the source diagram has no
   `ComponentCandidate` representation at all (Section 10) - upstream
   shape-detection incompleteness, not a resolution problem.
8. A likely-unaddressed extension of the AP-WIRE-FIX-002 crossing-gap
   false-positive pattern accounts for 12 of the 28 unresolved
   candidates (Section 7) - none of these were among the 3 coordinates
   that fix corrected.

## 14. Final Analysis — Answers to the Required Questions

1. **What are the 28 unresolved candidates?** Listed in full in Section
   3 and `artifacts/trx300/component_semantic_audit.json`; 26
   `circular_symbol` + 2 `enclosure` kind, all with `family: unknown`.
2. **How many have meaningful electrical evidence?** 7 (E2/E3: real
   `TerminalCandidate`/topology evidence), plus 2 more (E1) with
   human-legible but OCR-inaccessible identity text in the raster - 9
   total worth distinguishing from the remaining 19.
3. **How many have terminal evidence?** 5 (`763aea0b9f4edcf6`,
   `845947b0afd7451a`, `9c4d423986244e04`, `bfae05a427189376`,
   `c98f6566b0672535`).
4. **How many have conductor/topology evidence?** The same 5 - terminal
   presence and topology-edge presence coincide exactly in this model.
5. **How many have recognized symbol-family evidence?** **0**.
6. **How many have usable label/text evidence?** **0** with recognized
   content; 22 with unrecognized geometric proximity only.
7. **How many appear to be fragments of other candidates?** 1 clear
   (`0a02724ece05df31`), 2 more noted as possible fragments despite
   having their own terminal evidence (`9c4d423986244e04`,
   `c98f6566b0672535`).
8. **How many appear to be false positives?** 2 confirmed
   (pre-existing, AP-DIAG-AUDIT-004) + 12 suspected (new crossing-gap
   finding this audit) = 14 of 28 (50%).
9. **How many appear to correspond to genuine electrical objects but
   lack sufficient identity evidence?** 7 with real structural evidence
   (E2/E3) + 2 with raster-visible but OCR-inaccessible identity text =
   **9**, with the caveat that 2 of the 7 (`9c4d423986244e04`,
   `c98f6566b0672535`) carry simultaneous fragment suspicion.
10. **How many of the expected 24 have no candidate representation at
    all?** At least **1** concrete, visually-confirmed example found
    (Section 10); a full 24-object correspondence audit was not
    attempted (would require inventing identities, forbidden).
11. **What evidence is missing to resolve the legitimate candidates?**
    Text/OCR content (dominant by volume of direct evidence), complete
    terminal-lead extraction for enclosure-kind candidates, and possibly
    connector-geometry recognition for at least one candidate (Section
    11).
12. **Is the dominant next problem symbol recognition, OCR, terminal
    attribution, candidate grouping, semantic resolution, or upstream
    geometry extraction?** **A combination, but not evenly weighted**:
    text/OCR evidence is the largest single gap by volume (2 candidates
    with legible identity text, 22 with unread proximity text, and every
    existing non-Ground `SymbolFamily` rule already requires label
    text to fire); terminal-lead extraction is a distinct, smaller gap
    affecting at least 2 candidates; upstream geometry extraction
    (missing candidates entirely) affects at least 1 confirmed position.
    Symbol-family *rule-writing* is comparatively low-value right now,
    since the rules that exist already have nothing to match against.
13. **Which next AP should address the highest-value evidence gap?**
    See Section 15.

## 15. Recommended Next AP

In order of evidence-supported value, given the findings above:

1. **A text/OCR evidence AP** that connects a `TextRecognitionProvider`
   to the canonical baseline in a controlled, fully-audited way (with
   before/after evidence comparison at least as rigorous as this AP's),
   since two of the highest-evidence unresolved candidates have their
   identity apparently written directly in the diagram, and every
   existing non-Ground `SymbolFamily` rule already depends on exactly
   this evidence source. This is the single highest-leverage gap found.
2. **A `ShapeDetector` crossing-gap false-positive AP** (already
   recommended by AP-DIAG-AUDIT-007-F1, now supported by 12 additional
   suspected instances found here) - generalizing AP-WIRE-FIX-002's
   circularity/edge-support/interior-density methodology could plausibly
   remove close to half of the current 28 without any semantic
   recognition work at all.
3. **A terminal-lead extraction completeness AP** for enclosure-kind
   candidates, targeting the specific gap found for `0771bb11fe6c347d`
   and `16df4d7df3b94a27`.

None of these was implemented in this AP. This AP's purpose was solely
to establish what the extractor already knows before that work begins.

## Explicit Non-Changes

No change to: `ComponentCandidate` extraction, `ElectricalComponentResolver`
behavior, `SymbolFamily` recognition, conductor extraction, topology,
Wire reconstruction, Wire identity, Wire deduplication,
`ElectricalNetResolver`, ground detection, terminal detection, extraction
scope, or any existing export. Re-verified via clean Release rebuild
(Section 16): 61/61 tests, 8 compiler warnings (0 new), 34 runtime
warnings, 37 physical wires, 12 electrical nets, 6/6 ground endpoints, 0
validation errors, 81/0/28/53 `ComponentCandidate`/resolved/unresolved/
rejected - every value unchanged from the starting baseline.

## 16. Regression and Determinism

Clean Release rebuild, assertions active: 61/61 tests passing, 8
compiler warnings (0 new), identical to the starting baseline exactly.
Canonical `dx-extract` re-run after adding the new audit tool: 37 wires,
12 nets, 878 topology edges, 207 endpoint candidates, 0 validation
errors, 34 validation warnings - byte-identical to pre-audit values.

The new `dx-audit-component-semantic` tool was run twice unscoped and
twice scoped (`fixtures/trx300/scope_production.json`); all four raw
JSON outputs are deep-equal within their scope pair
(`unscoped_1 == unscoped_2`, `scoped_1 == scoped_2`), and the derived
`artifacts/trx300/component_semantic_audit.json` (built from the
unscoped run plus this document's fixed, hand-reviewed classification)
is therefore fully deterministic and reproducible by re-running the tool
against the same commit.

## 17. Final Commit and Push Status

See the Final AAR delivered alongside this document. Files changed:
`tools/component_semantic_audit.cpp` (new), `CMakeLists.txt` (one new
executable target), `docs/AP-DIAG-AUDIT-008_Unresolved_Component_Evidence_Audit.md`
(new), `artifacts/trx300/component_semantic_audit.json` (new). No other
file was touched.

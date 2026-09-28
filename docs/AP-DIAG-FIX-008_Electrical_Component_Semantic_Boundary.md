# AP-DIAG-FIX-008 — Electrical Component / Module Semantic Boundary

## Status

Architectural/semantic. This AP does not change conductor detection,
morphology, wire reconstruction, physical Wire identity, electrical net
resolution, ground detection, terminal detection, connector detection,
extraction scope, or OCR. It adds one new, purely additive resolution
stage (`ElectricalComponentResolver`) that consumes already-established
evidence and produces a new, separate reporting layer. Every existing
production extraction metric (topology, wires, nets, ground endpoints,
validation) is confirmed byte-identical before/after (Section 9).

## Governing principle

> **Extraction detects evidence. Semantic reconstruction establishes
> engineering identity.**
>
> **Geometry can create a candidate. Geometry alone cannot establish an
> electrical component.**

This principle governs `ElectricalComponent` identity the same way
`docs/AP-WIRE-029_Conductor_Boundary_and_Wire_Identity.md` governs
physical Wire identity. It should be treated as binding on all future
component-recognition work.

## 1. Formal definitions

**A. Geometric candidate** — a region/shape produced by image processing
with no inherent engineering meaning (a `ShapeRegion`, before it is even
grouped into a `ComponentCandidate`). Must never automatically become an
electrical component.

**B. Symbol primitive** (`SymbolPrimitive`, AP-WIRE-023) — a recognized
geometric primitive (line/circle/rectangle/terminal-lead) participating
in a possible engineering symbol, observed inside an already-detected
`ComponentCandidate`'s bounds. Still not necessarily an electrical
component.

**C. Component candidate** (`ComponentCandidate`) — an extraction-level
grouping that plausibly represents an engineering component/module.
Represents uncertainty at the extraction layer. **Not** equivalent to a
resolved `ElectricalComponent`. May ultimately become a resolved
`ElectricalComponent`, remain an unresolved candidate, or be rejected as
non-component geometry.

**D. Electrical Component / Module** (new: `ElectricalComponent`) — "An
identifiable electrical object represented by the engineering diagram
that has one or more electrical terminals and either controls,
transforms, stores, switches, consumes, produces, indicates, protects,
senses, actuates, or otherwise participates directly in the flow of
electrical energy or electrical control signals." Requires (1) an
identifiable electrical entity, (2) attributable electrical terminal(s),
and (3) an identifiable electrical function. Geometry alone is
insufficient.

**E. Electrical Interface** (`ConnectorCandidate`/`ConnectorTerminal`,
AP-WIRE-020/030, pre-existing) — a connector is an electrical interface
object. It is **never** automatically classified as an
`ElectricalComponent` merely because it has terminals — a connector may
have many terminals while performing no electrical energy/control
function itself. `ElectricalComponentResolver` explicitly rejects any
`ComponentCandidate` owned by a `ConnectorCandidate`
(`ElectricalComponentRejectionReason::ConnectorInterface`).

**F. Electrical Reference** (`ComponentCandidateKind::ChassisGround`,
pre-existing) — chassis ground is an electrical reference/termination
object, not an ordinary component/module. `ElectricalComponentResolver`
explicitly rejects every `ChassisGround`-kind candidate
(`ElectricalComponentRejectionReason::ChassisGroundReference`), even
when it has a resolved `SymbolFamily::Ground` identity and a valid
terminal — being definitively identified as ground is exactly why it is
excluded, not evidence for inclusion.

## 2. Hard semantic rule

> **`ComponentCandidate != ElectricalComponent`.**
>
> **Geometric detection alone MUST NOT establish `ElectricalComponent`
> identity.**

Promotion from `ComponentCandidate` to `ElectricalComponent` requires
independent evidence of electrical identity, terminal attribution, and
electrical function. If that evidence does not exist, the candidate is
preserved as `Unresolved` — never guessed into `Resolved`.

## 3. Inspection of the existing model (performed before writing code)

The repository already distinguishes several of the layers this AP
formalizes:

- `ComponentCandidate` (`include/eke_dx_wire/core/model.hpp`) is already
  documented as extraction-level ("uncertainty at the extraction layer");
  `ComponentSymbolRecognition`'s own header comment already states its
  `GeometricallyClassified` status "is not evidence that the specific
  symbol was identified, only that it was not Unknown-shaped."
- `SymbolFamilyResolution` (AP-WIRE-026A) already resolves specific
  engineering families (`Ground`, `Lamp`, `Switch`, `Relay`, `Motor`,
  `Diode`, `Alternator`, `Battery`, `Solenoid`, `Coil`) per component from
  independent evidence (a purpose-built geometric classifier for Ground,
  or a label-keyword + compatible-geometry rule for everything else). On
  the canonical TRX300 extraction, this resolves **only** `Ground` (all 6
  genuine `ChassisGround` components, `high` confidence) — every other
  component remains `Unknown`/`unresolved`, because OCR is off in the
  canonical baseline and no label-keyword evidence exists yet.
- `ConnectorCandidate`/`ConnectorTerminal` (AP-WIRE-020) already keep
  connector semantics as a wholly separate collection from
  `ComponentCandidate`/`ComponentSymbolRecognition` — no model change was
  needed to keep Definition E true; it already was.
- `TerminalCandidate`/`EndpointCandidate` (AP-WIRE-005/019/024/030)
  already require independent ownership evidence
  (`SymbolPrimitive`/`RejectedGeometryEvidence`) before attributing a
  `component_terminal`/`ground` endpoint to a component
  (AP-DIAG-FIX-005) — re-verified with 0 violations across all 18 such
  endpoints as part of this AP's regression (Section 9).
- `RejectedGeometryEvidence` already distinguishes geometry that was
  evaluated and rejected from geometry that was never a candidate at all.

**What did not exist**: a formal type distinguishing "this
`ComponentCandidate` has been evaluated and IS an electrical
component/module" from "this `ComponentCandidate` merely has a coarse
geometric bucket." `ComponentSymbolRecognition` and
`SymbolFamilyResolution` are both evidence *inputs* to that judgment, not
the judgment itself — neither one, by construction, ever asserts "this is
definitely an ElectricalComponent" versus "this is a chassis-ground
reference" versus "this is a connector interface." This AP adds exactly
that judgment, as `ElectricalComponentResolver` / `ElectricalComponent`,
and nothing else — no existing type was duplicated.

## 4. Semantic component model

New types (`include/eke_dx_wire/core/model.hpp`):

```cpp
enum class ElectricalComponentResolutionStatus {
    Resolved,
    Unresolved,
    Rejected
};

enum class ElectricalComponentRejectionReason {
    DiagramFurniture,
    ChassisGroundReference,
    ConnectorInterface,
    NotApplicable
};

struct ElectricalComponent {
    std::string id;
    std::string component_candidate_id;
    SymbolFamily family = SymbolFamily::Unknown;
    ElectricalComponentResolutionStatus status = ...Unresolved;
    ElectricalComponentRejectionReason rejection_reason = ...NotApplicable;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    std::vector<std::string> terminal_endpoint_ids;
    std::vector<std::string> evidence_ids;
};
```

`ElectricalComponentResolutionStatus` deliberately omits `Conflicted`:
this AP implements no evidence source capable of producing two
independently contradictory electrical-identity claims for the same
candidate. A future semantic-recognition AP that adds one should extend
this enum then.

Stable identity: `stable_id("electrical-component", component_candidate_id)`
— the same content-addressed convention every other resolver in this
codebase uses (mirrors `SymbolFamilyResolution`'s
`stable_id("symbol-family-resolution", component.id)` exactly).

`SymbolFamily` gained one new value, `Fuse` — added because the TRX300
semantic ground-truth inventory (Section 6) explicitly includes fuses,
and Definition D's own example list names Fuse as a canonical
`ElectricalComponent`. **No keyword-table entry was added to
`SymbolFamilyRecognizer`** — nothing in production currently produces
`SymbolFamily::Fuse`; it exists purely as taxonomy so a resolved fuse
identity has somewhere to be represented, and so this AP's Test 6 can
exercise the promotion rule against a genuine protection-function family
rather than an unrelated stand-in. Four exhaustive `switch (family)`
statements in the existing exporters (`structured_svg_exporter.cpp`,
`topology_exporter.cpp`, `symbol_renderer.cpp`,
`symbol_family_recognizer.cpp`'s own `family_name` helper) and one
counting switch (`extraction_audit.cpp`'s `SymbolFamilyCoverage` loop)
required a mechanical one-line addition each to avoid a new `-Wswitch`
warning; `symbol_renderer.cpp` maps `Fuse` to the existing generic
`UnresolvedRenderer` fallback (no dedicated `FuseRenderer` — out of
scope for a semantic-boundary AP).

## 5. Resolution rule (`ElectricalComponentResolver`)

For every `ComponentCandidate`, exactly one `ElectricalComponent` record
is produced:

1. `DiagramFurniture` kind → **Rejected** (`DiagramFurniture`).
2. `ChassisGround` kind → **Rejected** (`ChassisGroundReference`),
   regardless of terminal or family evidence (Definition F).
3. Owned by any `ConnectorCandidate` → **Rejected**
   (`ConnectorInterface`), regardless of terminal or family evidence
   (Definition E).
4. Otherwise: **Resolved** only if BOTH (a) at least one
   `EndpointCandidate` of kind `ComponentTerminal` is attributed to it,
   AND (b) a `SymbolFamilyResolution` for it has
   `status == Resolved` and `family` is neither `Unknown` nor `Ground`.
   Otherwise **Unresolved** (family/evidence preserved for
   explainability when partially available, per the never-guess rule —
   never promoted on partial evidence).

This is a pure, deterministic, read-only function of already-established
evidence: it never mutates a `ComponentCandidate`, `SymbolFamilyResolution`,
`EndpointCandidate`, or `ConnectorCandidate`, and never touches
topology/wires/electrical nets (verified in Section 9).

## 6. TRX300 ground-truth target

Manual engineering review of the canonical TRX300 diagram establishes a
working semantic ground-truth target of **24** actual electrical
components/modules. This is a validation/reference datum, **not** a
runtime constraint — no production code asserts `component_count == 24`,
and none should until a dedicated semantic-recognition stage exists with
its own independently-justified evidence for each of the 24.

**Revision history of this count**: the initial manual count was **22**.
Two fuses were subsequently recognized as having been omitted from that
count. The corrected working target is **24**, including those two
fuses. The specific names/identities of the 24 objects are not asserted
by this AP — they are not yet established from either the canonical
diagram or existing repository evidence at the level of rigor this
project's never-guess rule requires, and inventing them here would
violate that rule.

## 7. Reporting changes

`artifacts/audit/extraction_audit.json` gained a new top-level
`electrical_components` object (existing fields untouched, so this is
purely additive and does not break existing JSON compatibility):

```json
"electrical_components": {
  "component_candidates": 81,
  "resolved_electrical_components": 0,
  "unresolved_component_candidates": 28,
  "rejected_component_candidates": 53,
  "rejected_by_reason": {
    "diagram_furniture": 47,
    "chassis_ground_reference": 6,
    "connector_interface": 0
  }
}
```

`component_candidates` (= `audit.shapes` =
`model.component_candidates.size()`) is explicitly kept alongside the new
fields specifically so no reader can see "81" in isolation and read it as
an electrical-component count. `topology.json` gained a new
`electrical_components` array (one entry per `ComponentCandidate`, full
per-object detail: family, status, rejection_reason, confidence,
terminal_endpoint_ids, evidence_ids) — the same additive-array pattern
already used for `symbol_family_evidence`/`symbol_family_resolutions`.

**`resolved_electrical_components: 0` is not a bug and not evidence of
incomplete implementation reporting "not yet calculated."** It is the
correct, non-fabricated result of the resolution rule (Section 5) given
current evidence: `SymbolFamilyResolution` presently resolves only
`Ground` on TRX300 (Section 3), and `Ground` is categorically excluded
from `ElectricalComponent` identity (Definition F). Zero components
currently have both a qualifying non-Ground family AND a terminal, so
zero are `Resolved`. This is exactly what "never guess" produces when a
downstream semantic-recognition stage (label/OCR-driven family
resolution beyond the current Ground/label-keyword rules) does not yet
exist for the remaining 28 real candidates.

## 8. Audit of the current 81 `ComponentCandidate`s

Diagnostic only — no production behavior was changed because of this
audit. Computed directly from the canonical unscoped extraction combined
with `AP-DIAG-AUDIT-007`'s already-established false-positive findings:

| Category | Count | Basis |
|---|---:|---|
| F. Diagram furniture (rejected, not a component) | 47 | `ComponentCandidateKind::DiagramFurniture` |
| D. Ground/reference (rejected, not a component/module) | 6 | `ComponentCandidateKind::ChassisGround`, all 6 genuine (AP-DIAG-AUDIT-006/007) |
| H. Confirmed false-positive geometric candidate | 2 | `c2335cc575d107b1`, `d1aefcaca5503ad9` — AP-DIAG-AUDIT-004/007, annotation-leader dot glyphs, own 0 `SymbolPrimitive`s and 0 endpoints |
| I. Unresolved (real candidate, insufficient evidence) | 26 | 24 `circular_symbol` (26 total minus the 2 confirmed false positives above) + 2 `enclosure`; includes 2 visually-confirmed-genuine repeated ring symbols and up to ~19 requiring further manual/forensic review per AP-DIAG-AUDIT-007 §7/23 |
| A. Definite electrical component/module (resolved) | 0 | none currently qualify (Section 7) |
| C. Connector/interface | 0 | `connector_candidates` is 0/0 on TRX300 (standing limitation, unrelated to this AP) |
| **Total** | **81** | matches `model.component_candidates.size()` exactly |

Category B (probable electrical component/module — some evidence but not
yet a full Resolved case), E (symbol primitive/group with no clear
component identity), and G (housing/boundary geometry) were not separated
further from I in this pass; AP-DIAG-AUDIT-007's Section 7 already
performed targeted visual review on a sample of I and found a mix (some
clearly genuine repeated engineering symbols, some ambiguous
junction-dot/crossing-hop marks) — that finding is not repeated here, and
is referenced rather than duplicated. The 19 items AP-DIAG-AUDIT-007
marked "requires further manual review" retain that status; this AP does
not attempt to resolve them, per its own explicit scope limit (Section
13 of the governing task: "Do NOT attempt full semantic recognition of
all 24 TRX300 objects in this task").

## 9. Regression (before/after)

Clean Release build, assertions active, isolated `/tmp` build directory:

| Metric | Before | After |
|---|---:|---:|
| Tests passing | 60/60 | 61/61 (1 new: `dx-wire-test-electrical-component-resolver`) |
| Compiler warnings | 8 | 8 (0 new) |
| Physical wires (unscoped) | 37 | 37 |
| Electrical nets (unscoped) | 12 | 12 |
| Ground endpoints resolved | 6/6 | 6/6 |
| Validation errors | 0 | 0 |
| Validation warnings (unscoped) | 34 | 34 |
| `component_candidates`/`symbol_primitives`/`nodes`/`edges`/`endpoint_candidates` | unchanged | unchanged (byte-identical) |

No production extraction metric changed. `ElectricalComponentResolver`
runs after `SymbolFamilyRecognizer` and before
`PhysicalWireIdentityReconstructor` in `ExtractionPipeline::run()`,
consuming only already-computed collections and writing only the new
`model.electrical_components` field — nothing it reads or produces feeds
back into topology, wire, or net reconstruction.

## 10. Determinism

2 unscoped + 2 scoped extractions were run against the rebuilt pipeline.
`topology.json` and `extraction_audit.json` are deep-equal (`==`) across
both pairs. `electrical_components` ordering is deterministic
(`std::sort` by `component_candidate_id`, mirroring
`SymbolFamilyRecognizer`'s own resolution ordering), and every id is
content-addressed (`stable_id`), so no run-to-run variation exists beyond
the documented absence of any timestamp field in either artifact.

Scoped result: 34 `ComponentCandidate`s → 34 `ElectricalComponent`
records (0 resolved, 28 unresolved, 6 rejected — all `ChassisGroundReference`,
since furniture is excluded by scope before this stage even runs).
Unscoped: 81 → 81 (0 resolved, 28 unresolved, 53 rejected — 47
`DiagramFurniture` + 6 `ChassisGroundReference`). The `unresolved` count
(28) is identical in both scopes, as expected — scope exclusion only
removes furniture, never a "real" candidate.

## 11. Tests

`tests/test_electrical_component_resolver.cpp`, 61st test
(`dx-wire-test-electrical-component-resolver`), covers all 8 required
invariants:

1. A geometric candidate (no evidence at all) does not automatically
   become `Resolved`.
2. A candidate with a terminal but no family evidence remains
   `Unresolved`.
3. A candidate with family evidence but no terminal remains
   `Unresolved` (plus its positive counterpart: both present →
   `Resolved`).
4. A `ConnectorCandidate`-owned component is `Rejected` even with
   qualifying terminal + family evidence.
5. A `ChassisGround` component is `Rejected` even with a resolved
   `Ground` family + terminal (plus: `Ground` family on a
   *non*-`ChassisGround`-shaped candidate also never promotes it).
6. A `Fuse`-family, two-terminal candidate is `Resolved` (protection
   function).
7. `ComponentCandidate` count may exceed resolved `ElectricalComponent`
   count in a mixed batch.
8. Determinism: identical input → identical ids/ordering/status across
   repeated calls.

## 12. Explicit non-changes

No change to: `MorphologyWireDetector`, `ShapeDetector`,
`GroundApproachConductorRecovery`, `TerminalLocationDetector`,
`TerminalRecognizer`, `ConductorBoundaryResolver`,
`EndpointSemanticReconstructor`, `PhysicalWireIdentityReconstructor`,
`WireSemanticResolver`, `ElectricalNetResolver`,
`SymbolFamilyRecognizer`'s recognition logic (only its exhaustive
`switch` statements gained a mechanical `Fuse` case; its keyword table
and evidence rules are untouched), extraction scope, OCR/text
recognition, or any AI/LLM recognition path. No existing
`ComponentCandidate` record was deleted, and no image-processing
threshold was tuned in service of the TRX300 count.

## 13. Next AP

Per this AP's own scope limit, full semantic recognition of the 24
TRX300 objects is deliberately out of scope. The natural next AP is a
dedicated semantic component resolver that raises
`resolved_electrical_components` above 0 using: symbol evidence, OCR'd
labels/text, terminal topology, electrical function, connector
relationships, and known diagram conventions — while preserving the
never-guess rule this AP's `ElectricalComponentResolver` already
enforces structurally.

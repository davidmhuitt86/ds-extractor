# AP-WIRE-026A — Component Symbol-Family Recognition

## Purpose

Close the gap AP-WIRE-026's reference-image analysis named explicitly:
no stage between AP-WIRE-021 and AP-WIRE-026 ever establishes *engineering
symbol-family identity* (lamp, switch, relay, motor, diode, alternator,
battery, solenoid, coil, ground) - only a coarse geometric bucket
(`Enclosure`/`CircularSymbol`/`ChassisGround`/`PrimitiveSymbol`).

## Source material inspected before implementation

Per the task's requirement to inspect actual current source rather than
assume layout: `include/eke_dx_wire/core/model.hpp` (full model),
`src/topology/component_symbol_recognizer.cpp` (confirmed
`ComponentSymbolRecognition` only ever carries the geometric bucket
forward, never assigns family identity - `GeometricallyClassified` vs
`Recognized` status exists precisely because no stage produces
`Recognized`), `include/eke_dx_wire/image/symbol_geometry_extractor.hpp`
and its primitive kinds, `src/topology/terminal_recognizer.cpp`,
`include/eke_dx_wire/diagram/engineering_diagram*.hpp`,
`src/topology/component_identity_resolver.cpp`/
`component_identity_registry.cpp` (confirmed canonical identity is
text-label-driven, separate from geometry), and
`include/eke_dx_wire/topology/text_recognition_provider.hpp` (the
existing injectable-provider pattern this AP mirrors for symbol
recognition). No existing abstraction already does what this AP needs -
`ComponentSymbolRecognition` is explicitly documented as *not* symbol
identity, and nothing else in the codebase assigns symbol-family
identity at all.

## Model

```
ComponentCandidate
      |
      +-- ComponentSymbolGeometry (AP-WIRE-023, referenced by id)
      +-- ComponentIdentityCanonicalization (AP-WIRE-018, referenced by id)
      |
      v
SymbolFamilyRecognizer
      |
      v
SymbolFamilyResolution   (1:1 per real component, referenced by
                           EngineeringDiagram.DiagramComponent
                           .symbol_family_resolution_id - never inlined)
      |
      +-- SymbolFamilyEvidence[] (referenced by id, one record per
                                   contributing evidence item)
```

`SymbolFamilyResolution` and `SymbolFamilyEvidence` live in `WireModel`
(`core/model.hpp`), not in `EngineeringDiagram` - per the established
principle "earlier AP owns the engineering fact, later AP
composes/references it." `EngineeringDiagram` only gained one new
reference field (`DiagramComponent.symbol_family_resolution_id`) plus
two new independently re-derived validation checks. No duplicate parallel
representation was created.

## Taxonomy

`Ground, Lamp, Switch, Relay, Motor, Diode, Alternator, Battery,
Solenoid, Coil, Unknown`. Deliberately not exhaustive of everything
visible in the reference image (no `Fuse`, `Sensor`, `Connector`-as-symbol,
compound multi-symbol assemblies, etc.) - only families with a currently
definable, evidence-backed rule were included, per the task's explicit
instruction to avoid a "blind classifier for all of them." Adding a
family with no rule would be exactly the "invented taxonomy" the task
warns against.

## Evidence rules (exhaustive - these are the only two)

### Rule 1: purpose-built geometric classification (sufficient alone)

`ComponentCandidateKind::ChassisGround` → `SymbolFamily::Ground`,
confidence `High`.

Justification: `ShapeDetector`'s ground detector
(`ground_min_bars`/`ground_max_bars`/`ground_min_bar_spacing`/
`ground_width_ratio_tolerance` etc., `include/eke_dx_wire/image/shape_detector.hpp`)
is a **purpose-built pattern matcher for the ground symbol specifically**
(parallel bars of decreasing width) - it is categorically different from
"a circle was detected and circles sometimes look like lamps." No other
`ComponentCandidateKind` has an equivalently specific upstream detector,
which is exactly why no other family has a geometry-only rule.

### Rule 2: label keyword + compatible geometry (both required, neither alone)

A keyword match against **already-resolved** identity text
(`ComponentCandidate.semantic_labels`, or
`ComponentIdentityCanonicalization.canonical_name` **only** when
`ComponentIdentityCanonicalizationStatus::Resolved`) **AND** a
geometrically compatible `ComponentCandidateKind` for that family (see
the table in `src/topology/symbol_family_recognizer.cpp`'s
`label_rules()`). Confidence `Medium` (weaker than the purpose-built
rule, since it depends on external recognition evidence quality).

Label alone is explicitly rejected (test case: `"STARTER MOTOR"` label on
a `PrimitiveSymbol`-kind component, which is not in Motor's compatible-
kinds set, does not resolve) - this is the task's explicit "do not infer
a component type solely from its label" requirement enforced in code,
not just documentation.

### Rule 3 (evidence source, not a standalone rule): provider observations

A `SymbolRecognitionObservation` from an injected `SymbolRecognitionProvider`
contributes evidence but **a single such observation can never resolve a
family by itself** - see "Confidence and corroboration" below. This keeps
the door open for a future vision-based recognizer without letting one
unverified guess become authoritative.

### What was deliberately NOT made a rule

Raw `SymbolPrimitiveKind` counts/arrangement (e.g. "two `TerminalLead`
primitives → `Switch`") were considered and rejected: AP-WIRE-023's own
AAR found that most primitive geometry on TRX300 is `Unknown`-classified
and that even `TerminalLead` is "a shape heuristic... not electrical-lead
recognition" (AP-WIRE-023 AAR, Limitations). Treating primitive counts
alone as symbol-family evidence would be exactly the "visually similar
therefore same identity" reasoning the task prohibits. If a future AP
establishes a more specific primitive-arrangement detector (analogous to
the ground-bar detector), that would become a new Rule-1-style entry, not
a loosening of Rule 2.

## Confidence and corroboration

Per family, evidence is bucketed by whether it is "strong" (any
`PurposeBuiltGeometricClassification` or `LabelKeywordWithCompatibleGeometry`
item) or "weak" (only `ProviderObservation` items). A family needs either
one strong item, or **two or more** independent provider observations, to
be eligible for resolution at all ("provider-corroborated"). This is the
concrete mechanism behind "a high-confidence unsupported guess is still
wrong" (task §CONFIDENCE): `ConfidenceClass::High` on a lone provider
observation does not bypass the corroboration requirement.

- **Exactly one** family eligible → `Resolved`, confidence `High` (if it
  has purpose-built evidence) or `Medium` (label-keyword only).
- **Two or more** families eligible → `Conflicted`, `family = Unknown`,
  confidence `Unresolved`, `evidence_ids` lists every contributing item
  from every conflicting family (fully explainable, never a silent pick).
- **Zero** families eligible → `Unresolved`. If some evidence exists but
  didn't clear the bar (e.g. one lone provider guess), `evidence_ids`
  still lists it - this is the AAR's required distinction between
  "recognized because evidence supports it" (Resolved) and "recognizable
  in principle but evidence is currently insufficient" (Unresolved with
  non-empty `evidence_ids`) vs. "no signal at all" (Unresolved with empty
  `evidence_ids`).

## Vision/AI recognition boundary

`SymbolRecognitionProvider` (`include/eke_dx_wire/topology/symbol_recognition_provider.hpp`)
mirrors `TextRecognitionProvider` exactly: an abstract `recognize()` +
`provider_id()`, with `NullSymbolRecognitionProvider` as the pipeline
default. `ExtractionConfig.symbol_recognition_provider` is optional and
defaults to null - the deterministic extraction path and every test in
this AP run with zero network dependency. No vision-based implementation
was written in this AP (none is needed yet - the corroboration rule above
means a single provider without independent geometric/label backing
cannot resolve anything by itself, so a real provider implementation
would need to be validated against real observations before it could
change any TRX300 result).

## Determinism

`SymbolFamilyEvidence.id`/`SymbolFamilyResolution.id` use the existing
`stable_id()` content hash, keyed on component id + family + source -
never insertion order, pointers, or timestamps. Results are sorted by
`component_id`/`id` before returning. Verified: two independent fresh
TRX300 extractions produced byte-identical `engineering_diagram.json`
and `topology.json` (see the AAR).

## Relationship to component identity, terminal recognition, AP-WIRE-026

- **Component identity**: `SymbolFamily::Motor` never implies
  `ComponentIdentity = "Starter Motor"` and vice versa - they are
  separate resolutions over separate evidence, reinforced only in the
  sense that a Resolved canonical name can *feed into* Rule 2's keyword
  match; the two are never collapsed into one field.
- **Terminal recognition**: `SymbolFamilyRecognizer`'s signature accepts
  no `EndpointCandidate`/`TerminalCandidate` input at all - it is
  structurally impossible for it to create or reference a terminal.
- **AP-WIRE-026**: `EngineeringDiagram` composes this AP's output by
  reference only (`symbol_family_resolution_id`), exactly like every
  other cross-AP relationship in that model.

## Export

`topology.json` gains `symbol_family_evidence[]` and
`symbol_family_resolutions[]`. `extraction_audit.json` gains a
`symbol_families` block (resolved/unresolved/conflicted totals + a
per-family resolved breakdown). `engineering_diagram.json`'s component
objects gain `symbol_family_resolution_id`. No second/competing artifact
format was introduced; all three use the existing hand-rolled JSON writer
conventions already in each file.

## Review artifact

No new PNG layer was added. Symbol-family identity is a per-component
classification already covered by the existing `03_symbols.png`/
`10_component_bounds.png` layers' bounding-box view plus the new
`engineering_diagram.json` export for structural inspection; a dedicated
visual layer would not expose anything materially new before AP-WIRE-027
exists to actually render distinct symbol glyphs per family.

## Validation

See `docs/AP-WIRE-026A_AAR.md`.

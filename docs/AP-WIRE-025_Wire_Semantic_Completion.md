# AP-WIRE-025 — Wire Semantic Completion

## Purpose

Attach defensible engineering semantics to already-reconstructed `Wire`
objects using evidence already present in the extraction model. This AP
does not change what a wire *is* (endpoint-to-endpoint, per the standing
identity rule); it adds what a wire *means*, where the model already has
the evidence to say so.

## Implementation-scope assessment (per §3, before implementation)

Inspected `core/model.hpp`, `WireModel`, `Wire`, `EndpointCandidate`,
`TerminalCandidate`, `ConnectorTerminal`, `ElectricalNet`,
`EndpointSemanticReconstruction`, `WireModelValidator`, and the pipeline
order in `extraction_pipeline.cpp`, plus the three prior AARs. Findings
that shaped scope:

- `EndpointCandidate.wire_color` / `.function_label` / `.terminal_name` /
  `.component_id` are the only per-endpoint semantic fields that reach the
  final model. `component_id` is written exactly once
  (`EndpointSemanticReconstructor`, only in its `Resolved` branch - it is
  cleared on `Conflicted`), so it is safe to read directly. `wire_color`/
  `function_label` are written by `EngineeringObjectSemanticApplier`,
  which runs *after* `EndpointSemanticReconstructor` and is architecturally
  independent of component-identity conflict (a wire-color label near a
  conductor is not evidence about which component owns the terminal), so
  gating color/function on the endpoint's component-reconstruction status
  is not appropriate.
- `EngineeringObjectSemanticApplier::assign_if_empty_or_same` silently
  keeps the first value and drops a later disagreeing one at the
  *endpoint* level - it does not record that a conflict occurred. This AP
  therefore treats `EndpointCandidate.wire_color`/`function_label` as
  "best available per-endpoint text," and does its own conflict detection
  by comparing the wire's *two* endpoints against each other (which *is*
  visible), rather than trusting endpoint-level non-conflict as proof.
- `CircuitRoleEvidence` (per-endpoint distribution role) is computed
  inside `ExtractionPipeline::run` and consumed by `ElectricalNetResolver`
  but is **not retained on `WireModel`**. Only the coarser
  `ElectricalNet.role` survives. See "Deliberately omitted" below.
- `ConnectorCandidate`/`ConnectorTerminal` are currently empty on the
  TRX300 fixture (0/0, per the AP-WIRE-022A AAR - the furniture fix
  removed the population that used to satisfy connector heuristics). The
  connector-association field is implemented for architectural
  completeness and future fixtures, not because TRX300 currently
  exercises it.

## Non-goals confirmed against source

Verified structurally, not just by convention: `WireSemanticResolver::resolve`
takes every input by `const&` and returns a new `WireSemanticResolutionArtifacts`
value - there is no path by which it can mutate `Wire`, `EndpointCandidate`,
`TopologyNode`, `TopologyEdge`, or `ElectricalNet`. It is called once, after
`model.wires`/`model.electrical_nets` are final, and its only effect is
populating the new `model.wire_semantics` vector.

## Model

```
Wire
  |
  +-- WireSemanticResolution   (1:1, id = stable_id("wire-semantic-resolution", wire.id))
        |
        +-- wire_color / wire_color_status / wire_color_confidence
        +-- function_label / function_status / function_confidence
        +-- start_component_id / start_terminal_name / start_component_status
        +-- end_component_id / end_terminal_name / end_component_status
        +-- start_connector_id / start_connector_terminal_name / start_connector_status
        +-- end_connector_id / end_connector_terminal_name / end_connector_status
        +-- electrical_net_id / electrical_net_status / electrical_net_confidence
```

`WireSemanticStatus` is `{Resolved, Unresolved, Conflicted}`, matching the
three-state convention already used throughout this codebase
(`ConnectorTerminalStatus`, `ComponentIdentityResolutionStatus`,
`EndpointSemanticReconstructionStatus`). Strength of resolved evidence is
carried separately via the existing `ConfidenceClass`, per §4's explicit
requirement not to collapse strong/weak/unresolved/conflicted into one
value.

### Evidence rules

- **wire_color / function_label**: read from the wire's own
  `start_endpoint`/`end_endpoint`. Both non-empty and equal → `Resolved`,
  `High`. Both non-empty and different → `Conflicted` (value cleared,
  never averaged or arbitrarily picked). Exactly one non-empty →
  `Resolved`, `Medium` (single-sided, weaker). Neither → `Unresolved`.
- **component/terminal association**: read directly from
  `EndpointSemanticReconstruction` for `wire.start_endpoint`/`end_endpoint`.
  A `Conflicted` reconstruction (including the AP-WIRE-024
  boundary/alignment fallback case identified in that AP's AAR) is
  reported `Conflicted` here, never treated as authoritative - this is
  the AP spec's §5 constraint, verified empirically (see AAR §6).
- **connector-terminal association**: only a `ConnectorTerminalStatus::Resolved`
  `ConnectorTerminal` is used.
- **electrical-net association**: both endpoints resolved to the *same*
  net → `Resolved`, `High`. Resolved to *different* nets → `Conflicted`
  (a genuine cross-stage inconsistency between `WireReconstructor`'s path
  and `ElectricalNetResolver`'s independent net-building, worth
  surfacing, not hiding). Exactly one endpoint net-resolved → `Resolved`,
  `Medium`. Neither → `Unresolved`.

### Deliberately omitted: source/destination distribution role

Investigated per §9. No field was added. `CircuitRoleEvidence` (the only
model of "which distribution role was independently evidenced at this
specific endpoint") is not retained on `WireModel` - it exists only as a
local variable inside `ExtractionPipeline::run`. The only persisted role
information is `ElectricalNet.role`, which is a *net*-level aggregate,
and §8 explicitly warns against copying net role onto wire ends without
justification. Adding `source_role`/`destination_role` populated from net
role would violate that guidance; adding them as always-`Unresolved`
placeholders would violate §18 ("do not add fields merely because
theoretically useful... every field must have a defensible current
producer"). Both were rejected. A future AP that first exposes
`CircuitRoleEvidence` on `WireModel` (a small, independent, well-scoped
change) would give this a real producer.

## Determinism

`WireSemanticResolution.id` is `stable_id("wire-semantic-resolution", wire.id)` -
depends only on wire identity. Results are sorted by `wire_id` before
being returned. Net-membership lookup uses `std::map` (ordered) for
`endpoint_id -> net_ids`, not an unordered container, so output does not
depend on `ElectricalNet` vector order. Verified by a dedicated
determinism test (two `resolve()` calls against identical input produce
identical `id`/status/value for every resolution).

## Export

`topology.json` gains a `wire_semantics` array (full per-wire detail).
`extraction_audit.json` gains a `wire_semantics` coverage block computed
independently from `model.wire_semantics`, following the same pattern the
audit already uses for `WireModelValidator`'s issues (re-derived from raw
data, not trusting a producer's own summary). `review_manifest.json`
gains a compact summary. No new review-image layer was added: wire
semantics are per-wire scalar/text fields, not new geometry, and the
existing `01_wires.png`/`08_labels.png` layers already show wire and
label positions; a dedicated image layer would not expose anything the
JSON export doesn't already make inspectable.

## Tests

`tests/test_wire_semantic_resolver.cpp`, 20 cases covering every item in
§15 (see the AAR for the full list) plus connector-terminal gating.

## Validation

See `docs/AP-WIRE-025_AAR.md`.

# AP-DIAG-FIX-002 — Connector Taxonomy and Representation

## 1. Existing taxonomy

Before any change, the repository was searched exhaustively (`grep -rln
"Connector\|ConnectorCandidate\|ConnectorTerminal"`) rather than assumed
to lack connector support. It does not lack it. The full model already
exists, end to end:

- `ComponentCandidateKind` (`include/eke_dx_wire/core/model.hpp`):
  `Enclosure, CircularSymbol, ChassisGround, PrimitiveSymbol,
  DiagramFurniture, Unknown`.
- `TerminalCandidateKind`: `ComponentBoundary, ConnectorBoundary,
  GroundConnection, Unknown`.
- `EndpointKind`: `GeometricConductorEnd, ComponentTerminal,
  ConnectorTerminal, Splice, Ground, ExternalConnection, Unresolved`.
- `TerminalRole`: `Unknown, ComponentTerminal, ConnectorTerminal,
  GroundTerminal, PowerSource, ExternalConnection`.
- `ConnectorCandidate { id, component_candidate_id, bounds, confidence,
  semantic_labels }` and `ConnectorTerminal { id, connector_id,
  endpoint_id, position, terminal_name, function_label, wire_color, role,
  confidence, status }`, with `ConnectorTerminalStatus { Resolved,
  Unresolved, Conflicted }` — all in `core/model.hpp`.
- `ConnectorTerminalModelBuilder` (AP-WIRE-020,
  `src/topology/connector_terminal_model.cpp`): builds
  `ConnectorCandidate`/`ConnectorTerminal` objects from any
  `TerminalCandidate` whose `kind == ConnectorBoundary`.
- `ConductorBoundaryResolver` (AP-WIRE-030): already resolves
  `connector_status` and `connector_terminal_status` per endpoint from
  `ConnectorTerminal` evidence, already detects the "multiple distinct
  connectors claim the same endpoint" conflict case, and already reports
  per-category coverage counts (`connector_resolved`,
  `connector_terminal_resolved`, etc. in `ExtractionCoverage`).

And critically, tracing the actual data flow (not assumed from names)
confirms the classification mapping already exists and is already wired
into the pipeline in two places
(`terminal_recognizer.cpp:terminal_kind()`,
`terminal_location_detector.cpp:kind_for_component()`, identical logic):

```
ComponentCandidateKind::PrimitiveSymbol -> TerminalCandidateKind::ConnectorBoundary
```

and downstream (`terminal_semantic_evidence_builder.cpp:endpoint_kind()`/
`terminal_role()`):

```
TerminalCandidateKind::ConnectorBoundary -> EndpointKind::ConnectorTerminal
                                          -> TerminalRole::ConnectorTerminal
```

which `EndpointSemanticReconstructor` then writes onto the real
`EndpointCandidate.kind`/`.terminal_role` fields whenever the evidence is
unambiguous (unchanged code, confirmed by a new test — §9).

`ComponentCandidateClassifier` already produces `PrimitiveSymbol` from
any `ShapeKind::Rectangle` region whose `ShapeRole` is not `Enclosure`
(too small to qualify as a full component housing) — already tested in
`tests/test_component_candidate_classifier.cpp` (shape-4 in that file).

## 2. Connector representation gap

AP-DIAG-AUDIT-001 stated the taxonomy was "structurally impossible to
produce... because ComponentCandidateKind has no Connector-class value
at all." That framing is imprecise: the mechanism above exists and is
correctly wired top to bottom. The real, narrower gap is empirical, not
structural: **for `samples/trx300ODG.png` specifically, zero components
are ever classified `PrimitiveSymbol`** (`shape_kinds.primitive: 0` in
every extraction to date, reconfirmed fresh in this AP — §10). Every
Rectangle-shaped region in this raster is either large enough to be
classified `Enclosure`, or is contour-classified as `Circle` rather than
`Rectangle` (confirmed directly in AP-DIAG-FIX-001's own investigation:
the indicator-lamp base and other small blobs came back `ShapeKind::Circle`,
not `ShapeKind::Rectangle`).

So the taxonomy is complete and correctly wired; it is simply unfed for
this specific source image, because no upstream shape-detection stage
currently produces the one input (`PrimitiveSymbol`) the existing chain
needs.

## 3. Source evidence

The source diagram (`samples/trx300ODG.png`) does draw connector-like
multi-pin plug glyphs (a notched trapezoid shape under the CDI Unit,
Alarm Unit, and similar assemblies, confirmed visually in
AP-DIAG-AUDIT-001 and AP-DIAG-FIX-001). However:

- `ShapeDetector` does not currently produce a distinct `ShapeRegion` for
  these notch glyphs at all — their ink is absorbed into the parent
  enclosure's own contour or is not isolated as its own Rectangle
  candidate.
- Making it do so would be a shape-detection change (new geometry
  recognition for a notch/staircase silhouette), which this AP is
  explicitly not permitted to make: section 5 of this AP's own charter
  prohibits inferring a connector "solely because it is rectangular...
  resembles a connector," and doing so without a currently-available,
  reliable geometric signal to distinguish a genuine multi-pin plug notch
  from any other small shape would be exactly that kind of guess.

**Conclusion, stated plainly and without hedging: this AP recognizes
zero connector instances in `samples/trx300ODG.png`.** No connector was
fabricated to satisfy this AP's title. The alternative — inventing a
notch-detection heuristic to manufacture a demonstrable connector — was
rejected as guessing.

## 4. New/extended model representation

**No model types were added, renamed, or changed.** Per this AP's own
instruction ("If an existing connector concept exists but is incomplete,
extend it rather than creating a duplicate concept" / "If
`EndpointKind::ConnectorTerminal` already exists, USE IT"), and having
confirmed the existing concept is not incomplete — it is complete and
unused for this specific source — no new enum value, struct, or field
was introduced. `ComponentCandidateKind::PrimitiveSymbol` remains the
one existing, evidence-based route to `Connector`/`ConnectorTerminal`
classification. A dedicated `ComponentCandidateKind::Connector` value,
separate from the more general `PrimitiveSymbol` bucket, was considered
and rejected for this AP: populating it correctly would require the same
notch-recognition capability described in §3, which does not currently
exist and was not built here.

## 5. Recognition path

Unchanged, and confirmed correct by new tests added at each stage
(§9), tracing exactly the path in §1:

```
ShapeDetector (Rectangle, non-Enclosure role)
  -> ComponentCandidateClassifier (PrimitiveSymbol)
  -> TerminalRecognizer / TerminalLocationDetector (ConnectorBoundary)
  -> TerminalSemanticEvidenceBuilder (EndpointKind::ConnectorTerminal)
  -> EndpointSemanticReconstructor (writes endpoint.kind/.terminal_role)
  -> ConnectorTerminalModelBuilder (ConnectorCandidate + ConnectorTerminal)
  -> ConductorBoundaryResolver (connector_status / connector_terminal_status)
```

No recognition logic was duplicated at a later stage; every new test
exercises the existing, single implementation of each mapping.

## 6. Terminal ownership

Unchanged, and already correctly enforced by `ConnectorTerminalModelBuilder`
(confirmed by the pre-existing `test_connector_terminal_model.cpp`, now
extended — §9): every `ConnectorTerminal` carries its owning
`connector_id`; a terminal is never independently promoted to a generic
`ComponentCandidate` or to `ChassisGround`; and nothing in the builder or
`ConductorBoundaryResolver` ever links two terminals of the same
connector to each other — each terminal is resolved strictly from its
own endpoint's evidence.

## 7. Boundary-resolution interaction

Unchanged. `ConductorBoundaryResolver` already treats a `Resolved`
`ConnectorTerminal` as authoritative evidence for that endpoint's
`connector_status`/`connector_terminal_status`, already flags a genuine
conflict when more than one distinct connector claims the same endpoint,
and already leaves `connector_status = Unresolved` when boundary evidence
exists but no `ConnectorTerminal` was ever materialized for it. All of
this pre-dates this AP and required no change.

## 8. Electrical semantics

Confirmed by direct inspection (not just by test) that `Connector`/
`ConnectorTerminal` cannot influence electrical connectivity: neither
`ElectricalNetResolver` nor `DistributionDecomposer`'s source files
reference "connector" anywhere at all
(`grep -n "connector\|Connector" src/topology/electrical_net_resolver.cpp
src/topology/distribution_decomposer.cpp` returns nothing). Net
decomposition operates purely on `TopologyNode`/`TopologyEdge`/
`ConductorSegment` connectivity; it has no code path capable of merging
two endpoints merely because they share a `component_id` or belong to the
same connector. TEST 4 (§9) demonstrates this concretely rather than
relying on the absence of code as the only evidence.

## 9. Tests

All additions are new test *cases* in existing test files/targets — no
new CTest target was created, since every required scenario belongs to a
stage that already has a dedicated test file.

- **TEST 1** (connector taxonomy exists) — already fully covered by the
  pre-existing `tests/test_connector_terminal_model.cpp` and
  `tests/test_component_candidate_classifier.cpp` (Rectangle+non-Enclosure
  -> `PrimitiveSymbol`, distinct from Enclosure/CircularSymbol/
  ChassisGround). No gap found; nothing added.
- **TEST 2** (terminal ownership) — already fully covered by
  `test_connector_terminal_model.cpp` (`terminal.connector_id ==
  connector.id`). No gap found.
- **TEST 3** (connector terminal endpoint resolves via evidence) — **gap
  found and closed**: added to `tests/test_endpoint_semantic_reconstructor.cpp`,
  the one stage with zero prior connector-related assertions. Proves
  `TerminalSemanticEvidence{endpoint_kind: ConnectorTerminal, role:
  ConnectorTerminal}` resolves the real `EndpointCandidate.kind` and
  `.terminal_role` fields correctly.
- **TEST 4** (electrical independence) — **gap found and closed**: added
  to `tests/test_electrical_net_resolver.cpp`. Two `ConnectorTerminal`
  endpoints sharing `component_id = "connector-j1"`, each on its own
  independently Ground-anchored, structurally disconnected tree, resolve
  to two separate `ElectricalNet` records — never merged.
- **TEST 5** (generic component remains generic) — **gap found and
  closed**: added to `tests/test_terminal_recognizer.cpp` (previously zero
  connector-related coverage there, despite `terminal_kind()` living in
  that file). An `Enclosure`-kind component's terminal evidence resolves
  to `ComponentBoundary`, never `ConnectorBoundary`.
- **TEST 6** (ChassisGround remains distinct) — added alongside TEST 5:
  a `ChassisGround`-kind component's terminal evidence resolves to
  `GroundConnection`, never `ConnectorBoundary`. The ground classifier
  itself was not touched or improved, per this AP's explicit scope.
- **TEST 7** (unknown remains unknown) — added alongside TEST 5/6: an
  `Unknown`-kind component produces no `TerminalCandidate` at all (not a
  guessed classification of any kind).
- **TEST 8** (determinism) — **gap found and closed**: added to
  `test_connector_terminal_model.cpp`. Two calls to
  `ConnectorTerminalModelBuilder::build()` on identical input produce
  identical connector/terminal ids, ownership, and ordering.

All new assertions run with real, active `assert()` (AP-TEST-FIX-001
`-UNDEBUG` protection, reconfirmed — §13).

## 10. TRX300 before/after

No production source file was modified in this AP, so no change was
expected — and none occurred. Fresh extraction of
`samples/trx300ODG.png`, same invocation as every prior AP:

| Metric | AP-DIAG-FIX-001 | This AP | Delta |
|---|---|---|---|
| Physical wires | 35 | 35 | 0 |
| Electrical nets | 10 | 10 | 0 |
| Validation errors | 0 | 0 | 0 |
| Validation warnings | 30 | 30 | 0 |
| `connector_candidates` | 0 | 0 | 0 |
| `connector_terminals` | 0 | 0 | 0 |
| `shape_kinds.primitive` | 0 | 0 | 0 |
| `shape_kinds.chassis_ground` | 10 | 10 | 0 |
| `net_roles.ground` | 4 | 4 | 0 |
| `symbol_primitives` | 40 | 40 | 0 |
| Endpoint kinds | geometric 172 / component_terminal 16 / ground 12 / connector_terminal 0 | identical | 0 |

`extraction_audit.json`, `output/wires.svg`, and
`artifacts/topology/topology.json` SHA-256 hashes are **byte-identical**
to the AP-DIAG-FIX-001 baseline
(`c4e99f52bedfe3e4083f4f5c5df6bd283696e1365671c7f8c5a5cd6da212432f`,
`3f89de85415273f4546adab7e2f0d2f6b8eb3edf013feacb1ffa41935a2cfd6b`,
`9eaaa9468407c965b14a2bfdc7b4de45caf648117cc818a0a071ad81dd674a19`
respectively) — the strongest possible confirmation that this AP changed
zero extraction behavior. Two fresh runs of this AP's own build are
mutually byte-identical except for `review_manifest.json`'s
`generated_at` timestamp, exactly as established by every prior AP.

## 11. Remaining connector limitations

- Zero connector instances are recognized in TRX300, for the well-
  understood, evidence-based reason in §2–§3: no `PrimitiveSymbol`-kind
  component currently exists in this raster's shape-detection output.
- The existing `PrimitiveSymbol -> ConnectorBoundary` mapping conflates
  "small, non-enclosure rectangle" with "connector" — any future shape-
  detection improvement that starts producing `PrimitiveSymbol`
  components (e.g. for a diode or resistor glyph, not a connector) would
  currently also route through this same connector-recognition path.
  This pre-existing conflation was not introduced or worsened by this AP,
  and correcting it (e.g. splitting `PrimitiveSymbol` into a genuinely
  distinct `Connector` kind backed by real, connector-specific evidence)
  requires the same not-yet-existing recognition capability discussed in
  §3–§4, and is left for a future AP once that capability exists.
- No connector pin numbering, internal pinout, or pin-to-pin
  relationship is inferred anywhere in this pipeline — confirmed
  structurally true both before and after this AP (§8), and unaffected
  by it.

## 12. Relationship to the future ground-classification AP

AP-DIAG-AUDIT-001's separate CRITICAL finding — a rectifier diode
misclassified as `ChassisGround` — was explicitly out of scope here and
was not touched. As direct confirmation:

- `shape_kinds.chassis_ground` remains 10, unchanged, before and after
  this AP.
- `net_roles.ground` remains 4, unchanged.
- The `ChassisGround -> GroundConnection` mapping is unchanged (and is
  now explicitly regression-tested by this AP's new TEST 6, which locks
  in that a ground-classified component's terminal evidence is
  `GroundConnection`, never `ConnectorBoundary` — a property that
  incidentally also protects the future ground-classification fix from
  ever accidentally producing a connector as a side effect, though no
  such fix was made here).
- The `PrimitiveSymbol`/`Connector` pathway and the `ChassisGround`
  pathway are structurally independent branches of the same `switch`
  statement in both `terminal_kind()`/`kind_for_component()` — a future
  fix to ChassisGround misclassification (e.g. tightening the gate that
  currently lets a diode glyph through) cannot, by construction, route
  anything into `ConnectorBoundary`, and vice versa.

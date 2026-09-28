# AP-DIAG-DESIGN-002 — Connector Semantic Materialization Boundary

## Status

DESIGN COMPLETE — production implementation deferred to AP-DIAG-IMPL-002.

Starting commit: `70687ad5e7ea77fbd5234bc943fc14bdc43e5a7a`

This AP resolves the implementation contradiction identified by
AP-DIAG-IMPL-001-PRECHECK without changing production behavior.

## Source-grounded constraints

The TRX300 source-grounded connector inventory is 12 connector locations and
33 visible connector-side pins. Connector geometry is not represented by the
existing ComponentCandidate taxonomy. The approved AP-DIAG-DESIGN-001 decision
is that ConnectorBody geometry bypasses ComponentCandidate.

The existing production connector path is component-centric:

`ComponentCandidate -> TerminalCandidate(ConnectorBoundary) -> ConnectorTerminalModelBuilder -> ConnectorCandidate/ConnectorTerminal`

That path cannot accept a ConnectorBody-native connector without either a
synthetic ComponentCandidate or an overloaded meaning for
TerminalCandidate::component_candidate_id. Both are rejected.

## Current implementation trace

### ConnectorTerminalModelBuilder

The existing builder:

1. accepts ComponentCandidate, TerminalCandidate, and EndpointCandidate;
2. ignores ConnectorBoundary candidates with an empty
   component_candidate_id;
3. looks up the referenced ComponentCandidate;
4. creates ConnectorCandidate from that component;
5. creates ConnectorTerminal from the candidate's endpoint.

Therefore a ConnectorBody that bypasses ComponentCandidate cannot enter this
API.

### TerminalLocationDetector / TerminalRecognizer

Both existing paths derive TerminalCandidateKind from
ComponentCandidateKind. ConnectorBoundary currently corresponds to
PrimitiveSymbol.

A ConnectorPin is deliberately not a ComponentCandidate. Therefore it needs a
connector-native semantic association path rather than being routed through
the component-kind switch.

### ConductorBoundaryResolver

The resolver already consumes ConnectorTerminal as authoritative connector
boundary evidence. It does not need to discover connector identity from
geometry and must remain unchanged.

This makes it the correct downstream consumer once connector-terminal
materialization is complete.

## Architectural decision

### Selected: connector-native semantic materialization boundary

Do NOT create a second independent terminal model.

Do NOT overload ComponentCandidate semantics.

Do NOT add ComponentCandidateKind::Connector.

Instead, extend the existing connector-model boundary with a connector-native
input path while preserving the existing component-based build path unchanged.

The preferred implementation is an additive connector-native overload on
ConnectorTerminalModelBuilder, backed by a small explicit association-evidence
type.

Conceptually:

`ConnectorBody geometry -> ConnectorCandidate`

`ConnectorCandidate + ConnectorPin + independently established EndpointCandidate -> ConnectorTerminalAssociation`

`ConnectorTerminalAssociation + EndpointCandidate -> ConnectorTerminal`

The existing legacy path remains:

`ComponentCandidate -> TerminalCandidate -> ConnectorTerminal`

Both paths converge only at ConnectorModelArtifacts.

This keeps ConnectorTerminalModelBuilder as the single owner of
ConnectorCandidate/ConnectorTerminal materialization while removing its
historical assumption that every connector originates from a component.

## Why not a separate ConnectorSemanticMaterializer?

A separate materializer is technically valid, but it would duplicate ownership
of ConnectorCandidate/ConnectorTerminal construction and create two independent
implementations of status, identity, sorting, and serialization assumptions.

The additive builder boundary is smaller and preserves the existing public
ownership of connector model construction.

The legacy overload must remain behaviorally unchanged.

## Object ownership

| Object | Represents | Does not represent | Producer | Independent evidence |
|---|---|---|---|---|
| ConnectorBody ShapeRegion | recognized connector-body geometry | pin, terminal, wire, net | ConnectorGeometryDetector | geometry only |
| ConnectorCandidate | connector-body semantic candidate | component identity, electrical net | connector semantic materialization | ConnectorBody evidence |
| ConnectorPin | observed pin/contact location | Wire endpoint, electrical terminal | connector semantic stage | connector geometry + conductor interaction |
| TerminalCandidate | terminal-association candidate attached to an existing endpoint | physical connector body | terminal association stage | existing EndpointCandidate + connector pin evidence |
| EndpointCandidate | conductor-boundary location | connector identity by itself | existing topology/endpoint stages | conductor/topology evidence |
| ConnectorTerminal | connector pin associated with a specific endpoint | Wire identity, electrical equivalence | ConnectorTerminalModelBuilder | ConnectorPin + EndpointCandidate + association evidence |
| Wire | endpoint-to-endpoint physical conductor identity | connector geometry identity | existing Wire reconstruction | existing boundary/topology evidence |
| ElectricalNet | electrical connectivity grouping | physical wire identity | ElectricalNetResolver | existing topology/semantic evidence |

## ConnectorPin schema

The prior design called this a five-field structure but listed six fields.
The corrected minimum schema is six fields:

- `id`
- `connector_id`
- `position`
- `ordinal`
- `conductor_crossing_evidence_ids`
- `confidence`

No endpoint_id is allowed on ConnectorPin. A pin is deliberately pre-terminal.

`ordinal` is a local geometric ordering observation only. It must never be
used to infer mating, entry/exit pairing, wire identity, or electrical
equivalence.

ConnectorPin does not require a resolution status. It is an observation with
confidence. A pin may exist without any TerminalCandidate.

## Conductor crossing evidence

Add a connector-native evidence record with the minimum fields:

- id
- connector_id
- pin_id
- conductor_segment_id
- crossing_point
- confidence
- source_region/provenance

One record means that a specific ConductorSegment was independently observed
to interact with a specific connector pin region.

Crossing evidence establishes conductor interaction only. It does not establish
terminal identity, Wire identity, or electrical equivalence.

A crossing record must never be created merely because a conductor is close to
the connector bounding box.

## Connector terminal association

Because TerminalCandidate currently has a field explicitly named
component_candidate_id, it must not be repurposed to carry connector IDs.

Introduce a connector-native association evidence object rather than
overloading TerminalCandidate.

Minimum conceptual fields:

- id
- connector_id
- pin_id
- endpoint_id
- evidence_ids
- confidence
- status

The association object is created only after an existing EndpointCandidate is
available.

Rules:

- zero qualifying endpoints -> no resolved association;
- exactly one independently supported endpoint -> Resolved association;
- multiple equally plausible endpoints -> Conflicted;
- weak/insufficient evidence -> Unresolved;
- no distance-only winner selection.

This association object is connector semantic evidence, not a replacement for
TerminalCandidate.

## TerminalCandidate relationship

The existing TerminalCandidate model remains unchanged.

For the connector-native path, a connector association can coexist with the
existing TerminalCandidate taxonomy without pretending the connector is a
ComponentCandidate.

If an actual TerminalCandidate is needed for downstream
ConductorBoundaryResolver compatibility, the implementation must establish
its connector ownership through the connector-native association path rather
than through component_candidate_id.

No existing component-terminal candidate may be reclassified.

## ConnectorTerminal materialization

ConnectorTerminal is produced from:

1. ConnectorCandidate;
2. ConnectorPin;
3. connector-native terminal association;
4. independently existing EndpointCandidate.

The builder copies endpoint semantic fields only from the referenced endpoint.
It must not invent terminal names, function labels, wire colors, or roles.

Status:

- Resolved only when the association is independently established;
- Unresolved when the connector/pin exists but terminal association evidence
  is insufficient;
- Conflicted when competing connector or endpoint associations remain.

No status is upgraded merely because a connector body was detected.

## ConnectorCandidate identity

The existing `component_candidate_id` field must NOT be overloaded with a
ConnectorBody ID.

For connector-native candidates it should remain empty because there is no
ComponentCandidate association.

The implementation must therefore update connector coverage/serialization
logic that currently interprets a missing component association as an invalid
connector. That is a consumer correction, not a semantic redefinition of
ComponentCandidate.

Do not rename the existing field in this AP.

A future schema revision may introduce an explicit connector-body reference,
but it is not required to implement this boundary safely.

## Pipeline ordering

The eventual implementation order is:

1. source scoping
2. image normalization
3. existing ShapeDetector
4. ConnectorGeometryDetector
5. Conductor normalization/evidence extraction
6. topology reconstruction
7. existing endpoint creation
8. connector-conductor crossing evidence
9. ConnectorPin creation
10. connector-native terminal association
11. ConnectorTerminalModelBuilder connector-native path
12. ConductorBoundaryResolver
13. existing Wire identity reconstruction
14. ElectricalNetResolver

Connector recognition never creates topology.

Connector recognition must not run terminal association until independent
EndpointCandidates exist.

## Resolved / Unresolved / Conflicted

### ConnectorCandidate

Resolved means connector geometry has sufficient source evidence to establish a
connector-body candidate.

Unresolved means geometry is suggestive but required independent semantic
evidence is incomplete.

Conflicted means independent evidence produces incompatible connector claims.

### ConnectorPin

Pin is observational. It has confidence but no electrical resolution status.

### ConnectorTerminal

Resolved requires one connector, one pin, and one independently supported
EndpointCandidate with no competing association.

Unresolved means the pin exists but terminal evidence is incomplete.

Conflicted means two or more incompatible connector/endpoint associations
remain plausible.

No winner may be selected by geometric preference.

## Serialization

The exported model must preserve:

- ConnectorCandidate identity and bounds;
- ConnectorPin identity, connector ownership, position, ordinal, confidence;
- crossing evidence IDs and conductor segment references;
- connector association evidence;
- ConnectorTerminal endpoint and status.

The exported evidence must answer:

1. Why was this geometry considered a connector?
2. Why was this location considered a pin?
3. Which conductor was observed at the pin?
4. Which endpoint was associated?
5. Why was the terminal resolved, unresolved, or conflicted?

Do not duplicate the same geometry in multiple records.

## Audit and coverage

Implementation should add connector-native counts only where they are not
already represented:

- ConnectorBody candidates
- ConnectorPins
- crossing evidence records
- connector associations
- ConnectorTerminals by status

Existing ConnectorCandidate and ConnectorTerminal counts remain authoritative
for their respective model objects.

Coverage logic must stop treating an empty ComponentCandidate association as
automatically invalid for a connector-native ConnectorCandidate.

## Negative cases

The implementation must prove:

1. connector geometry without conductor interaction is not a resolved connector;
2. conductor interaction without connector geometry is not a connector;
3. ConnectorBody without EndpointCandidate remains unresolved;
4. equal endpoint candidates remain unresolved/conflicted;
5. diagram furniture cannot become a connector solely from shape;
6. connector geometry cannot create ChassisGround;
7. ConnectorPin cannot create Wire;
8. connector recognition cannot alter ElectricalNetResolver solely by
   recognizing geometry;
9. component-terminal behavior is byte-for-byte unchanged for the existing
   baseline.

## Deterministic identity

All new identities must use stable_id-based namespaces and source-derived
inputs.

No array index may participate in identity.

Suggested namespaces:

- connector-body
- connector-pin
- connector-conductor-crossing
- connector-terminal-association

Inputs must be normalized and sorted before identity materialization where
order could otherwise affect IDs.

## Implementation boundary for AP-DIAG-IMPL-002

### Add

- ConnectorGeometryDetector
- ConnectorPin model
- connector-conductor crossing evidence model
- connector-terminal association evidence model
- connector-native ConnectorTerminalModelBuilder path
- dedicated tests

### Modify

- extraction pipeline integration
- connector serialization
- connector coverage/audit where required by the new native path
- terminal association APIs only where necessary

### Must remain behaviorally unchanged

- ShapeDetector's existing three detector bodies
- all existing ShapeDetector thresholds
- ComponentCandidateKind taxonomy
- existing component-terminal recognition
- PhysicalWireIdentityReconstructor
- ConductorBoundaryResolver semantics
- ElectricalNetResolver
- WireSemanticResolver
- Wire model
- ElectricalNet model
- ground detection

### Explicitly forbidden

- synthetic ComponentCandidate for connectors
- ComponentCandidateKind::Connector
- overloading component_candidate_id with connector IDs
- connector mating inference
- entry/exit pairing
- electrical equivalence inference
- connector-created Wire boundaries
- connector-created topology
- threshold tuning to force recognition

## Regression invariants

Implementation must preserve unless a directly demonstrated connector dependency
exists:

- 37 physical Wires
- 12 ElectricalNets
- 81 ComponentCandidates
- 0 resolved ComponentCandidates
- 28 unresolved ComponentCandidates
- 53 rejected ComponentCandidates
- 6/6 genuine ChassisGround endpoint references
- 0 validation errors
- 34 runtime warnings
- existing component-terminal results
- existing topology and Wire identity

New connector-native objects are additive and must not inflate the
ComponentCandidate count.

## Deferred work

Not part of this AP or AP-DIAG-IMPL-002:

- connector entry/exit pairing
- connector mating
- cross-connector convention inference
- electrical equivalence across connector halves
- connector pin numbering inference
- text/OCR-based connector labels
- automatic connector-name resolution
- connector-specific wire-color interpretation

## Design gates

1. Connector bypasses ComponentCandidate without synthetic objects: PASS
2. Existing component-terminal path remains intact: PASS
3. ConnectorPin remains distinct from EndpointCandidate: PASS
4. Terminal association has independent endpoint evidence: PASS
5. No connector evidence can create Wire/topology/net: PASS
6. Unresolved and Conflicted states remain explicit: PASS
7. Existing ConnectorTerminal downstream semantics remain usable: PASS
8. Existing ConductorBoundaryResolver can consume resulting ConnectorTerminal
   objects without semantic modification: PASS
9. ConnectorCandidate identity is not falsified through component_candidate_id:
   PASS
10. Deterministic provenance is preserved: PASS

## Final recommendation

Proceed to AP-DIAG-IMPL-002.

The implementation should use an additive connector-native materialization path
inside the existing ConnectorTerminalModelBuilder ownership boundary, with an
explicit connector-terminal association evidence type. The legacy
ComponentCandidate-based path must remain unchanged.

This is the smallest architecture that resolves the contradiction without
creating a synthetic component taxonomy or overloading an existing field with
a false semantic meaning.

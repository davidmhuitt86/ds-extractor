# AP-DIAG-FIX-009 — Attached Connector Semantics

## Status

IMPLEMENTATION IN PROGRESS — architecture correction applied to the connector-native extraction path.

## Source correction

The TRX300 connector symbol family is the notched/interlocking connector-body geometry shown in the source diagram.

The connector count used for the current extraction work is **13 physical connector instances**. The previous working count of 6 was incorrect.

The previous implementation also carried an invalid universal assumption:

> every connector has a conductor entry and a conductor exit through the connector body.

That is true for inline/pass-through connectors, but it is not true for every connector in the TRX300 diagram. The CDI unit, Alarm unit, and rectifier-related assembly demonstrate connector bodies physically attached to modules/components where one or more conductors terminate at the connector/component boundary rather than continuing through the connector.

## Correct semantic rule

A ConnectorCandidate represents a physical connector/interface body.

It does **not** imply a pass-through topology.

A connector may be:

- Inline — conductor interaction represents a pass-through connector interface.
- ComponentAttached — connector is physically attached to a component/module and conductor interaction may terminate at that interface.
- Unknown — evidence does not uniquely establish the attachment relationship.

Connector recognition is therefore separated from conductor interaction classification.

## Model changes

ConnectorCandidate now carries:

- attached_component_candidate_id
- ConnectorTopologyKind

ConnectorConductorCrossingEvidence now carries:

- ConnectorConductorInteractionKind

with:

- PassThrough
- Termination
- Unknown

component_candidate_id remains unchanged and is never overloaded with connector identity.

## Recognition rule

Connector-body geometry is sufficient to produce a ConnectorBody candidate when the characteristic notched/interlocking geometry is present.

Pass-through conductor continuity is **evidence**, not a mandatory connector-recognition gate.

This is required because a component-attached connector can have legitimate conductor termination rather than pass-through continuity.

## Pin and endpoint rule

A ConnectorPin remains distinct from an EndpointCandidate.

For conductor interaction:

- pass-through observations retain the existing interior connector observation behavior;
- terminating observations anchor the pin observation to the independently observed conductor endpoint.

Connector-terminal association is not permitted from distance alone. An endpoint must be both spatially compatible with the pin and independently incident to a conductor segment observed at that connector pin.

## Topology invariants

Connector recognition must not:

- create a Wire;
- split a Wire;
- create a topology edge;
- create an ElectricalNet;
- infer mating;
- infer electrical equivalence.

The existing endpoint-to-endpoint Wire definition remains unchanged.

## Regression requirement

The connector implementation must be evaluated against the established TRX300 invariants:

- physical Wire identity remains independent of connector recognition;
- ElectricalNet identity remains independent of connector recognition;
- genuine chassis-ground coverage remains unchanged;
- component candidates are not fabricated for connector bodies;
- unresolved/conflicted connector associations remain explicit.

The current extraction result must be rerun after this correction. No expected connector count, Wire count, or ElectricalNet count is asserted here until the corrected implementation is executed against the current TRX300 source.

## Test coverage

The connector geometry detector now includes a regression case proving that a valid notched connector body is recognized without a pass-through conductor.

This directly protects the attached-connector correction from reintroduction of the old universal pass-through gate.

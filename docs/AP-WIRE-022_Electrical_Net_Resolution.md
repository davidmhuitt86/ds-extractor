# AP-WIRE-022 — Electrical Net Resolution

## Purpose

AP-WIRE-022 establishes the explicit electrical-net resolution boundary.

It combines deterministic topology/distribution decomposition, endpoint-level circuit-role evidence, semantic circuit-role resolution, and deterministic normalization of the resulting electrical-net objects.

## Inputs

- TopologyNode
- TopologyEdge
- EndpointCandidate
- ConductorSegment
- resolved circuit-role evidence from recognized semantic observations

## Outputs

### ElectricalNet

Each resolved net contains:

- deterministic net ID;
- endpoint membership;
- splice-node membership;
- topology-edge membership;
- distribution role;
- confidence;
- optional semantic anchor endpoint.

### Wire

Distribution branches produced from an explicitly anchored net remain endpoint-to-endpoint Wire objects. A splice is not converted into a wire endpoint.

## Resolution rules

1. Electrical connectivity is inherited only from electrically connective topology edges.
2. Ground and external-connection anchors may establish a distribution net according to the existing decomposition configuration.
3. Explicit semantic circuit-role evidence may establish PowerFeed or SharedFunctionFeed roles.
4. An unanchored multi-terminal distribution structure remains an unresolved electrical net; the resolver does not invent a source.
5. Cyclic distribution structures remain unresolved rather than selecting an arbitrary traversal.
6. A two-terminal endpoint-to-endpoint wire is not promoted to a distribution net merely because it passes through a splice.
7. An anchor must be an actual member of the resolved net.
8. Every referenced endpoint must exist in the authoritative endpoint model.
9. Duplicate deterministic net IDs are emitted only once.
10. Net membership collections are deterministically sorted and de-duplicated.
11. No component identity, symbol identity, terminal identity, topology, or wire geometry is inferred or mutated by this stage.
12. No fuzzy matching or OCR is performed here.

## Confidence

Confidence is inherited from the strongest unambiguous circuit-role evidence selected by CircuitRoleResolver. Unresolved or invalidated nets are assigned ConfidenceClass::Unresolved.

## Pipeline Position

Image -> Geometry -> Topology -> Endpoint Semantic Reconstruction -> Connector & Terminal Model -> Component Symbol Recognition -> Component Identity -> Electrical Net Resolution -> Engineering Diagram Reconstruction

AP-WIRE-022 is intentionally a model-boundary stage. It does not attempt to determine electrical behavior, current flow, voltage, loading, or circuit correctness. Those require later engineering semantics.

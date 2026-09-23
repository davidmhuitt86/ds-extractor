# AP-WIRE-019 — Endpoint Semantic Reconstruction

## Purpose

AP-WIRE-019 converts terminal semantic evidence into a deterministic, conflict-aware endpoint semantic state.

AP-WIRE-019 is downstream of endpoint reconstruction, terminal location detection, and terminal semantic evidence construction, and upstream of wire reconstruction and electrical-net resolution.

## Rules

1. Endpoint geometry remains authoritative for endpoint location.
2. Semantic evidence may change endpoint kind, terminal role, component association, and confidence.
3. Multiple compatible evidence records may reinforce the same endpoint identity.
4. Conflicting component identities, endpoint kinds, or terminal roles leave the endpoint semantically unresolved.
5. A conflict never selects a winner.
6. No geometry, topology edge, splice, junction, or wire identity is mutated.
7. Deterministic ordering and IDs are mandatory.
8. An endpoint with no semantic evidence remains unresolved at the semantic layer.

## Output

Each endpoint receives an EndpointSemanticReconstruction record containing endpoint ID, component evidence IDs, reconstructed component ID, endpoint kind, terminal role, confidence, and Resolved, Conflicted, or Unresolved status.

## Architectural boundary

AP-WIRE-019 does not perform OCR, component recognition, fuzzy matching, topology inference, or canonical component lookup. Those responsibilities remain in their existing boundaries.

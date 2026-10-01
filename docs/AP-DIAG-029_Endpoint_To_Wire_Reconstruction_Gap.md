# AP-DIAG-029 — Endpoint-to-Wire Reconstruction Gap

## Status

IMPLEMENTED — LOCAL EXTRACTION VERIFICATION PENDING

## Scope

This AP investigates the current TRX300 cropped-source endpoint-to-Wire coverage gap without changing topology construction, endpoint detection, conductor segmentation, or electrical-net resolution.

Authoritative extraction artifact reviewed from `extraction-results`:

- Source: `samples/trx300ODG.png`, 898×549, page 0
- Audit schema: 2
- Endpoint candidates: 192
- Wires: 37
- Topology nodes: 533
- Topology edges: 643
- Conductor segments: 230
- Electrical nets: 12
- Zero-Wire endpoints: 118
- Unowned topology edges: 602
- Fully unresolved Wire population: 28

No detector threshold or morphology tuning was performed.

## 1. Root-cause partition of the 118 zero-Wire endpoints

All 118 zero-Wire endpoints are degree-1 `ConductorEnd` nodes, as required by the existing `EndpointReconstructor` contract.

By kind:

| Endpoint kind | Count |
|---|---:|
| geometric | 111 |
| component_terminal | 7 |
| splice / ground / connector_terminal | 0 |

By first adjacent topology-node type:

| Adjacent node type | Count |
|---|---:|
| Crossing | 54 |
| Splice | 42 |
| Continuation | 22 |

This establishes that the zero-Wire population is not a population of malformed endpoint candidates. It is a population of already-established degree-1 endpoints whose physical Wire producer did not assign an endpoint-to-endpoint Wire.

## 2. Deterministic forensic replay

Using the published topology graph and the existing AP-WIRE-031 physical-continuity rule:

1. Start at each zero-Wire endpoint.
2. Follow a degree-2 Continuation node deterministically.
3. At a Splice/Junction/Crossing, continue only through an incident edge carrying the same `ConductorSegment` as the arriving edge.
4. Stop on another existing EndpointCandidate.
5. Never promote a Splice/Junction/Crossing node to a Wire endpoint.
6. Do not use proximity, straightness, color, electrical-net membership, or shortest-path heuristics.

The result is:

- 82 of 118 zero-Wire endpoints reach another existing zero-Wire endpoint.
- Those 82 endpoints form exactly 41 unique endpoint-to-endpoint pairs.
- All 41 pairs are disjoint at the endpoint level.
- No candidate path edge is shared by another candidate pair.
- No candidate pair touches an existing Wire endpoint.
- The candidate paths are therefore a deterministic reconstruction population, not a speculative branch pairing.

The remaining 36 zero-Wire endpoints terminate at a distribution node where no other incident edge carries the arriving `ConductorSegment`. The current evidence is insufficient to reconstruct those 36 endpoints without an additional evidence source.

## 3. Why AP-WIRE-031 did not produce these 41 Wires

`PhysicalWireIdentityReconstructor` Pass 2 currently requires both endpoints to have an AP-WIRE-030 `Resolved` `ConductorBoundaryResolution`.

That requirement was intended to prevent creation of a new Wire boundary from a bare geometric conductor end.

The forensic result demonstrates a narrower and important distinction:

> AP-DIAG-029 is not creating a Wire boundary. The EndpointCandidate already exists and is already a valid degree-1 endpoint. The operation is only establishing physical identity between two existing endpoints.

Therefore semantic boundary resolution must not be a prerequisite for this specific physical identity operation.

## 4. Implementation

Pass 2 eligibility was changed from:

- endpoint must have a Resolved ConductorBoundaryResolution

to:

- endpoint must already exist as an EndpointCandidate;
- endpoint kind must not be `Splice` or `Unresolved`;
- endpoint node must remain degree-1;
- physical continuation through distribution/crossing nodes still requires the existing exact ConductorSegment-sharing rule.

No topology node or edge is created, deleted, or reclassified.

No endpoint is created.

No Splice/Junction/Crossing is promoted to a Wire endpoint.

When semantic boundary evidence is absent, the resulting Wire's `identity_evidence_ids` contain only the real ConductorSegment evidence used to establish continuity.

## 5. Regression coverage

A regression case was added proving that two existing geometric endpoints can form a resolved physical Wire through a Splice using shared conductor-segment evidence even when no ConductorBoundaryResolution records are supplied.

The test also verifies:

- exactly one Wire is produced;
- the Splice is not a Wire endpoint;
- the Wire is `Resolved`;
- the only identity evidence is the actual shared ConductorSegment id;
- both topology edges are retained.

## 6. Expected TRX300 movement

The published forensic artifact predicts, before local extraction verification:

- Wires: 37 → 78
- Zero-Wire endpoints: 118 → 36
- Endpoint-to-Wire coverage gained: 82 endpoints
- Topology edges newly owned by these candidate Wires: 0–not yet claimed by an existing Wire in the forensic partition; exact fresh count must be taken from the regenerated audit.
- Electrical-net membership: expected to change downstream, but must not be predicted as a correctness result until the local extraction is run.

These are **forensic projections**, not verified extraction results.

## 7. Remaining 36 endpoints

The 36 unresolved cases are intentionally left untouched.

They stop at Splice/Junction/Crossing nodes with no exact segment-sharing continuation. Resolving them requires a new independent evidence source or a source-visual reconciliation of those specific boundaries.

No proximity or geometric convenience rule is authorized to close this remainder.

## 8. Validation gate

Local verification is required:

1. Clean Release build.
2. Full Release CTest.
3. Fresh TRX300 extraction.
4. Verify deterministic audit and topology artifacts.
5. Confirm the expected 41 additional Wires are actually produced.
6. Confirm all new Wires remain endpoint-to-endpoint.
7. Confirm zero Splice/Junction/Crossing nodes become Wire endpoints.
8. Confirm no existing Wire is duplicated or overlapped.
9. Recalculate electrical-net coverage and validation warnings.
10. Run the extraction twice and compare structured artifacts byte-for-byte.

Until those checks pass, AP-DIAG-029 remains implementation-complete but verification-open.

## 9. Next AP

If the 41-wire reconstruction verifies cleanly, the next diagnostic target is the remaining 36 zero-Wire endpoints plus the independent 62-endpoint electrical-net resolution gap.

If verification produces unexpected collateral changes, stop and audit the changed population before further reconstruction work.

## Implementation commits

- `b44bd58cf65a7284c89f16ec71b4e9c21a626d3f` — Pass 2 accepts existing non-splice/non-unresolved EndpointCandidates.
- `2757b39c50b9e118d9065217d75be4e41cb85077` — regression test for unresolved semantic boundary + geometric endpoint continuity.

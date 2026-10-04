# AP-DIAG-039 — Exact Production Replay of Residual Endpoint Termination

Status: COMPLETE — diagnostic only; no production reconstruction logic changed

## 1. Purpose

Resolve the apparent discrepancy raised during review of AP-DIAG-037 by replaying the
current `PhysicalWireIdentityReconstructor` semantics exactly against the freshly
published production extraction.

The authoritative production inputs are:

- `extraction-results` commit
  `d0918b162eb5206853c9790e110e838176348fea`
- `artifacts/audit/extraction_audit.json`
- source image dimensions 898 x 549
- 192 EndpointCandidates
- 78 published Wires

## 2. Exact replay contract

The replay mirrors the production `walk_from()` behavior:

1. Start only from an existing degree-1 EndpointCandidate not already claimed by a
   Pass-1 Wire.
2. At a degree-2 Continuation, take the only other edge.
3. At a degree-2 Splice/Junction, continue only when the arriving and departing
   edges have the same non-empty ConductorSegment ID.
4. At a degree-3-or-greater Splice/Junction/Crossing, continue only through other
   incident edges carrying the same ConductorSegment ID as the arriving edge.
5. Zero matching continuation edges terminates the walk.
6. More than one matching edge is an ambiguity/fork and is handled using the
   production `expanded_ambiguities` guard.
7. An existing EndpointCandidate is an endpoint outcome; a topology node is never
   promoted to a Wire endpoint.

No electrical-net membership, geometric proximity, shortest-path selection, or
new endpoint creation is used.

## 3. Result

The exact replay produces:

| Population | Count |
|---|---:|
| Eligible residual zero-wire endpoints | 36 |
| Residual endpoints with a terminal stop | 36 |
| Terminal stop events at Splice nodes | 36 |
| Unique terminal Splice nodes | 35 |
| Residual endpoints reaching an existing endpoint | 0 |
| Residual endpoints ending at Crossing nodes | 0 |
| Residual endpoints ending at Continuation nodes | 0 |

All 36 residual endpoints terminate at a Splice with the production stop reason
`no_same_segment`.

One Splice is the terminal stop for two residual endpoints, giving 35 unique
terminal Splice nodes.

## 4. Terminal Splice structure

Independent analysis of the 35 unique terminal Splice nodes gives:

| Structure | Count |
|---|---:|
| Degree-3 with repeated ConductorSegment (T-splice pattern) | 32 |
| Degree-4 with repeated ConductorSegment | 1 |
| Degree-3 with no repeated ConductorSegment | 2 |
| **Total** | **35** |

This exactly matches the structure reported by AP-DIAG-037.

## 5. Correction to the apparent Crossing discrepancy

A prior review note classified 20 residual walks as terminating at Crossing nodes.
That result came from a simplified diagnostic trace that treated a Crossing as a
terminal whenever its immediate onward match was not selected.

That trace was not equivalent to the production recursive walk.

The authoritative replay demonstrates that those residual walks continue through
one or more eligible Crossing/Continuation nodes under the same-segment rule and
eventually terminate at Splices.

Therefore:

- the 20 Crossing classification is superseded;
- AP-DIAG-037's statement that all 36 residual endpoints ultimately terminate at
  Splices is confirmed;
- AP-DIAG-038's intended population of 35 terminal Splice locations is correct.

## 6. Production conclusion

There is no residual endpoint-classification defect to fix in the current Wire
reconstructor.

The architecture boundary remains exactly the one established by AP-DIAG-037:

    78 production Wires
           +
    36 zero-wire endpoints
           |
           v
    35 terminal Splice nodes
           |
           v
    source-level physical-Wire branch evidence required

The 36 endpoints are unresolved because the current production rule has reached
a genuine Splice and found no unique same-ConductorSegment through continuation.
That is a physical-Wire identity evidence gap, not a Crossing traversal defect.

## 7. Required next action

AP-DIAG-038 is the correct next evidence step.

The 35 terminal Splice locations must be reconciled against the source pixels,
with:

- the 32 degree-3 repeated-segment Splices reviewed as the dominant cohort;
- the 1 degree-4 repeated-segment Splice reviewed separately;
- the 2 degree-3 non-repeated-segment Splices reviewed separately.

No production branch-decomposition rule is authorized until that source review
establishes a repeatable convention.

## 8. Acceptance

AP-DIAG-039 is complete when:

- the current production walk has been replayed with equivalent semantics;
- all 36 residual endpoints have an explicit terminal classification;
- the 35 unique terminal Splices are reproduced;
- the 32/1/2 structural partition is reproduced;
- no production code or extraction output is changed.

All criteria are satisfied by this audit.

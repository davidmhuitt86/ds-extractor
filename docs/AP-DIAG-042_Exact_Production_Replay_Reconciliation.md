# AP-DIAG-042 — Exact Production Replay Reconciliation

Status: IMPLEMENTED — diagnostic only; no production extraction logic changed

## 1. Purpose

AP-DIAG-041 produced a baseline of 77 Wires and 35 residual endpoints, but its
internal terminal-Splice helper reported only 14 terminal Splices. That conflicts
with AP-DIAG-039, which established 36 residual endpoints, 36 terminal stop events,
and 35 unique terminal Splice nodes on the published production extraction.

AP-DIAG-042 resolves that discrepancy by replaying the current
PhysicalWireIdentityReconstructor walk semantics directly over the same
WireModel returned by the canonical ExtractionPipeline.

The diagnostic also compares the replayed endpoint-pair population with the
WireModel's published Wire endpoint pairs. This distinguishes:

- a diagnostic tracing defect;
- a current in-memory production-baseline change;
- or a mismatch between the published extraction artifact and the current
  canonical pipeline result.

## 2. Scope

The replay mirrors the production algorithm in
src/topology/physical_wire_identity_reconstructor.cpp:

1. Reconstruct Pass 1 with WireReconstructor.
2. Identify eligible degree-1 EndpointCandidates not claimed by Pass 1.
3. Walk eligible endpoints in deterministic endpoint-ID order.
4. Continue degree-2 nodes directly, except Splice/Junction nodes require the
   same non-empty ConductorSegment ID on both sides.
5. At degree greater than two, continue only through incident edges carrying the
   same ConductorSegment ID as the arriving edge.
6. Expand every equally supported match, mark the walk conflicted, and use the
   shared expanded_ambiguities guard exactly as production does.
7. Never promote a topology node into an EndpointCandidate.

The diagnostic additionally records where a residual walk stops. Recording the
stop location is instrumentation only; production behavior still returns no
Wire when there is insufficient continuation evidence.

## 3. Canonical input

Source:

samples/trx300ODG.png

Expected dimensions:

898 x 549

Invocation:

.\build\Release\dx-audit-exact-production-replay.exe samples\trx300ODG.png artifacts\audit

## 4. Outputs

The tool writes:

artifacts/audit/AP-DIAG-042_exact_production_replay.json

The report contains:

- current canonical WireModel counts;
- Pass-1 WireReconstructor count;
- exact replay Pass-2 candidate-pair count;
- total unique replay endpoint-pair count, defined as the union of unchanged Pass-1
  pairs and reconstructed Pass-2 candidate pairs;
- replay-vs-model missing/extra pair counts computed against that full replay
  population;
- residual endpoint count;
- residual stop classifications;
- terminal Splice stop-event count;
- unique terminal Splice count;
- terminal Splice structural partition;
- deterministic residual endpoint IDs;
- per-residual trace classification.

## 5. Interpretation

### Result A — exact reproduction

The replay pair population equals the current model Wire pair population with zero
missing and zero extra pairs.

This proves the current production model is internally explained by the current
PhysicalWireIdentityReconstructor semantics. Any difference from a published
artifact is upstream of or external to the identity replay.

### Result B — diagnostic mismatch

The replay and current model contain different endpoint pairs.

This is a production/diagnostic semantic mismatch and must be resolved before any
further physical-Wire interpretation work.

### Result C — 35-terminal-Splice restoration

The exact instrumentation reproduces 36 residual stop events at 35 unique Splice
nodes.

This establishes that the AP-DIAG-041 terminal-Splice helper, rather than the
production graph, was responsible for the 14-node count.

### Result D — current-baseline divergence

The canonical pipeline produces a different Wire/endpoint/topology baseline from
the published extraction artifact used by AP-DIAG-039.

That divergence must be explicitly documented and the affected extraction
re-published before using the old artifact for quantitative comparisons.

## 6. Non-goals

This AP does not:

- alter PhysicalWireIdentityReconstructor;
- alter WireReconstructor;
- add branch decomposition;
- promote Splices to endpoints;
- change connector recognition;
- change object masking;
- modify the TRX300 extraction;
- authorize any production Wire-count target.

## 7. Acceptance

AP-DIAG-042 is complete when the canonical source is replayed successfully and
the resulting report provides:

- exact current model Wire count;
- exact Pass-1 and Pass-2 replay counts;
- zero or explicitly enumerated replay/model pair differences;
- exact residual endpoint accounting;
- exact terminal-Splice stop-event and unique-node accounting;
- deterministic structural partition of terminal Splices.

No production conclusion is permitted until the report is reviewed.

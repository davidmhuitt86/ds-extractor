# AP-DIAG-037 — Residual Zero-Wire Splice Termination Forensic Audit

Status: COMPLETE — diagnostic only; no production reconstruction rule changed

## 1. Purpose

Determine the structural cause of the 36 zero-wire endpoint candidates remaining
after AP-DIAG-035 and establish whether they can be safely addressed by a new
physical-Wire reconstruction rule.

The audit uses the published production extraction on branch
`extraction-results`, commit `c7f3978c37cd33a84dad432ce7e24a4453e514cb`,
with `artifacts/audit/extraction_audit.json` as the structured source.

## 2. Production baseline

Current extracted populations:

| Population | Count |
|---|---:|
| Endpoint candidates | 192 |
| Wires | 78 |
| Zero-wire endpoints | 36 |
| Topology nodes | 533 |
| Topology edges | 643 |
| Conductor segments | 230 |
| Electrical nets | 12 |
| Validation errors | 0 |
| Validation warnings | 72 |

Endpoint coverage is 156 single-wire endpoints, 36 zero-wire endpoints, and
0 endpoints in multiple wires.

## 3. Deterministic replay

The current `PhysicalWireIdentityReconstructor` logic was independently
replayed over the published topology graph.

Result:

- Pass 1 reproduces 37 base Wires.
- Pass 2 reproduces 41 additional endpoint pairs.
- Unique reconstructed endpoint pairs = 78.
- Published Wires = 78.
- Missing published endpoint pairs = 0.
- Extra reconstructed endpoint pairs = 0.

Therefore the current production result is completely explained by the
current reconstruction algorithm and published graph. The 36 zero-wire
endpoints are not an export/publishing discrepancy and are not being lost
after Wire creation.

## 4. Residual endpoint tracing

Every one of the 36 zero-wire endpoints is degree-1 and, when traced using
the current continuity rules, ultimately terminates at a `Splice` node.

The first-hop population (15 Splice / 12 Crossing / 9 Continuation) therefore
describes only the first topology node encountered. It is not the causal
termination class.

There are 35 unique terminal Splice nodes because two residual endpoints
terminate at the same Splice node.

## 5. Terminal Splice structure

The 35 terminal Splice nodes partition as follows:

| Structure | Count |
|---|---:|
| Degree-3 Splice with a repeated ConductorSegment (T-splice pattern) | 32 |
| Degree-4 Splice with a repeated ConductorSegment | 1 |
| Degree-3 Splice with no repeated ConductorSegment | 2 |
| Total terminal Splices | 35 |

The dominant 32-node pattern is:

`branch segment (1 incident edge)`
+
`through segment (2 incident edges sharing the same ConductorSegment ID)`

In the dominant cases, the residual endpoint is reached through the singleton
segment while both other legs reference the same ConductorSegment.

Representative observed geometry includes:

- (82,467): branch segment -> vertical repeated segment
- (443,55): branch segment -> horizontal repeated segment
- (491.5,337): branch segment -> horizontal repeated segment
- (659,348.5): branch segment -> horizontal repeated segment
- (516,187): branch segment -> horizontal repeated segment

The same structural pattern repeats throughout the remaining dominant cases.

## 6. Outward leg analysis

For the dominant T-splice cases, tracing outward from the two repeated-segment
legs generally reaches already-existing endpoint candidates. In several cases
those endpoints already participate in published Wires; in some cases the
reachable endpoint itself is another zero-wire endpoint.

This proves that the missing branch endpoint is structurally capable of being
paired with real endpoint candidates on the two through directions.

It does NOT by itself prove which physical-Wire decomposition the source
intends.

## 7. Normative model interaction

`docs/WIRE-IDENTITY-001_Endpoint_to_Endpoint_Wire_Model.md` gives the
normative branching representation:

    Terminal A ---------o--------- Terminal B
                        |
                        +--------- Terminal C

with intended Wire objects:

    Wire 1: A -> B
    Wire 2: A -> C

and explicitly permits the conductor section from A to the junction to be
shared by both Wire objects.

However, `docs/AP-WIRE-029_Conductor_Boundary_and_Wire_Identity.md` separately
binds the reconstruction stage to a never-guess rule: splice topology alone
does not establish which physical conductor pairing is intended. A future
implementation must have explicit source evidence or an explicit source
convention supporting the decomposition.

These two documents are therefore complementary:

- WIRE-IDENTITY-001 defines what a valid branch representation looks like.
- AP-WIRE-029 defines the evidence threshold required before selecting that
  representation for an actual source.

## 8. Hypothetical branch-pair population

As a diagnostic only, the literal WIRE-IDENTITY-001 branch representation was
projected onto the current TRX300 graph:

- 58 unique new endpoint-to-endpoint candidate pairs result.
- 0 of those 58 pairs already exist in the published Wire population.
- 42 endpoints would acquire more than one Wire incidence.
- The projection therefore materially changes endpoint coverage and cannot be
  treated as a harmless completion of the existing 78-Wire set.

The 58-pair result is NOT an accepted production result.

## 9. Exceptional structures

Three terminal Splices require separate treatment:

### (a) (347,354.5)

Degree 3 with three distinct ConductorSegment IDs:

- vertical segment `c2fdaaee5cf50c86`
- left branch `ba92e9fd65c18b7c`
- right branch `cee8346a0b83d266`

Two separate zero-wire endpoints terminate at this same Splice.

There is no repeated segment to establish a through leg, so no deterministic
branch decomposition is justified by the current evidence model.

### (b) (370,354.5)

Degree 3 with three distinct ConductorSegment IDs:

- `d6e48ad7edd3da26`
- `cee8346a0b83d266`
- `3aa7402e8270ca92`

Again, no repeated segment identifies a through conductor.

### (c) (347,373.5)

Degree 4 with:

- two incident edges sharing `c2fdaaee5cf50c86`
- branch segment `3518682708d566b1`
- branch segment `879df362b65b15f6`

This is a multi-branch case and cannot safely be reduced to the degree-3
T-splice rule.

## 10. Root-cause determination

The dominant residual population is not caused by:

- missing endpoint candidates;
- failed result publication;
- incorrect Pass-2 eligibility;
- a Crossing-specific walk failure;
- a Continuation-specific walk failure;
- or an endpoint-to-Wire serialization defect.

The dominant residual is a **physical-Wire decomposition gap at genuine
Splice topology events**.

The graph currently contains enough structure to show a likely
branch/through arrangement at 32 degree-3 Splices, but the current evidence
model deliberately refuses to choose a physical Wire interpretation there.

## 11. Decision

No production code change is authorized by this diagnostic AP.

In particular, the following rule must NOT be introduced yet:

> "At every degree-3 Splice with one singleton segment and one repeated segment,
> pair the singleton endpoint with both through endpoints."

That rule would be an explicit engineering rule, but this audit has not yet
established that the TRX300 source itself uses that convention at all 32
locations.

## 12. Required next evidence

The next AP must perform source-level reconciliation at the 35 terminal Splice
locations, with the 32 dominant T-splices treated as one cohort and the three
exceptional structures treated separately.

Required evidence for the dominant cohort is one of:

1. explicit source convention establishing branch-pair decomposition;
2. source visual evidence that distinguishes the through conductor from the
   branch in a way that establishes the physical-Wire relationships; or
3. another independent conductor-identity evidence source already represented
   by the repository model.

Until that evidence exists, the 36 residual endpoints remain correctly
unresolved under the current never-guess contract.

## 13. Acceptance criteria for the next implementation AP

A future implementation AP must include:

- deterministic synthetic tests for the WIRE-IDENTITY-001 A/B/C branch model;
- a negative test proving that a generic degree-3 splice does not receive the
  branch rule without the required evidence;
- a multi-branch test for the degree-4 case;
- preservation of shared conductor geometry across multiple Wire objects;
- deterministic endpoint incidence accounting when an endpoint legitimately
  participates in multiple Wire objects;
- zero promotion of a Splice node to a Wire endpoint.

No production TRX300 count target is specified by this diagnostic AP.

## 14. Conclusion

AP-DIAG-037 closes the current structural investigation enough to identify the
remaining architecture boundary precisely:

    current reconstruction
            |
            v
    78 resolved Wires
            +
    36 zero-wire endpoints
            |
            v
    35 terminal Splices
            |
            v
    physical-Wire branch decomposition evidence still required

The next implementation should therefore be evidence-driven branch
decomposition, not another generic graph-walk relaxation.

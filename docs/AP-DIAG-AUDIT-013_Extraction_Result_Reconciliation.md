# AP-DIAG-AUDIT-013 — Extraction Result Reconciliation

**Status:** AUDIT ONLY — no production extraction, connector, wire, topology, or classification code was modified.

## 1. Audit objective

Reconcile the latest automatically published extraction on branch `extraction-results`
against the source-grounded TRX300 engineering inventory, with emphasis on:

- ConnectorCandidate count and source coverage
- connector-terminal evidence
- physical-wire count and changes
- electrical-net count and changes
- unresolved coverage
- source identity before any cross-run comparison is treated as valid

The audit is deliberately evidence-first. No detector threshold, classifier rule,
wire identity rule, or topology rule is changed by this AP.

## 2. Current extraction snapshot

The latest `extraction-results/artifacts/extraction_review/review_manifest.json`
was generated at `2026-09-30T07:38:19Z`.

| Metric | Current result |
|---|---:|
| Source ID | `C:\\dev\\ds-extractor\\samples\\trx300ODG.png` |
| Image width | 898 |
| Image height | 549 |
| Wires | 39 |
| Wire-color observations | 230 |
| Symbols | 34 |
| Terminals | 18 |
| Connector candidates | 37 |
| Connector terminals | 29 |
| Splices/junctions | 533 |
| Grounds | 6 |
| Topology edges | 643 |
| Electrical nets | 12 |
| Validation errors | 0 |
| Validation warnings | 35 |
| Endpoints with zero Wire ownership | 114 |
| Electrical-net endpoints in no net | 154 |
| Wires fully unresolved by coverage diagnostics | 24 |
| Wires with component association resolved | 10 |
| Findings | 1,076 |

Manifest blob SHA at the time of this audit:
`18f730bf143e03becc7acb69289ad2b55ac0739f`.

## 3. Source-identity gate

This is the first blocking reconciliation finding.

The repository's verified baseline identifies the canonical TRX300 input as:

`samples/trx300ODG.png`

with SHA-256:

`a145fffeb4cc9261152930936737048e39e92b030de5c9427b8f60e427d5da61`

The baseline/AP record establishes the canonical extraction history around that
source at **1056×816**.

The current published extraction manifest identifies the same filename but reports
**898×549**.

Those dimensions are not equal and their aspect ratios are materially different.
Therefore the latest extraction **cannot yet be treated as a directly comparable
run of the verified canonical source** solely from the filename.

No assumption is made that the 898×549 raster is a crop, resize, re-render, or
replacement of the canonical 1056×816 source.

### Finding AUDIT-013-001 — SOURCE IDENTITY UNVERIFIED

**Severity:** CRITICAL for cross-run reconciliation.

**Evidence:**
- Current extraction manifest: 898×549, source ID `samples/trx300ODG.png`.
- Verified repository baseline: canonical `samples/trx300ODG.png`, SHA-256
  `a145fffeb4cc9261152930936737048e39e92b030de5c9427b8f60e427d5da61`,
  historical baseline 1056×816.

**Disposition:** OPEN.

Until the current local source file is hashed and compared with the canonical
SHA-256, comparisons between the current 37 connectors / 39 wires / 12 nets and
the prior source-grounded inventory must be treated as provisional.

## 4. Connector reconciliation

The latest review layer contains **37 ConnectorCandidate visual regions**.

The established source-grounded connector census in the repository documents
**12 located inline connector instances** and **33 visible connector-side pins**.
A later engineering working count identifies **13 physical connector instances**,
with attached-connector semantics explicitly covering the CDI Unit, Alarm Unit,
and rectifier-related assembly.

The current 37-candidate visual layer is sufficient to establish that the detector
is now producing connector-native geometry candidates across the diagram. It is
not sufficient by itself to establish a one-to-one mapping of all 37 candidates
to the 12 source-census locations because the published review PNG does not
display candidate IDs.

### Finding AUDIT-013-002 — CANDIDATE OVERPRODUCTION

**Severity:** HIGH.

37 extracted candidates versus 12 source-grounded inline locations means the
candidate population is substantially larger than the established source
inventory.

This is a detector/materialization reconciliation issue, not evidence that the
diagram contains 37 connectors.

The current artifact does not contain enough per-candidate identity data to
classify all 37 individually as VALID / FALSE_POSITIVE / DUPLICATE /
COMPONENT_ATTACHED / UNRESOLVED without introducing assumptions.

**Disposition:** OPEN.

## 5. Source-location coverage

The current connector review image was visually inspected against the known
TRX300 connector census locations.

The known source connector areas remain visibly represented in the latest
connector review layer, including:

- tail-light/fuse area
- CDI Unit
- Alarm Unit
- relay cluster
- Pulse Generator / Reverse Switch area
- Rectifier area
- Regulator/Rectifier area

This establishes **source-location coverage at the visual ROI level**.

It does **not** establish that every candidate within those regions is correct,
nor does it resolve the later 13th physical-instance distinction.

### Finding AUDIT-013-003 — SOURCE COVERAGE PRESENT, IDENTITY NOT RECONCILED

**Severity:** MEDIUM.

The detector is no longer failing simply because no connector-native candidates
exist. The remaining question is candidate precision and semantic materialization.

**Disposition:** OPEN.

## 6. Connector-terminal reconciliation

Current extraction:

- ConnectorCandidates: 37
- ConnectorTerminals: 29

Prior source-grounded inline census:

- 12 located connector instances
- 33 visible connector-side pins
- 33 conductor entries
- 33 conductor exits
- 33 pass-through observations

The 29 current connector-terminal records therefore cannot be treated as a
validated source-terminal count.

The counts are not directly comparable in kind: the source census counts visible
connector-side pin positions, while the current model counts materialized
ConnectorTerminal records.

### Finding AUDIT-013-004 — TERMINAL COUNT NOT YET SOURCE-RECONCILED

**Severity:** HIGH.

The 29 materialized ConnectorTerminals neither proves nor disproves the 33
source-grounded visible connector-side pins. A candidate-by-candidate and
pin-by-pin mapping is required.

**Disposition:** OPEN.

## 7. Physical-wire reconciliation

Current extraction reports:

- 39 physical wires
- 0 invalid wires
- 24 fully unresolved by coverage diagnostics
- 10 with resolved component association

The prior verified scoped baseline used in repository audits reports 35 physical
wires, while the earlier canonical baseline records 40 physical wires.

Because the current source raster identity is not yet verified, the apparent
39-wire result cannot be attributed to detector behavior, source differences,
or legitimate topology differences.

### Finding AUDIT-013-005 — WIRE COUNT DELTA NOT ATTRIBUTABLE

**Severity:** HIGH.

The current 39-wire result is recorded, but no causal conclusion is made about
the difference from historical 35/40-wire measurements.

The 24 fully unresolved wires are an audit target, not automatically errors:
Wire identity remains governed by endpoint-to-endpoint physical identity and must
not be guessed from visual convenience, alignment, color, or net membership.

**Disposition:** OPEN.

## 8. Electrical-net reconciliation

Current extraction reports **12 electrical nets**.

Historical verified runs document **10 electrical nets**.

The current manifest also reports:

- 154 endpoint candidates not present in any electrical net
- 0 endpoints belonging to multiple electrical nets
- 6 `NET-ROLE-UNRESOLVED` warnings

The 12-net result cannot yet be reconciled against the historical 10-net result
until source identity is established and the current structured net membership
records are available.

No net is to be merged, split, or assigned a role merely to make the count match.

### Finding AUDIT-013-006 — NET COUNT DELTA NOT ATTRIBUTABLE

**Severity:** HIGH.

**Disposition:** OPEN.

## 9. Validation state

The latest extraction reports:

- **0 validation errors**
- **35 validation warnings**

Warning codes:

| Warning | Count |
|---|---:|
| `NET-ROLE-UNRESOLVED` | 6 |
| `WIRE-GEOMETRIC-ENDPOINTS` | 29 |

The absence of validation errors is confirmed by the current manifest. It does
not establish source correctness; validation checks structural/model invariants,
not complete source-to-model semantic fidelity.

## 10. Audit disposition

### CLOSED

- Current extraction artifact publication mechanism is functioning.
- Latest extraction result is frozen on `extraction-results`.
- Current extraction manifest is internally readable.
- Current review set contains the expected visual layers.
- Current result contains 37 connector candidates and 29 connector terminals.
- Current result contains 39 physical wires and 12 electrical nets.
- Current result contains 0 validation errors.

### OPEN

1. **AUDIT-013-001 — source identity**
   - Verify the current local `samples/trx300ODG.png` SHA-256 against the canonical
     `a145fff...` SHA-256.
2. **AUDIT-013-002 — connector candidate reconciliation**
   - Produce a structured per-candidate inventory containing candidate ID,
     bounds, topology kind, conductor interaction, attached component, pin count,
     and confidence.
3. **AUDIT-013-004 — connector terminal reconciliation**
   - Map each materialized ConnectorTerminal to source connector location/pin evidence.
4. **AUDIT-013-005 — wire reconciliation**
   - Enumerate all 39 current wires and classify the 24 currently fully unresolved
     records from evidence.
5. **AUDIT-013-006 — electrical-net reconciliation**
   - Enumerate all 12 nets and reconcile endpoint membership without forcing
     historical counts.

## 11. No-fix rule

No production code was changed by this audit.

In particular, this AP does **not**:

- change connector thresholds;
- change notch detection;
- change wire reconstruction;
- change electrical-net resolution;
- change component classification;
- manufacture missing connector terminals;
- delete candidate connectors to force a count;
- merge/split wires to force a historical count;
- merge/split nets to force a historical count.

## 12. Next AP gate

The next implementation decision is blocked on **AUDIT-013-001**.

The first required evidence is the SHA-256 of the exact local source file used by
the latest extraction. Once source identity is established, the 37 current
ConnectorCandidates can be reconciled against the correct source raster rather
than against a potentially different historical raster.

**Conclusion:** The current extraction has materially advanced connector
recognition, but the audit cannot legitimately declare the 37 candidates,
29 connector terminals, 39 wires, or 12 nets source-correct until source identity
and per-object structured reconciliation are completed.

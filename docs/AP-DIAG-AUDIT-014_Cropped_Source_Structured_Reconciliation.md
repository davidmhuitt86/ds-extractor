# AP-DIAG-AUDIT-014 — Cropped Source Structured Reconciliation

**Status:** AUDIT COMPLETE — no production extraction logic modified.

## 1. Test condition

The current extraction input is intentionally cropped to remove the switch matrix.

This crop is a controlled diagnostic input. The switch matrix is physically absent from the raster; no masking/exclusion region is required for this run.

Therefore the current 898×549 raster is **not** to be reconciled against the historical 1056×816 raster as though they were the same geometric source.

The relevant audit question is whether the current extraction model is internally and structurally reconcilable from the cropped source.

## 2. Current extraction snapshot

Published manifest:

- Source ID: `C:\dev\ds-extractor\samples\trx300ODG.png`
- Raster: 898×549
- Wires: 39
- Conductor/color observations: 230
- Symbols/components: 34
- Terminal candidates: 18
- Connector candidates: 37
- Connector terminals: 29
- Splices/junctions: 533
- Grounds: 6
- Topology edges: 643
- Electrical nets: 12
- Validation errors: 0
- Validation warnings: 35
- Zero-wire-ownership endpoints: 114
- Electrical-net endpoints in no net: 154
- Fully unresolved wires: 24
- Wires with resolved component association: 10
- Wires with resolved color: 0

## 3. Source-identity finding from AP-DIAG-AUDIT-013

### AUDIT-014-001 — CLOSED / SUPERSEDED

The prior source-identity blocker is superseded by the explicit test-condition definition.

The 898×549 input is intentionally cropped for this diagnostic run. Its reduced dimensions therefore do not constitute evidence of an extraction or source-file error.

Historical 1056×816 measurements remain useful only as historical/reference measurements and must not be used as a direct geometric baseline for this run.

## 4. Structured-artifact availability

The published `review_manifest.json` contains only aggregate counts.

Its `coverage_summary` contains:

`"see": "artifacts/audit/extraction_audit.json#coverage"`

However, inspection of the complete `extraction-results` tree shows that no `artifacts/audit/extraction_audit.json` is published.

The extraction-results branch currently contains the visual review set and `review_manifest.json`, but no machine-readable per-object extraction/audit dataset.

### AUDIT-014-002 — MACHINE-READABLE AUDIT ARTIFACT MISSING

**Severity:** CRITICAL for structured reconciliation.

The producer advertises a machine-readable audit artifact that is absent from the published extraction-results branch.

Consequences:

- The 39 wires cannot be enumerated from the published result.
- The 24 fully unresolved wires cannot be individually classified.
- The 37 connector candidates cannot be mapped individually.
- The 29 connector terminals cannot be mapped to their parent candidates.
- The 12 electrical nets cannot be enumerated or inspected.
- The 114 zero-wire-ownership endpoints cannot be classified.
- The 154 endpoints absent from nets cannot be classified.
- Cross-run object-level comparison cannot be performed from the published artifacts alone.

**Disposition:** OPEN.

## 5. Producer-side evidence

The current `ReviewArtifactWriter` writes:

- visual extraction layers;
- `review_manifest.json`;
- aggregate coverage counts.

The manifest explicitly references `artifacts/audit/extraction_audit.json#coverage`.

The writer implementation inspected for this audit does not establish publication of that referenced audit JSON.

This is an artifact-publication completeness defect, not evidence that the underlying extraction model is incorrect.

## 6. Connector reconciliation

Current aggregate result:

- 37 ConnectorCandidates
- 29 ConnectorTerminals
- 0 unresolved connectors according to aggregate coverage
- 0 furniture-derived connectors

The 37/29 relationship cannot be judged as overproduction or underproduction without object-level records.

A connector candidate is not equivalent to a connector terminal. Terminal materialization requires independent conductor/pin evidence. Therefore no count-forcing change is justified.

### AUDIT-014-003 — CONNECTOR RECONCILIATION BLOCKED

**Severity:** HIGH.

**Disposition:** BLOCKED by AUDIT-014-002.

Required evidence per candidate:

- stable candidate ID
- bounding box
- connector geometry classification
- conductor interaction
- attached component
- materialized terminal IDs
- pin count
- confidence/status

## 7. Wire reconciliation

Current aggregate result:

- 39 physical wires
- 0 invalid wires
- 24 fully unresolved
- 10 component-association resolved
- 0 color-resolved

The zero color-resolution count is particularly important but cannot yet be interpreted as a wire-extraction failure. Color semantics and physical wire identity are separate concerns.

The 24 fully unresolved wires must be examined individually before changing wire reconstruction.

### AUDIT-014-004 — WIRE RECONCILIATION BLOCKED

**Severity:** HIGH.

**Disposition:** BLOCKED by AUDIT-014-002.

Required evidence per wire:

- stable wire ID
- start endpoint
- end endpoint
- conductor segment IDs
- geometry
- color observations
- component association
- electrical-net membership
- semantic resolution status
- unresolved/conflict reason

The established endpoint-to-endpoint wire identity rule remains unchanged.

## 8. Electrical-net reconciliation

Current result:

- 12 electrical nets
- 154 endpoints not in any net
- 0 endpoints in multiple nets
- 6 NET-ROLE-UNRESOLVED warnings

The current count of 12 must not be forced toward the historical count of 10.

The actual membership of each net must be inspected first.

### AUDIT-014-005 — NET RECONCILIATION BLOCKED

**Severity:** HIGH.

**Disposition:** BLOCKED by AUDIT-014-002.

Required evidence per net:

- stable net ID
- endpoint IDs
- wire IDs
- component/connector-terminal relationships
- net role/status
- unresolved endpoint IDs

## 9. Endpoint reconciliation

The current model contains 192 endpoint candidates, of which 114 have zero Wire ownership.

That is approximately 59.4% of endpoint candidates.

This is too large to dismiss as ordinary noise, but the aggregate count alone cannot distinguish:

- legitimate geometric endpoints;
- component-boundary endpoints;
- connector-terminal candidates;
- ground endpoints;
- annotation artifacts;
- duplicate endpoint candidates;
- endpoints that should own a wire but do not.

The 154 electrical-net endpoints absent from any net likewise require classification before any resolver modification.

### AUDIT-014-006 — ENDPOINT OWNERSHIP COVERAGE REQUIRES OBJECT-LEVEL AUDIT

**Severity:** HIGH.

**Disposition:** BLOCKED by AUDIT-014-002.

## 10. Validation interpretation

The current result reports:

- 0 validation errors
- 35 validation warnings

Warnings:

- NET-ROLE-UNRESOLVED: 6
- WIRE-GEOMETRIC-ENDPOINTS: 29

This is internally consistent with the current validation layer.

However, zero validation errors establishes model-structural validity, not source fidelity.

No production logic should be changed merely to reduce warning counts.

## 11. Audit disposition

### CLOSED

- Cropped-source test condition is valid.
- Switch matrix removal is correctly treated as an input change rather than masking.
- 898×549 dimensions are not a defect for this test.
- Current extraction result is published to the dedicated extraction-results branch.
- Visual review artifact generation is functioning.
- Aggregate manifest generation is functioning.
- Current model reports zero validation errors.

### OPEN

1. **AUDIT-014-002 — machine-readable audit artifact missing**
2. **AUDIT-014-003 — connector reconciliation blocked**
3. **AUDIT-014-004 — wire reconciliation blocked**
4. **AUDIT-014-005 — electrical-net reconciliation blocked**
5. **AUDIT-014-006 — endpoint ownership reconciliation blocked**

## 12. Next AP gate

The next implementation AP should address **artifact completeness**, not detector tuning.

### AP-DIAG-015 — Structured Extraction Audit Artifact

The extraction-results publisher should emit a deterministic machine-readable artifact containing the complete structured model required to audit the current run.

At minimum:

```
artifacts/
  audit/
    extraction_audit.json
```

The artifact should contain:

- source metadata
- extraction run identity
- component candidates
- terminal candidates
- connector candidates
- connector terminals
- conductor segments
- endpoint candidates
- wires
- topology nodes
- topology edges
- electrical nets
- coverage diagnostics
- validation results
- unresolved/conflict reasons

The artifact must be generated from the existing `WireModel`; it must not perform a second detection pass or independently rediscover geometry.

The existing manifest reference to:

`artifacts/audit/extraction_audit.json#coverage`

should become a valid published reference.

**No detector threshold, connector classifier, wire reconstruction rule, or electrical-net rule should be changed until this artifact exists and the cropped extraction has been reconciled object-by-object.**

## 13. Conclusion

The cropped test successfully removes the switch matrix as a confounding variable.

The present blocker is now clear and architectural:

**the extractor produces a rich internal model and aggregate coverage metrics, but the extraction-results branch does not publish the structured evidence required to audit that model.**

Therefore the next AP is an **artifact/publication completeness AP**, not another detector-tuning AP.

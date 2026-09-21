# EKE-DX-WIRE-REQ-001
# Requirements Traceability Matrix

| Requirement | Architecture | Algorithm | Model | Test |
|---|---|---|---|---|
| Single centerline per conductor | ARCH-001 | ALG-001 | MDL-001 | TST-001 |
| No double-edge wires | ARCH-001 | ALG-001 | MDL-001 | TST-001 |
| Orthogonal routing | ARCH-001 | ALG-001 | MDL-001 | TST-001 |
| Non-wire exclusion | ARCH-001 | ALG-001 | MDL-001 | TST-001 |
| Segment clipping | ARCH-001 | ALG-001 | MDL-001 | TST-001 |
| Collinear merging | ARCH-001 | ALG-001 | MDL-001 | TST-001 |
| Endpoint snapping | ARCH-001 | ALG-001 | MDL-001 | TST-001 |
| Junction/crossing distinction | ARCH-001 | ALG-001 | MDL-001 | TST-001 |
| Topology graph | ARCH-001 | ALG-001 | MDL-001 | TST-001 |
| Heavy cable classification | ARCH-001 | ALG-001 | MDL-001 | TST-001 |
| Stable object identity | ARCH-001 | ALG-001 | MDL-001 | TST-001 |
| Provenance | ARCH-001 | ALG-001 | MDL-001 | TST-001 |
| SVG export | ARCH-001 | ALG-001 | MDL-001 | TST-001 |
| Deterministic replay | ARCH-001 | ALG-001 | MDL-001 | TST-001 |
| Standalone application | APP-001 | API-001 | MDL-001 | TST-001 |
| EKE integration | ARCH-001 | API-001 | MDL-001 | TST-001 |

---

## Traceability rule

No implementation feature should be considered complete until it has:

```text
requirement
  ->
architecture location
  ->
algorithm definition
  ->
domain representation
  ->
automated test
```

This keeps the extraction engine aligned with the original specification rather than allowing implementation details to become the architecture.

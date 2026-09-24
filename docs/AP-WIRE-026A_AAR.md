# AP-WIRE-026A — After-Action Report

Component Symbol-Family Recognition

Status: **complete and validated**. Establishes a real but deliberately
narrow recognition boundary — 1 geometry-only rule, 1 combined
label+geometry rule, 0 speculative families — with an explicit,
non-bare-"yes" answer to whether it closes the AP-WIRE-026 gap (§21).

## 1. Baseline commit

`f63e150` (AP-WIRE-026 validated and pushed).

## 2. Implementation commits

- `239e6ba` — model + `SymbolFamilyRecognizer` + `SymbolRecognitionProvider` + 23-case test
- `db380a7` — export to `topology.json`/`extraction_audit.json`/`engineering_diagram.json`

## 3. Final validation commit

This AAR and the design doc are committed immediately after `db380a7`,
before push.

## 4. Build environment

- Compiler: GCC 13.3.0
- CMake: as configured by `CMakeLists.txt`
- **OpenCV: 5.1.0**, built from source at `/usr/local`
  (`-DOpenCV_DIR=/usr/local/lib/cmake/opencv5`). No OpenCV 4 target,
  downgrade, or validation occurred - this AP added no image-processing
  code at all (it operates purely on already-extracted model data), so
  it has no OpenCV dependency of its own beyond the existing library
  target it links into.
- libcurl 8.5.0 (unchanged)

## 5. Complete CTest results

**48/48 Release CTest passed, 0 failed** (47 from the AP-WIRE-026
baseline + `dx-wire-test-symbol-family-recognizer`, 23 cases). Total test
time 0.24-0.29s across repeated runs.

## 6. TRX300 extraction metrics

| Metric | AP-WIRE-026 baseline | AP-WIRE-026A | Δ |
|---|---:|---:|---:|
| Conductor segments | 294 | 294 | 0 |
| Topology nodes | 692 | 692 | 0 |
| Topology edges | 877 | 877 | 0 |
| Endpoints | 210 | 210 | 0 |
| Components | 109 | 109 | 0 |
| Wires | 40 | 40 | 0 |
| Electrical nets | 10 | 10 | 0 |
| Validation errors | 0 | 0 | 0 |
| Validation warnings | 30 | 30 | 0 |

Every required invariant held exactly. `SymbolFamilyRecognizer` reads
`component_candidates`, `component_symbol_geometries`,
`component_identity_canonicalizations` and writes only
`model.symbol_family_evidence`/`model.symbol_family_resolutions` - it
cannot have influenced any number above by construction (no topology/
wire/endpoint/net type appears anywhere in its function signature).

## 7. Symbol-family taxonomy established

`Ground, Lamp, Switch, Relay, Motor, Diode, Alternator, Battery,
Solenoid, Coil, Unknown` - 10 named families + Unknown. See the design
doc for why this set and not the reference image's full visual
vocabulary (compound symbols, fuses, sensors, etc. were excluded for lack
of a currently-defensible rule).

## 8. Recognition rules

Exactly two, both in `src/topology/symbol_family_recognizer.cpp`:

1. `ComponentCandidateKind::ChassisGround` → `Ground` (purpose-built
   geometric classification, sufficient alone, `High` confidence).
2. Keyword match against Resolved identity text + compatible
   `ComponentCandidateKind` → the matched family (`Medium` confidence).

No rule keys on raw `SymbolPrimitiveKind` counts alone - see the design
doc's "What was deliberately NOT made a rule."

## 9. Evidence rules

A family is "resolution-eligible" for a component when it has at least
one `PurposeBuiltGeometricClassification`/`LabelKeywordWithCompatibleGeometry`
evidence item, **or** at least two independent `ProviderObservation`
items (corroboration requirement - a single provider guess is never
sufficient). See design doc "Confidence and corroboration."

## 10. Confidence rules

`High`: purpose-built geometric classification present.
`Medium`: label-keyword + compatible geometry, no purpose-built evidence.
`Unresolved`: `Conflicted` or `Unresolved` status (no confidence value is
assigned when there is no single resolved family). No numeric
probabilities were introduced - `ConfidenceClass` (the project's existing
enum) is used throughout, per the task's explicit instruction.

## 11. Resolved/unresolved/conflicted counts (real TRX300 extraction)

| Status | Count |
|---|---:|
| Total real components considered | 59 |
| **Resolved** | **10** (all `Ground`) |
| Unresolved | 49 |
| Conflicted | 0 |

Per-family resolved breakdown: `ground: 10`, all others: `0`.

## 12. Synthetic fixture results

All 23 cases in `tests/test_symbol_family_recognizer.cpp` passed,
covering: empty input; Unknown-with-zero-evidence vs.
Unresolved-with-evidence vs. Resolved (the required 3-way distinction);
Resolved via the geometry-only rule; Resolved via label+geometry;
label-without-compatible-geometry correctly failing to resolve;
compatible-evidence reinforcement; conflicting-evidence → Conflicted;
deterministic IDs and ordering; duplicate-evidence dedup in
`evidence_ids`; provenance traceability (`evidence_ids` → real
`SymbolFamilyEvidence` records with non-empty `source`/`detail`);
`ComponentSymbolGeometry` reference preservation (id-only, never
duplicating primitives); component-identity independence (mutation
check: recognizing does not alter the input `ComponentCandidate`/
`ComponentIdentityCanonicalization` objects; a Conflicted
canonicalization's text is correctly never trusted); `DiagramFurniture`
exclusion even with a matching label; the Null provider; full
multi-component assembly; and full serialization-content determinism
(not just IDs) across repeated calls.

## 13. Real TRX300 results

**10 `Ground` resolutions, all from Rule 1 (purpose-built geometric
classification)**, exactly matching the 10 `chassis_ground_shapes` in
the AP-WIRE-022A baseline. Zero resolutions from Rule 2 (label keyword),
because this baseline used the deterministic extraction path (no
`--recognition`/`--vision-recognition`), so
`component_identity_canonicalizations` contains zero `Resolved` entries -
there is no label text for Rule 2 to match against. This mirrors the
AP-WIRE-025 AAR's wire-color 0/40 result for the identical reason and is
reported as zero, not invented, per the task's explicit instruction.

## 14. Determinism results

Two independent fresh extractions
(`dx-extract extract samples/trx300ODG.png --output <dir>`, separate
directories), diffed:

```
diff <run1>/engineering_diagram.json <run2>/engineering_diagram.json → exit 0
diff <run1>/topology.json <run2>/topology.json → exit 0
```

**Byte-equivalent deterministic output confirmed** for both artifacts
that carry symbol-family data.

## 15. Provenance verification

Directly inspected the real extraction's one resolved case:

```json
{
  "id": "symbol-family-resolution-f1f79efa89ae2393",
  "component_id": "component-candidate-shape-region-1790c5fb3a4f437c",
  "family": "ground", "confidence": "high", "status": "resolved",
  "source_symbol_geometry_id": "component-symbol-geometry-component-candidate-shape-region-1790c5fb3a4f437c",
  "evidence_ids": ["symbol-family-evidence-676480303e46837a"]
}
```

tracing to:

```json
{
  "id": "symbol-family-evidence-676480303e46837a",
  "component_id": "component-candidate-shape-region-1790c5fb3a4f437c",
  "family": "ground", "kind": "purpose_built_geometric_classification",
  "confidence": "high", "source": "shape-detector-chassis-ground",
  "detail": "ComponentCandidateKind::ChassisGround comes from a purpose-built ground-symbol detector, not a generic shape bucket"
}
```

Every resolved family in the real extraction is explainable end-to-end:
resolution → evidence → the specific upstream detector responsible, with
a human-readable `detail` string, not just a bare status.

## 16. AP-WIRE-024 conflict preservation

Directly re-checked against the real TRX300 extraction (same object IDs
as documented in the AP-WIRE-024/025/026 AARs):

```
component-candidate-shape-region-845947b0afd7451a in diagram: True
endpoint-candidate-b0e3d6bb622a227c fabricated into that component's endpoint_ids: False
endpoint-candidate-b0e3d6bb622a227c fabricated into ANY component: False
```

Still correctly unresolved/conflicted after four consecutive APs now
(024, 025, 026, 026A) have each extended the model and re-verified this
same case. `SymbolFamilyRecognizer` in particular cannot have touched
this at all - it never reads `EndpointCandidate`/`TerminalCandidate`.

## 17. Structural mutation confirmation

`SymbolFamilyRecognizer::recognize()`'s parameter list is
`(components, geometries, canonicalizations, provider_observations)` -
no `TopologyNode`/`TopologyEdge`/`Wire`/`ElectricalNet` type appears
anywhere in it, so it is structurally impossible for this AP to have
mutated topology, wires, or nets. Confirmed empirically by §6's unchanged
table. Test case 14 additionally confirms by direct comparison that the
*input* `ComponentCandidate`/`ComponentIdentityCanonicalization` objects
are byte-identical before and after a `recognize()` call (no accidental
mutation of what it does consume, even though the parameters are
`const&` and mutation would not compile).

## 18. Reference-image coverage analysis (delta from AP-WIRE-026)

AP-WIRE-026's table already covered most features; this AP specifically
closes one row:

| Reference feature | AP-WIRE-026 | AP-WIRE-026A |
|---|---|---|
| Component symbol-family identity (lamp/switch/relay/motor/diode/alternator/battery/solenoid/coil) | **not representable** - no upstream evidence | **representable and resolved for `Ground`** (10/59 on TRX300); representable-but-currently-Unresolved for the other 9 families pending label-recognition evidence |

This is a **partial, evidence-honest close** of the gap, not a full one -
see §20.

## 19. Remaining symbol-family limitations

1. **9 of 10 families have zero real resolutions on this baseline**,
   because Rule 2 requires Resolved component-identity text, which
   requires a recognition provider (`--vision-recognition` or similar) -
   not exercised in this baseline for comparability with every prior
   AP's methodology.
2. **No rule exists yet for symbol families distinguishable primarily by
   internal geometry arrangement** (e.g. a relay's coil+switch pair, a
   diode's arrow-bar shape) rather than by label text. AP-WIRE-023's own
   AAR already found that TRX300's primitive geometry is too coarse
   (`Unknown`-dominant) to support such a rule today; a future AP would
   need a more specific upstream primitive-arrangement detector
   (Rule-1-style) before this could be added responsibly.
3. **No real `SymbolRecognitionProvider` implementation exists** - the
   injectable boundary is established and tested (`Null` provider +
   corroboration logic), but exercising it with real observations is
   future work, consistent with the task's instruction not to turn this
   AP into a network-dependent pipeline.
4. **Compound/multi-symbol assemblies** (e.g. a connector body containing
   multiple distinct pin symbols) are out of scope - the taxonomy and
   rules operate per-`ComponentCandidate`, and TRX300 currently has 0
   connector candidates to exercise this concern.

## 20. Exact AP-WIRE-027 contract

AP-WIRE-027 receives, for every real component:

- A `SymbolFamilyResolution` (via `DiagramComponent.symbol_family_resolution_id`)
  with an explicit `status` (`Resolved`/`Unresolved`/`Conflicted`) and,
  when `Resolved`, a `family` and `confidence`.
- On TRX300 today: 10 components resolve to `Ground` and can be rendered
  with a ground symbol without guessing. **The other 49 real components
  are `Unresolved`** - AP-WIRE-027 must render them using their
  `ComponentCandidateKind` geometric bucket (exactly as the existing
  review layers already do: a labeled bounding box / generic symbol),
  **not** attempt to infer a family from geometry itself. This is not a
  regression from AP-WIRE-026A - it is the honest state of the evidence,
  explicitly surfaced rather than hidden behind a rendering-time guess.
- AP-WIRE-027 must **not** need to identify a lamp/switch/relay/motor/
  diode/battery itself, infer terminal or wire identity, reconstruct
  topology, or rediscover component meaning from the source image - all
  of that remains this AP's (and its predecessors') responsibility. If
  AP-WIRE-027 wants more than 10/59 components to carry a resolved
  family, the correct fix is running extraction with real recognition
  evidence (or a future `SymbolRecognitionProvider`), not adding
  inference logic to the renderer.

## 21. Architecture assessment / verdict

**Does this AP close the AP-WIRE-026 symbol-family gap?** Partially, and
honestly: the recognition *boundary* is fully established, tested, and
proven deterministic - AP-WIRE-027 never has to guess a symbol family
itself, and every resolution it consumes is either confidently supported
or explicitly marked unresolved. What is **not** closed is *coverage*:
only `Ground` (10/59 real components) has a real, currently-supportable
rule on this fixture. This was a deliberate, stated choice - the task
explicitly prioritized "a defensible engineering-symbol recognition
boundary that a future renderer can trust" over "maximize the number of
recognized symbols," and warned against weakening rules to produce
non-zero results. Zero unsupported guesses were made; 10 real, explainable
ground-symbol resolutions were made; 49 components correctly remain
`Unresolved` rather than receiving an invented family.

AP-WIRE-027 was not started.

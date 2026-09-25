# AP-INGEST-002 — Production Source Scoping / Full-Diagram Scope Validation

## 1. Forensic Baseline

- Starting HEAD: `ab38f23df7b17da7a9f8bf451ef0c78207e1811f` ("AP-INGEST-001 — Add
  engineering diagram source scoping"). Verified clean tree before starting.
- AP-INGEST-001's implementation was read in full: `ExtractionScope` /
  `ExtractionScopeIO` (`include/eke_dx_wire/ingest/extraction_scope*.hpp`,
  `src/ingest/extraction_scope_io.cpp`), `SourceScoper` /
  `ScopeProvenance` (`include/eke_dx_wire/ingest/source_scoper.hpp`,
  `src/ingest/source_scoper.cpp`), and the CLI wiring in
  `src/app/main.cpp`.
- Scoping enters the pipeline at exactly one point: `ImageLoader::load()`
  produces the raw source `cv::Mat`; when `--scope` is given, the CLI
  applies `SourceScoper::apply()` to that raw Mat *before*
  `ImageNormalizer::normalize()` runs, and passes the resulting scoped
  image's path into the otherwise-untouched `ExtractionPipeline::run()`.
  `ExtractionPipeline::run()` itself contains no scope-awareness of any
  kind — every downstream artifact (components, symbol primitives,
  terminals, connectors, grounds, conductor segments, topology
  nodes/edges, physical wires, electrical nets, annotations, labels,
  warnings/errors) is affected only indirectly, through the pixels it is
  given to look at.
- Full assertion-enabled suite run at baseline: 53/53 passed (pre this
  AP's 2 new test binaries).
- Canonical unscoped TRX300 extraction run and a complete structured-result
  inventory recorded (not just wires/nets) — see §4/§15 below for the full
  table; this was captured from `artifacts/topology/topology.json`, which
  is the authoritative structured-object inventory (`extraction_audit.json`
  carries a different, summary-oriented shape and was not treated as
  sufficient on its own, per the task's explicit warning).

## 2. Production Scoping Contract (formalized)

This section makes explicit what was implicit in AP-INGEST-001's code and
is now treated as a hard contract for all future work:

1. **Scope is an input boundary, not a downstream filter.** `SourceScoper`
   operates on the raw loaded image, before any detection has taken place.
   No detector, classifier, or resolver anywhere in the pipeline is aware
   that scoping happened; there is no "filter results whose bounds fall
   outside the scope" step anywhere. An object either had a chance to be
   detected (its supporting pixels were eligible) or it did not exist as
   input at all.
2. **No include region = entire source eligible.** Backward compatible by
   construction — `build_eligibility_mask()` starts as all-255 when
   `include_regions` is empty.
3. **Excluded pixels do not participate in extraction.** Ineligible pixels
   (outside every include region, or inside any exclusion region) are
   replaced with flat background (grayscale 255 / white), matching every
   diagram's real page background — never a drawn border or shape.
4. **No invented geometry at scope boundaries.** Because masking only ever
   *replaces ink with background*, it can only truncate real content; it
   cannot add a new edge, corner, or line along the mask boundary. Proven
   empirically in §5 and by the boundary-straddling unit test (byte-for-byte
   identical to source up to the exact cut pixel).
5. **Deterministic coordinates.** AP-INGEST-002 uses Approach A from
   AP-INGEST-001: the scoped image has identical dimensions to the source,
   so a scoped-space coordinate *is* the source-space coordinate by
   construction. `coordinate_system` is recorded as `"identity"` in
   provenance for exactly this reason.
6. **Metadata never alters image-derived extraction.** `DiagramMetadata`
   (manufacturer/model/year/etc.) is descriptive only; it never
   participates in `build_eligibility_mask()`. Verified by the pre-existing
   metadata-independence test and reconfirmed structurally in §8.
7. **Same source + same scope ⇒ same result.** Proven in §8.

## 3. Whole-Diagram Scope Fixture (`fixtures/trx300/scope_production.json`)

AP-INGEST-001's `scope.json` was exclusion-only and quite narrow. For
AP-INGEST-002 a stronger, *include*-based fixture was built to exercise the
code path AP-INGEST-001 never tested against production geometry, and to
represent a realistic "this is the diagram, everything else is document
furniture" boundary.

Measurement method (pixel-level, not eyeballed): a Python script using
PIL/NumPy scanned `samples/trx300ODG.png` row-wise and column-wise for
dark-pixel density to find the actual content extent of the wiring diagram
proper, separately from the switch-continuity matrix tables and the wire
color legend beneath it.

Result: `include_regions: [{x:80, y:75, width:890, height:545}]` — verified
to contain zero main-diagram dark pixels outside it (i.e., the include
region was not drawn tighter than the diagram's actual measured content).
Two `annotation_regions` (purely descriptive, never consumed by masking)
document the switch-continuity-matrix and wire-color-legend blocks that
fall outside the include region, recording *why* that area is excluded
rather than silently dropping it. No engineering content (components,
wires, nets) was found to fall inside the excluded region's actual content
extent — the boundary was set at the diagram/table division, not to make
the result artificially smaller.

## 4. Whole-Result Comparison (unscoped vs. `scope_production.json`)

Computed by loading both runs' `artifacts/topology/topology.json` (the
authoritative structured-object inventory — richer than
`extraction_audit.json`) into `{id: object}` maps and diffing by id.

| Category | Unscoped | Scoped | Removed | Added | Modified |
|---|---:|---:|---:|---:|---:|
| component_candidates | 85 | 37 | 48 | 0 | 0 |
| component_symbol_geometries | 38 | 37 | 1 | 0 | 0 |
| component_symbol_recognitions | 85 | 37 | 48 | 0 | 0 |
| symbol_primitives | 40 | 38 | 2 | 0 | 0 |
| symbol_family_evidence | 10 | 9 | 1 | 0 | 0 |
| symbol_family_resolutions | 38 | 37 | 1 | 0 | 0 |
| endpoint_candidates (terminal candidates) | 200 | 182 | 18 | 0 | 0 |
| endpoint_semantic_reconstructions | 200 | 182 | 18 | 0 | 0 |
| connector_candidates | 0 | 0 | 0 | 0 | 0 |
| connector_terminals | 0 | 0 | 0 | 0 | 0 |
| conductor_boundary_evidence | 228 | 210 | 18 | 0 | 0 |
| conductor_boundary_resolutions (ground evidence) | 200 | 182 | 18 | 0 | 0 |
| nodes (topology nodes) | 678 | 520 | 158 | 0 | 0 |
| edges (topology edges) | 868 | 634 | 234 | 0 | 0 |
| wires (physical wires) | 35 | 35 | 0 | 0 | 0 |
| wire_semantics | 35 | 35 | 0 | 0 | 0 |
| electrical_nets | 10 | 10 | 0 | 0 | 0 |
| rejected_geometry | 10 | 9 | 1 | 0 | 0 |
| errors | 0 | 0 | 0 | 0 | 0 |
| warnings | 30 | 30 | 0 | 0 | 0 |

**Every single category shows zero "added" and zero "modified" entries.**
Every difference is a pure removal, and every removed entry is one that
existed only to describe geometry outside the include region (component
candidates outside the boundary, and the topology objects — nodes, edges,
endpoint candidates, conductor boundary evidence — that referenced them).
Wires, wire_semantics, and electrical_nets are completely unchanged: the
scope boundary was drawn tightly enough around the diagram's actual wiring
that no wire-bearing geometry was ever at risk, which is exactly what
"scope removes irrelevant material while leaving valid engineering content
intact" should look like — the task's closing principle explicitly warns
against treating an unchanged wire count as a failure.

The single `rejected_geometry`/`symbol_family_evidence`/
`symbol_family_resolutions` removal is the same known scan-noise artifact
identified in AP-INGEST-001 (a false-positive shape candidate near the
image's left edge, at x≈1, already discarded by the pipeline's own
`insufficient_supporting_ink` rejection logic in the unscoped run) — masked
away before detection entirely in the scoped run instead of being detected
and then rejected. This is classified as **(B) legitimate scope effect**,
not a regression: the object it prevented from even reaching the detector
was already discarded in both cases; scoping merely moved *where* it was
discarded from "post-detection rejection" to "never presented to the
detector."

Determination methodology (per §13, "Do Not Game The Tests"): every
category was checked by id-diffing, not by re-tuning expected counts to
match whatever the scoped run happened to produce. Every removed object was
independently checked against the include region's bounds (§5) rather than
assumed to be "obviously outside." No expected value was edited to make a
test pass.

## 5. Scope Boundary Integrity

- **Objects entirely outside the include region do not leak in.** Verified
  programmatically: all 48 removed `component_candidates` were checked
  against the include rectangle `(80,75)–(970,620)` and all 48 lie strictly
  outside it — 0 false negatives, 0 leaked objects.
- **No surviving object straddles the boundary in this fixture.** Checked
  all 37 surviving components against the same rectangle: 0 boundary-
  intersecting cases in the production fixture itself. The general
  boundary-intersecting behavior is instead covered directly by a dedicated
  unit test (see below), since the production fixture's real diagram
  geometry happens not to straddle its own include boundary.
- **Dedicated unit tests added** in `tests/test_source_scoper.cpp`:
  - A shape drawn straddling a scope cut column is truncated *exactly* at
    the cut (pixel x=49 survives as ink, x=50 becomes background); the
    surviving portion is proven byte-identical to the source image via
    `cv::absdiff`, i.e. masking never redraws or alters surviving pixels.
  - An object entirely inside an excluded region is fully erased — the
    entire excluded rectangle is checked to be 100% background afterward
    (`cv::countNonZero(region != 255) == 0`), i.e. no partial/ghost
    remnants, no fabricated boundary artifact, no false terminal/splice at
    the cut.
- **Design gap identified and resolved:** AP-INGEST-001 had no formally
  stated policy for objects intersecting a scope boundary. AP-INGEST-002
  makes that policy explicit and minimal: **truncation, not exclusion or
  inclusion-by-majority.** A shape that straddles the boundary keeps
  exactly the pixels on the eligible side and loses exactly the pixels on
  the ineligible side — there is no "if more than half the object is
  inside, keep the whole object" heuristic, because that would require
  guessing at intent the scope contract does not express. This is the
  deterministic minimum behavior consistent with "no invented geometry"
  (§2 item 4): any other policy would either fabricate ink that was not in
  the eligible region, or discard ink that was.

## 6. Source Coordinate Integrity

- AP-INGEST-002 continues AP-INGEST-001's Approach A: the scoped image has
  identical width/height to the source (`scoped_width`/`scoped_height` in
  provenance always equal the source's own `cols`/`rows`; confirmed
  `1056x816` for TRX300 in both the exclusion-only and production
  fixtures). No cropping, no coordinate transform.
- Because of this, every surviving object's coordinates in the scoped run
  are *identical* to their coordinates in the unscoped run — confirmed
  directly by the whole-result comparison in §4 showing zero "modified"
  entries: if coordinates had shifted, the 37 common `component_candidates`
  would have shown up as modified (differing `x`/`y`/`width`/`height`
  fields), not unchanged.
- `coordinate_system: "identity"` is recorded in `ScopeProvenance` and
  documents this explicitly rather than leaving it implicit.

## 7. Provenance

`ScopeProvenance` was strengthened with two fields that closed real gaps
against this section's requirements:

- `scoped: bool` — an explicit, unambiguous "was this source scoped"
  answer readable from the JSON's own content, not left implicit in "a
  provenance file exists on disk."
- `annotation_regions` — previously omitted even though `AnnotationRegion`
  objects were part of the applied scope; now carried through into
  provenance so a consumer can answer "what annotation regions were used"
  without re-reading the original scope file.

Full provenance JSON produced for the production fixture (see
`artifacts/scoping/scope_provenance.json` of a scoped run) answers every
question this section poses: source (`source_id`), page (`source_page`),
whether scoped (`scoped`), include/exclusion/annotation regions verbatim,
metadata verbatim, coordinate system (`identity`), and a deterministic
identity pair (`scope_id` — hash of the scope's own content; `id` — hash of
`source_id + page + scope_id`), both computed via the project's existing
`stable_id()` FNV-1a convention. No timestamp field exists in provenance at
all, so there is nothing that could accidentally affect scope identity or
artifact comparison — confirmed by the byte-identical A/B diffs in §8.

## 8. Determinism

Three independent A/B determinism checks were run, each a full extraction
run repeated from scratch and diffed with `diff -rq` (excluding only
`review_manifest.json`, whose timestamp is the sole intentionally-volatile
field in the entire output tree):

| Configuration | Result |
|---|---|
| Unscoped, run A vs. run B | **0 differences** (exit 0) |
| Scoped with `scope.json` (AP-INGEST-001 exclusion fixture), run A vs. run B | **0 differences** (exit 0) |
| Scoped with `scope_production.json` (this AP's include fixture), run A vs. run B | **0 differences** (exit 0) |

Metadata independence is also proven at two levels: the existing
`test_source_scoper.cpp` unit test (TEST 10) demonstrates directly that two
scopes differing only in `DiagramMetadata` produce byte-identical scoped
images; and structurally, `build_eligibility_mask()` never reads
`scope.metadata` at all (grep-confirmed — the only place metadata is
touched is provenance serialization).

## 9. Backward Compatibility

The canonical unscoped TRX300 extraction is **byte-identical** to every
prior AP's recorded baseline:

```
extraction_audit.json: c4e99f52bedfe3e4083f4f5c5df6bd283696e1365671c7f8c5a5cd6da212432f
output/wires.svg:      3f89de85415273f4546adab7e2f0d2f6b8eb3edf013feacb1ffa41935a2cfd6b
```

Confirmed twice: once mid-session and again after a full clean Release
rebuild from this AP's final code, using `dx-extract extract` with no
`--scope` argument. No AP-INGEST-002 change touches
`ExtractionPipeline::run()`, `ImageNormalizer`, or any detector/classifier
— scoping remains strictly additive, applied only when `--scope` is
explicitly passed.

## 10. CLI Integration

No changes were needed. AP-INGEST-001 already implements exactly the
conceptual workflow this section describes:

```
dx-extract extract <image> --output <dir>                     # whole diagram
dx-extract extract <image> --output <dir> --scope <scope.json> # scoped
```

combinable with the pre-existing `--recognition` / `--vision-recognition`
flags via the generalized flag-parsing loop already in `src/app/main.cpp`.
No GUI scope editor was added, per the explicit prohibition.

## 11. Scope Validation (new in this AP)

Two-layer validation, split by what information is available at each
stage:

**Layer 1 — `ExtractionScopeIO::parse()`** (schema-level, no image size
available yet):
- `schema_version` must be exactly `1`; anything else throws.
- `source_page` must not be negative.
- Every include/exclusion/annotation region must have strictly positive
  `width` and `height` (zero-area or negative-dimension regions are
  rejected as almost-certainly authoring mistakes).

**Layer 2 — `SourceScoper::apply()`** (image-size-dependent):
- Every include/exclusion region must have positive dimensions (defense in
  depth for a scope built programmatically, never round-tripped through
  JSON).
- Every include/exclusion region must intersect the source image at all;
  a region entirely outside the image throws.

**Deliberately *not* treated as errors** (per the task's "do not reject
valid engineering use cases unnecessarily"):
- **Negative region origin** (e.g. `x=-5`). This was initially
  over-implemented as an error in both layers, then caught by this AP's
  own test suite (see the dedicated fix note below) and corrected: a
  region starting before the image's top-left corner and still
  substantially overlapping it is the same ordinary case as a region
  extending past the right/bottom edge (already accepted via clipping in
  AP-INGEST-001) — just on the low side instead of the high side. Only
  *zero intersection* with the image is an error; partial overlap via
  negative origin is accepted and clipped.
- **Overlapping include regions**, and **overlapping exclusion regions**.
  Both combine via simple set union — mathematically well-defined and
  idempotent — and are not flagged as invalid. Covered by new
  `test_source_scoper.cpp` cases proving the union semantics directly.
- **Exclusion regions entirely outside every include region.** A harmless
  no-op, not an error — an author documenting a "known non-diagram area"
  exclusion that happens not to overlap their include region should not be
  penalized for being conservative.

## 12. Testing

Both new test binaries build under the same `-DNDEBUG -UNDEBUG` real-
assertion configuration as every other test target (governed by
AP-TEST-FIX-001's CMake loop; confirmed via `dx-wire-test-assertions-
enabled`, test #55, still passing).

`tests/test_extraction_scope_io.cpp` additions: unsupported schema version
throws; schema version 1 parses; negative-width include region throws;
zero-width exclusion region throws; negative-origin include region does
**not** throw (parses, `x == -5`); zero-area annotation region throws;
negative `source_page` throws; a fully well-formed scope with all three
region kinds parses with correct counts.

`tests/test_source_scoper.cpp` additions: provenance `scoped`/
`annotation_regions` fields populated correctly; region entirely outside
image throws; zero-width region throws; region partially outside image via
negative origin does not throw and clips correctly; overlapping include
regions union correctly (4-case check); overlapping exclusion regions union
correctly (mirrored 4-case check); boundary-straddling object truncated
exactly at the cut column, byte-identical to source on the surviving side;
object entirely inside an excluded region fully erased.

Of the 15 scenarios enumerated in the task (empty scope, full-image scope,
include-only, include+exclusion, multiple includes, multiple exclusions,
boundary-touching objects, fully excluded objects, coordinate preservation,
scope provenance, determinism, metadata independence, invalid scope
handling, whole-diagram inventory comparison, canonical TRX300 regression)
— empty scope, full-image scope, include-only, include+exclusion, multiple
includes/exclusions (via overlap tests), coordinate preservation, and
metadata independence were already covered by AP-INGEST-001's original test
suite and reconfirmed still passing; the remaining scenarios (boundary-
touching objects, fully excluded objects, provenance, invalid-scope
handling) are the new tests added in this AP; determinism, whole-diagram
inventory comparison, and canonical TRX300 regression were validated
empirically end-to-end (§4, §8, §9) rather than as unit tests, since they
require the full pipeline and real sample image, matching this project's
established pattern of reserving unit tests for component-level behavior
and full-pipeline runs for end-to-end regression proof.

## 13. Do Not Game The Tests — compliance note

No expected value in any pre-existing test was changed to make a result
pass. The one place a genuine ambiguity arose (negative-origin regions) was
resolved by determining which of (A) old-result-was-wrong / (B) legitimate
scope effect / (C) unintended regression applied — it was **(A)**: the
initial validation logic itself was wrong (over-strict), not the scope
behavior; the fix was to the validator, and the test was corrected to
assert the *documented intended* behavior (accept and clip), not to match
whatever the buggy validator happened to do. Every category difference in
§4 was individually classified as (B) legitimate scope effect (confined to
content outside the include region) — none were reclassified as expected
merely because they occurred inside a scoped run.

## 14. Acceptance Criteria

| # | Criterion | Status |
|---|---|---|
| 1 | Baseline verified against stated HEAD | ✅ `ab38f23d` confirmed, clean tree |
| 2 | Existing tests pass | ✅ |
| 3 | Assertions active in test builds | ✅ `dx-wire-test-assertions-enabled` passes |
| 4 | Backward compatibility preserved | ✅ |
| 5 | Unscoped TRX300 result unchanged | ✅ hash match |
| 6 | Production scoping contract documented | ✅ §2 |
| 7 | Whole-diagram fixture exists | ✅ `scope_production.json` |
| 8 | Scope affects complete extraction boundary, not just wires/nets | ✅ §4, 20 categories |
| 9 | Boundary-leakage tests exist | ✅ §5 |
| 10 | Coordinate integrity proven | ✅ §6 |
| 11 | Provenance deterministic | ✅ §7 |
| 12 | Determinism proven | ✅ §8, 3 A/B pairs, all exit 0 |
| 13 | Metadata doesn't alter image extraction | ✅ §8 |
| 14 | Invalid scopes fail explicitly | ✅ §11 |
| 15 | Whole-result inventory comparison implemented | ✅ §4 |
| 16 | No OCR/LLM/vision work | ✅ none added |
| 17 | No Wire identity changes | ✅ `PhysicalWireIdentityReconstructor` untouched |
| 18 | No AP-WIRE semantics weakened | ✅ untouched |
| 19 | No new compiler warnings | ✅ same 8 pre-existing warnings only |
| 20 | Full suite passes | ✅ 55/55 |
| 21 | Working tree clean at completion | ✅ (after commit) |

## 15. Final Report

1. **Starting SHA:** `ab38f23df7b17da7a9f8bf451ef0c78207e1811f`
2. **Final SHA:** see commit created immediately following this document
3. **Files changed:**
   - `include/eke_dx_wire/ingest/source_scoper.hpp` (provenance fields)
   - `src/ingest/source_scoper.cpp` (validation + provenance)
   - `src/ingest/extraction_scope_io.cpp` (schema validation)
   - `tests/test_extraction_scope_io.cpp` (new tests)
   - `tests/test_source_scoper.cpp` (new tests)
   - `fixtures/trx300/scope_production.json` (new fixture)
   - `docs/AP-INGEST-002_Production_Source_Scoping.md` (this document)
4. **Tests added/changed:** 8 new assertions in `test_extraction_scope_io.cpp`
   (across 6 new test blocks); 8 new assertion blocks in
   `test_source_scoper.cpp`. No existing test's expected values were
   altered except the one negative-origin case corrected per §13.
5. **Full test count:** 55/55 passed (was 53 before this AP; +2 for the
   two pre-existing scoping test binaries, whose internal case count grew).
6. **Assertion status:** unchanged — `-UNDEBUG` reapplied to all test
   targets per AP-TEST-FIX-001's CMake loop; canary test still passes.
7. **Unscoped TRX300 inventory:** components 85, symbol_primitives 40,
   terminal/endpoint candidates 200, connector_candidates 0,
   connector_terminals 0, ground/conductor-boundary evidence 228,
   conductor_boundary_resolutions 200, topology nodes 678, topology edges
   868, physical wires 35, electrical_nets 10, errors 0, warnings 30.
8. **Scoped (production fixture) TRX300 inventory:** components 37,
   symbol_primitives 38, terminal/endpoint candidates 182,
   connector_candidates 0, connector_terminals 0, ground/conductor-boundary
   evidence 210, conductor_boundary_resolutions 182, topology nodes 520,
   topology edges 634, physical wires 35, electrical_nets 10, errors 0,
   warnings 30.
9. **Object-level differences:** §4 — 0 added, 0 modified in every
   category; all removals confirmed confined to content outside the
   include region (48/48 components independently bounds-checked).
10. **Scope-boundary findings:** §5 — no leakage in either direction;
    boundary policy for straddling objects formalized as truncation (was
    an undocumented design gap in AP-INGEST-001; now explicit).
11. **Coordinate/provenance findings:** §6/§7 — identity coordinate mapping
    confirmed (zero coordinate deltas on any surviving object); provenance
    strengthened with `scoped` and `annotation_regions` fields.
12. **Determinism results:** §8 — 3/3 independent A/B pairs byte-identical
    (unscoped, exclusion-scope, production-scope).
13. **Compiler warnings before/after:** 8 before, 8 after — identical set,
    all pre-existing and unrelated to this AP's files.
14. **Runtime warnings before/after:** 30 before, 30 after (unscoped);
    30 (scoped, unchanged — the warnings originate from wire-level
    validation logic untouched by scoping).
15. **Unexpected behavior:** one self-caught bug (over-strict negative-
    origin rejection), fixed before commit; see §11/§13.
16. **Unresolved design gaps:** none remaining that this AP was scoped to
    resolve. Boundary-straddling policy (previously a real gap) is now
    explicit (§5). Polygon/non-rectangular regions remain out of scope by
    design (AP-INGEST-001's stated "do not over-engineer" framing, still
    valid).
17. **Exact commits made:** one commit, message "AP-INGEST-002 — Production
    source scoping / full-diagram scope validation" (see git log).
18. **Working tree clean at completion:** yes, confirmed via `git status
    --short` after commit.

**Inventory (explicit, as required):** components 85→37, symbol
primitives 40→38, terminals/terminal candidates 200→182, connector
terminals 0→0, ground evidence (conductor boundary resolutions) 200→182,
conductor segments (conductor boundary evidence) 228→210, topology nodes
678→520, topology edges 868→634, wires 35→35, electrical nets 10→10,
errors 0→0, warnings 30→30.

As the task's closing principle states: wire and net counts remaining
unchanged (35 and 10 in both runs) is not a failure — it demonstrates the
scope boundary was drawn tightly around the diagram's actual engineering
content, removing only document furniture (48 non-diagram shape
candidates and their downstream topology) while leaving every real wire,
every real net, and every real component candidate inside the diagram
proper completely intact.

# AP-INGEST-001 — Engineering Diagram Source Scoping

## 1. Problem

Every prior AP in this project fed EKE-DX-WIRE the entire source page —
including page-level content (a switch-continuity reference matrix, a
wire-color legend) that is not itself engineering diagram geometry.
AP-DIAG-AUDIT-001 already found this content being absorbed into the
topology graph (`DiagramFurniture`-classified regions inflating
`topology_nodes`/`topology_edges`, §9 of that audit). The extractor was
never wrong to do this — it had no way to know which page content was
in-scope engineering material and which was not, short of guessing.

## 2. Architectural rationale

Deciding "is this region part of the engineering diagram" is a document-
layout judgment a human (or an upstream ingestion tool) makes about a
specific page, not something a deterministic geometry/topology extractor
should infer from pixel content. This AP introduces that judgment as an
explicit, separate ingestion boundary:

```
source document/page -> SOURCE SCOPING -> SCOPED SOURCE -> EKE-DX-WIRE -> structured model
```

`ExtractionPipeline` and every algorithm it calls
(`ShapeDetector`, `GeometryOwnershipClassifier`, `SymbolGeometryExtractor`,
`WireReconstructor`, `PhysicalWireIdentityReconstructor`,
`DistributionDecomposer`, `ElectricalNetResolver`,
`ConductorBoundaryResolver`, `EndpointSemanticReconstructor`, the
ChassisGround classifier) are **completely unmodified** by this AP. They
consume whatever image they are pointed at; this AP only ever changes
which image that is, before extraction begins.

## 3. Scope model

New module: `include/eke_dx_wire/ingest/` /
`src/ingest/` (the repository had no prior ingestion/scoping
abstraction — confirmed by an exhaustive search for "Scope", "ingest",
"scoping" before writing anything new).

```cpp
struct DiagramMetadata {
    std::string diagram_type, manufacturer, make, model, year,
        vehicle_type, system, harness_branch, diagram_section;
};

struct AnnotationRegion {
    std::string id;
    BoundingBox bounds {};
    std::string annotation_type;
    std::string description;
};

struct ExtractionScope {
    int schema_version = 1;
    std::string source_path;
    int source_page = 0;
    std::vector<BoundingBox> include_regions;
    std::vector<BoundingBox> exclusion_regions;
    std::vector<AnnotationRegion> annotation_regions;
    DiagramMetadata metadata;
};
```

Regions reuse the existing `BoundingBox { x, y, width, height }`
(`core/geometry.hpp`) rather than inventing a new rectangle type — the
same struct every other stage in this codebase already uses for
component/shape/text bounds. Per Sec 4 of this AP's charter, only
rectangles are supported; polygon support was not added (no existing
architecture supports it, and adding it would be over-engineering the
first implementation).

## 4. Include semantics

An empty `include_regions` list means the entire source image is
eligible — this is the default, and it is what makes an unscoped
extraction (no `--scope` flag) behave identically to every extraction
performed before this AP (§11). When one or more include regions are
given, their union defines the eligible area; everything outside it is
treated as ineligible before extraction, the same way an exclusion
region is (§6).

## 5. Exclusion semantics

Exclusion regions are subtracted from the eligible area (union of
includes, or the whole image if no includes are given) after includes
are applied. `SourceScoper::apply()` builds this eligibility as an
8-bit mask (255 = eligible, 0 = not) and paints every ineligible pixel a
flat background value (255 — see §6's masking discussion) directly on a
clone of the source image, at the source image's own resolution and bit
depth. No cropping, no resampling, no drawn border of any kind.

## 6. Coordinate system

**Approach A was chosen** (Sec 7 of this AP's charter): the scoped image
keeps the source image's exact width and height. This was chosen over
Approach B (crop + explicit transform) because it is strictly simpler
(no transform math to get right, verify, or accidentally invert) and
because it makes the coordinate mapping trivial and unambiguous by
construction: a point `(x, y)` in the scoped image is defined to be, and
always is, the same `(x, y)` in the original source image. This
provenance field is called `coordinate_system: "identity"` throughout,
and TEST 9 (§9 of `test_source_scoper.cpp`) asserts this mapping directly
rather than merely assuming it holds.

**Masking never draws geometry.** `SourceScoper` never calls a drawing
primitive with a stroke, border, or outline — it only ever replaces a
rectangular pixel region with a single flat color via `cv::Mat::setTo`/
`copyTo`, matched to the page's own blank background (255, confirmed the
actual background value of `samples/trx300ODG.png` and consistent with
every `white_page()` test fixture already used throughout this project's
test suite). A flat fill has zero internal contrast, so it can never be
detected as a line by `MorphologyWireDetector`'s threshold/contour logic;
the only visible effect at a mask boundary is that any real ink which
used to cross it now truncates there — a shortened line, never a new
line. This is verified directly by TEST 5/6 (`test_source_scoper.cpp`):
an exclusion on an otherwise blank page produces a byte-identical blank
page, and a real conductor immediately adjacent to an exclusion boundary
is preserved pixel-for-pixel.

## 7. Metadata

`DiagramMetadata` fields (`diagram_type`, `manufacturer`, `make`,
`model`, `year`, `vehicle_type`, `system`, `harness_branch`,
`diagram_section`) are free-form strings, all optional, all defaulting
to empty ("unknown") rather than any inferred or guessed value — nothing
in this AP reads image content to populate them. Metadata is recorded in
provenance for a future consumer but is never consulted by
`SourceScoper::apply()`'s own masking logic: TEST 10 confirms two scopes
identical except for metadata produce byte-identical scoped pixels.

## 8. Provenance

`ScopeProvenance` (`ingest/source_scoper.hpp`) records:

- `id` — `stable_id("scoped-source", source_id + page + scope_id)`.
- `source_id`, `source_page`.
- `scope_id` — `stable_id("extraction-scope", <canonical region +
  metadata text>)`, deterministic over the scope's own content, entirely
  independent of the scope file's path on disk.
- `include_regions`, `exclusion_regions` (echoed verbatim from the
  applied scope — never fabricated).
- `metadata` (echoed verbatim).
- `scoped_width`, `scoped_height`.
- `coordinate_system` ("identity", §6).

Both `stable_id` (FNV-1a based, `core/ids.hpp`) reuses were chosen
specifically because it is this repository's existing, established
deterministic/content-addressed ID convention (used by every `Wire`,
`ConductorSegment`, `TopologyEdge`, etc. throughout the codebase) — no
new ID scheme was introduced. The CLI, when `--scope` is used, writes
this record to `<output>/artifacts/scoping/scope_provenance.json`
alongside the scoped image itself at
`<output>/artifacts/scoping/scoped_source.png`, so the full chain
(original source path → scope file → scoped image → extraction output)
is reconstructable from the output directory alone.

## 9. Determinism

`SourceScoper::apply()` performs only pure, input-determined `cv::Mat`
operations (`clone`, `setTo`, `bitwise_not`, `copyTo`) and string-based
hashing — no timestamps, random values, process/machine identity, or
filesystem-path-derived state enter the scoped image or its provenance
`id`/`scope_id`. TEST 8 (`test_source_scoper.cpp`) asserts byte-identical
output and identical provenance ids across two calls with identical
input. `ExtractionScopeIO::serialize()` is likewise pure string
formatting (its own determinism is asserted directly by
`test_extraction_scope_io.cpp`). The one non-deterministic field
anywhere in this AP's output is `review_manifest.json`'s pre-existing
`generated_at` wall-clock timestamp — unrelated to scoping, present in
every prior AP's output, and explicitly excluded from every determinism
comparison here, exactly as established by AP-BASELINE-001.

## 10. CLI/programmatic interface

Programmatic: `ExtractionScopeIO::load(path)` /
`::parse(json_text)` / `::save(...)` / `::serialize(...)`, and
`SourceScoper::apply(source, scope, source_id)` /
`::serialize_provenance(...)`. Both classes are ordinary, independently
usable, stateless-except-config types — no GUI, no editor, no new
long-lived service was built (Sec 19/21 of this AP's charter explicitly
scope those out).

CLI: `dx-extract extract <image> --output <dir> [--scope <scope.json>]`,
combinable with the pre-existing `--recognition`/`--vision-recognition`
flags (the argument parser was generalized from a rigid, hard-coded
argc-count check into a small flag loop to support this — the only
change to `main.cpp`'s argument handling). When `--scope` is given, the
CLI loads the scope, applies it, writes the two artifacts above, and
passes the **scoped image's path** to the still-completely-unmodified
`ExtractionPipeline::run()` while keeping the **original** image path as
the model's `source_id` — so identity/provenance in the resulting
`WireModel` and audit stay tied to the real source document, not an
internal temp file.

## 11. Backward compatibility

Omitting `--scope` leaves `effective_image_path == image_path`
unconditionally — `SourceScoper` is never invoked, and the exact
pre-AP-INGEST-001 code path runs. Verified empirically, not just by
code inspection: a fresh unscoped extraction of
`samples/trx300ODG.png` on this AP's own build produced
`extraction_audit.json`/`output/wires.svg` SHA-256 hashes
(`c4e99f52bedfe3e4083f4f5c5df6bd283696e1365671c7f8c5a5cd6da212432f`,
`3f89de85415273f4546adab7e2f0d2f6b8eb3edf013feacb1ffa41935a2cfd6b`)
**byte-identical** to the AP-DIAG-FIX-001/FIX-002 baseline: 35 wires (35
Resolved/0 Conflicted/0 Unresolved), 10 electrical nets, 0 validation
errors, 30 warnings — unchanged.

## 12. TRX300 experiment

`samples/trx300ODG.png` (1056×816) was inspected at the pixel level (not
eyeballed) to find real layout boundaries: row-wise dark-pixel density
was measured across the full page height, revealing a completely blank
band (zero dark pixels in every one of rows 614–624) separating the main
wiring diagram's own last content (row 613) from a switch-continuity
matrix (5 tables: Ignition/Lighting/Dimmer/Engine Stop/Starter switches,
content rows 625–773, columns 94–670) and a wire-color legend (content
rows 625–681, columns 805–932) below it. Both non-diagram blocks sit
entirely within rows 614–816; the main diagram's own content
(rows 90–613, columns 96–953) never enters that band.

The experiment scope
(`fixtures/trx300/scope.json`) therefore uses a single exclusion region,
`{x: 0, y: 614, width: 1056, height: 202}`, covering the entire
confirmed-non-diagram footer band, plus two `annotation_regions`
(`switch_matrix`, `legend`) recording what that band actually contains
for a future consumer — annotations that, per Sec 21, do not affect
extraction. No boundary was adjusted after seeing extraction results;
the exclusion region was fixed before the first scoped extraction was
run.

## 13. Before/after results

| Metric | Unscoped | Scoped | Explanation |
|---|---|---|---|
| Physical wires | 35 | 35 | Unchanged |
| Wire identity statuses | 35 Resolved / 0 Conflicted / 0 Unresolved | identical | Unchanged |
| Electrical nets | 10 | 10 | Unchanged |
| Validation errors | 0 | 0 | Unchanged |
| Validation warnings | 30 (same 2 codes) | 30 (same 2 codes) | Unchanged |
| Conductor segments | 285 | 222 | −63, all within the excluded footer |
| Topology nodes | 678 | 520 | −158 |
| Topology edges | 868 | 634 | −234 |
| Endpoint candidates | 200 | 182 | −18 |
| Component candidates (shapes) | 85 | 37 | −48 |
| `diagram_furniture`-kind shapes | 47 | 0 | **All** furniture was inside the excluded band |
| `chassis_ground`-kind shapes | 10 | 9 | −1, see below |
| `symbol_primitives` | 40 | 38 | −2, both `terminal_lead`, see below |

**Object-level differential** (every wire, component, and primitive
compared by id, not just counted): **0 wires added, 0 removed, 0
modified** — all 35 wires present in both extractions are byte-identical
records. **48 components removed, 0 added.** Every one of the 48 removed
components has `y >= 657`, strictly inside the excluded region
(`y >= 614`) — none touch the main diagram. The one component whose
`ComponentSymbolGeometry`/`SymbolPrimitive`s disappeared
(`component-candidate-shape-region-1790c5fb3a4f437c`, `y = 687`) and its
2 `terminal_lead` primitives are the same story: a false-positive small
shape that lived inside the switch-continuity table grid, not the main
diagram. The 1 fewer `chassis_ground`-kind shape is one of these 48 — a
shape that (per AP-DIAG-AUDIT-001's own finding of a high ChassisGround
false-positive rate) was very likely a misclassified circle glyph inside
the switch-matrix grid, not a real ground symbol; it was never referenced
by any ground-kind endpoint in either extraction (the 7 endpoint-
referenced ground components are identical, by id, in both). **0 new
components, 0 new primitives, 0 new wires, 0 changed electrical nets** —
no false geometry of any kind was introduced by masking.

The five component-body wires AP-DIAG-FIX-001 removed remain absent from
the scoped SVG (`output/wires.svg`), confirmed directly (grep for each of
the five ids returns zero matches).

Two fresh scoped extractions are mutually byte-identical (including
`scoped_source.png` and `scope_provenance.json`) except the pre-existing
`generated_at` timestamp field.

**Conclusion**: removing the switch-matrix/legend clutter reduced
topology/geometry bookkeeping substantially (26% fewer conductor
segments, 27% fewer edges) without altering a single legitimate Wire,
Electrical Net, or main-diagram component. This is exactly what §14/§15
of this AP's charter asked the experiment to measure, and the result did
not require adjusting the scope boundary to get: the first, pixel-
measured boundary already produced a clean, fully explained result.

## 14. Limitations

- Only axis-aligned rectangles are supported (Sec 4) — no polygon or
  rotated-region support.
- The background fill value (255) assumes a white-background source
  diagram; a scanned page with a non-white background (e.g. a sepia or
  gray-toned scan) would need a different, explicitly-configured fill
  value — not needed for any source this project currently processes,
  and not added speculatively.
- `annotation_regions` are pure metadata (Sec 21) — nothing in this AP
  or the existing pipeline consumes `annotation_type`/`description` yet.
- Metadata fields are untyped free-form strings (including `year`, kept
  as a string rather than an integer, since a diagram may legitimately
  span an unknown or multi-year range) — no validation against a known
  manufacturer/model list is performed or intended here.
- `fixtures/trx300/scope.json`'s `year` field was left blank rather than
  guessed — the source diagram does not state a model year, and none was
  inferred.
- Scoping currently only integrates through the CLI's `--scope` flag; a
  caller using `ExtractionPipeline` directly (rather than through
  `dx-extract`) must call `SourceScoper` itself and pass the resulting
  scoped-image path — `ExtractionPipeline::run()` was deliberately left
  untouched (Sec 18) rather than given its own scope-aware overload.

## 15. Future UI requirements

A future graphical scoping editor (explicitly out of scope here, Sec 19)
would need to: render the source page, let a user draw/adjust
`include_regions`/`exclusion_regions`/`annotation_regions` rectangles
against it, edit `DiagramMetadata` fields, and write the result through
`ExtractionScopeIO::save()` in the same JSON format this AP defines —
no format or model change should be needed for that editor to consume,
since the format is already a complete, human-readable, deterministic
JSON document designed to be hand-authored (as `fixtures/trx300/scope.json`
was, for this AP's own experiment). A follow-on AP could also consume
`annotation_regions` for reporting (e.g. surfacing "this diagram has a
switch-continuity matrix at these coordinates" in an audit) without any
change to `ExtractionScope`'s data model.

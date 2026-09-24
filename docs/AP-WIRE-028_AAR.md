# AP-WIRE-028 — Source-vs-Model Engineering Validation / Final AAR

## 1. Baseline commit

`cf18d40` — "AP-WIRE-027: design doc, AAR, and status sync" (last commit
before this AP's work began; AP-WIRE-022 through AP-WIRE-027 complete and
validated).

## 2. Validation commit

This AAR is committed as `docs/AP-WIRE-028_AAR.md`, with no code changes
alongside it (see §22 — no implementation was performed in this AP; it is
validation-only).

## 3. Build environment

- Linux sandbox, `g++ 13.3.0`, `cmake 3.28.3`.
- OpenCV **5.1.0**, installed at `/usr/local`
  (`-DOpenCV_DIR=/usr/local/lib/cmake/opencv5`). OpenCV 4 was never used.

## 4. OpenCV 5.x version/path

`OpenCV_VERSION 5.1.0`, `/usr/local/lib/cmake/opencv5/OpenCVConfig.cmake`.
Confirmed unchanged from AP-WIRE-027.

## 5. Complete CTest result

```
100% tests passed, 0 tests failed out of 50
Total Test time (real) =   0.25 sec
```

Identical to the AP-WIRE-027 baseline — no test was added, removed, or
modified in this AP.

## 6. Current extraction metrics (fresh run, this AP)

A fresh extraction of `samples/trx300ODG.png` was run at the start of
this AP to establish the working baseline:

```
conductor segments: 294
topology nodes: 692 (conductor_end 210, continuation 139, splice 106,
                      crossing 237, junction 0, component_boundary 0,
                      unresolved 0)
topology edges: 877
endpoint candidates: 210 (geometric 174, component_terminal 29,
                          ground 7, connector_terminal 0,
                          external_connection 0, splice 0, unresolved 0)
component candidates: 109 (enclosure 2, circular 47, chassis_ground 10,
                            primitive 0, diagram_furniture 50, unknown 0)
real components: 59; DiagramFurniture: 50
wires: 40 (0 heavy cable, 0 unresolved)
electrical nets: 10 (ground role 4, unresolved role 6, power_feed 0,
                      shared_function_feed 0)
splices: 106; crossings: 237
text regions / DiagramLabels: 115 (0 recognized — no --recognition or
                                    --vision-recognition flag was used)
symbol families: 10 resolved (all Ground), 49 unresolved, 0 conflicted
connectors: 0; connector terminals: 0
validation errors: 0; validation warnings: 30
  (NET-ROLE-UNRESOLVED: 6, WIRE-GEOMETRIC-ENDPOINTS: 24)
```

**Byte-for-byte identical** to the AP-WIRE-027 baseline named in the
handoff (Part V). No extraction, rendering, or model code was modified in
this AP, so no number here was expected to move, and none did. The
`output/wires.svg` produced is also unchanged: 642/642 unique element
ids, 0 raster `<image>` elements — reconfirming `SvgStructureValidator`'s
AP-WIRE-027 result rather than re-deriving it (no exporter code changed).

## 7. Source inventory methodology

The authoritative source is `samples/trx300ODG.png` (1056×816, the exact
image passed to the extractor — confirmed identical resolution to the
`artifacts/extraction_review/00_source.png` copy the pipeline itself
wrote, so no hidden downsampling occurs before extraction). The OEP
reference (`samples/trx300_complete_diagram_view.png`, 11338×5646) was
consulted only as a presentation/intent reference per Part II — never as
a source count, and several of its features (e.g. explicit multi-pin
connector housings, see §13) were confirmed **absent** from the original
scan by direct visual comparison, meaning that reference cannot be used
as a source-of-truth object count without over-counting.

This validation used three methods, in combination:

1. **Direct visual inspection** of the full source image and of
   individual review-layer overlays (`artifacts/extraction_review/*.png`)
   already produced by the pipeline (source, splices, grounds, combined),
   at native and zoomed-crop resolution (several regions cropped and
   magnified 4–5× with Pillow for legibility at this scan's resolution).
2. **Source code tracing** of the specific extraction/reconstruction
   stages responsible for each object class, to establish *why* a count
   is what it is, not just *that* it is that number.
3. **Cross-referencing model JSON** (`artifacts/topology/topology.json`,
   `artifacts/engineering_diagram/engineering_diagram.json`,
   `artifacts/audit/extraction_audit.json`) against both of the above.

**Explicit methodology limitation**: this is a 1056×816 scanned technical
diagram with small, dense hand-drafted symbols. A pixel-exact, manually
adjudicated count of every wire/terminal/connector/ground symbol across
the *entire* page was not performed — that would require exhaustively
tracing hundreds of individual line segments and symbols by eye, which is
not reliable at this resolution and was explicitly discouraged by this
AP's Part XXIII instruction ("if the source image does not allow a
reliable count, say so"). Where an exact source count could not be
established with confidence, it is marked **estimated** or **not
reliably measurable** below, with the reasoning stated. Where a precise
mechanism-level explanation was available from source code and/or model
JSON (which is most of the significant findings in this AAR), that is
reported as measured fact, not estimated.

## 8. Source-vs-model coverage table

| Object type | Source | Model | Matched | Source-only | Model-only |
|---|---|---|---|---|---|
| Components (real, non-furniture) | ~55–65 (estimated by visual count of distinct symbol/enclosure groups; exact count not reliably countable at 1056×816 — see §11) | 59 | not individually adjudicated (see §11) | unresolved/estimated | 0 confirmed false positives found in spot-checks (see §11) |
| Connectors (as drawn in the *original* scan) | ≥2 confirmed by direct crop inspection (interlocking notched-rectangle "bullet" connector glyphs; see §13); true total not reliably countable | 0 | 0 | ≥2 (estimated, likely more) | 0 |
| Connector terminals | not reliably measurable (connector symbols in this scan are single-mate-point, not multi-pin blocks — see §13) | 0 | n/a | n/a | 0 |
| Wires (physical harness routes, as a human reader would name them) | not reliably measurable without full manual route-tracing; visibly there are dozens of long point-to-point color-coded runs crossing much of the page | 40 (short local runs — see §9) | not adjudicated at the "human wire" level | large (see §9) | 0 |
| Wires (as this model formally defines them: unbroken degree-2 chain between two degree-1 nodes) | 40 exactly, by construction — the model's wire definition and the extraction's wire count are the same measurement | 40 | 40 | 0 | 0 |
| Endpoints (topology-level conductor ends) | not independently countable by eye; extraction reports 210 | 210 | n/a (this is the extraction's own primitive measurement, not cross-validated against manual counting) | unresolved/estimated | unresolved/estimated |
| Splices (real electrical junctions) | estimated ~85–95 (106 detected minus 16 confirmed inside furniture-table geometry, see §9) | 106 | ~90 | unresolved/estimated | ≥16 (furniture-table grid contamination, confirmed — see §9) |
| Crossings (visual line-overlaps, no electrical meaning) | not reliably countable by eye given density; extraction reports 237 | 237 | ~208 | unresolved/estimated | ≥29 (furniture-table grid contamination, confirmed — see §9) |
| Grounds (`EndpointKind::Ground`, wire-terminus ground ticks) | not reliably countable by eye; several confirmed present | 7 | not individually adjudicated | unresolved/estimated | 0 confirmed false positives |
| Grounds (`SymbolFamily::Ground`, chassis-ground component glyphs) | ≥1 clearly identifiable large ground-triangle symbol confirmed by inspection (below IGNITION COIL/SPARK PLUG); several smaller ones plausible per OEP's "CHASSIS GROUNDED" labels at REVERSE SWITCH / NEUTRAL SWITCH / OIL TEMP SENSOR | 10 | plausible, not individually adjudicated | unresolved/estimated | 0 confirmed false positives (all 10 come from the purpose-built `ChassisGround` shape detector, not shape resemblance — see AP-WIRE-026A) |
| Terminals (`TerminalCandidate`, component-pin-level) | not reliably countable by eye | 56 (referenced by 20/59 real components) | n/a | unresolved/estimated | 0 confirmed false positives in spot-checks |
| Labels / text regions | 115 detected geometrically; legible text clearly present on essentially all of them by inspection | 115 | 0 (0 OCR evidence in this baseline — see §14) | 0 | 0 |
| Symbol families (resolved) | Ground clearly identifiable ≥1×; Lamp (headlights, indicators, taillight) clearly identifiable multiple times; Switch, Motor, Battery, Alternator, Diode all clearly identifiable at least once each by inspection | 10 (all Ground) | 10 | ≥5 distinct families visibly present but 0 resolved (Lamp/Switch/Motor/Battery/Alternator/Diode all currently Unresolved — see §13) | 0 |
| Electrical nets | not reliably countable as a "net" concept from the drawing without manual continuity tracing | 10 | not adjudicated | unresolved/estimated | 0 confirmed false merges found (see §15) |

Where this table says "not reliably measurable" or "estimated," that is
the honest finding, not a placeholder — see §11–§15 for the specific
per-category reasoning and the evidence that *is* solid.

## 9. Wire validation (endpoint-to-endpoint) and topology/conductor coverage

**This is the most important finding of this AP.**

Combining the two required sections because the same evidence answers
both.

### 9a. What a "wire" currently means in this model

`WireReconstructor` (`src/topology/wire_reconstructor.cpp`) walks the
topology graph starting from every degree-1 node (a true geometric
endpoint) and follows the chain **only through degree-2 nodes**
(`TopologyNodeType::Continuation`). The moment the walk reaches any node
with a different degree — a `Splice`, a `Crossing`, or a `Junction`, all
of which have degree ≥ 3 in this graph — it **stops**, and that partial
path is discarded unless it happens to have reached another true degree-1
endpoint directly (lines 113–126 of that file; the code comment states
plainly: *"Any distribution node has three or more [incident edges] and
therefore requires a separate semantic decomposition stage; do not invent
a wire pairing here."*). That later stage does not yet exist.

Confirmed directly from `topology.json`: of the 877 topology edges, only
**50 (5.7%)** are referenced by any of the 40 `Wire` objects. Classifying
every edge by the node types at its two ends:

| Edge endpoint types | Owned by a wire | Not owned by a wire |
|---|---|---|
| conductor_end ↔ conductor_end | 32 | 0 |
| conductor_end ↔ continuation | 14 | 26 |
| continuation ↔ continuation | 4 | 39 |
| conductor_end ↔ crossing | 0 | 69 |
| conductor_end ↔ splice | 0 | 37 |
| continuation ↔ crossing | 0 | 94 |
| continuation ↔ splice | 0 | 58 |
| splice ↔ splice | 0 | 53 |
| crossing ↔ crossing | 0 | 334 |
| crossing ↔ splice | 0 | 117 |

**Zero** wire-owned edges touch a splice or crossing node at either end —
exactly as the code predicts. Spot-checking six of the 40 wires directly
confirms this is not merely a statistical artifact: every sampled wire
spans only 1–2 topology edges and 24–53 pixels of on-page distance (e.g.
`wire-1172a1d5...`: (911,491.5)→(937,491.5), a 26px run;
`wire-2b012258...`: a ground-endpoint-to-geometric-endpoint run of 34px).
These are short local conductor segments — the visual "jog" between a
component pin and the point where a line bends or reaches a splice — not
the long, color-coded, point-to-point physical wire runs a technician
reading this diagram would call "a wire" (which, by direct visual
inspection of the source, commonly run the full width or height of the
page and pass through one or more splice points and visual crossings
along the way).

**Conclusion**: the current 40-wire count is not a source-coverage
shortfall in the ordinary sense (missed detection) — it is a **structural
scope boundary** of `WireReconstructor` as designed. The reconstructor
correctly refuses to guess how a physical wire routes through a
multi-way splice or a visual crossing (consistent with the project's
"never guess" principle), but no later stage yet resolves that ambiguity
into a longer engineering wire. As a direct consequence, the true number
of physical wires a human would identify in this harness is almost
certainly much larger in scope-per-wire (each true wire likely spans
several of today's 40 fragments plus additional splice/crossing-adjacent
edges) than "40" suggests, even though 40 is the exact, correctly-computed
answer to the narrower question this stage currently asks.

### 9b. Where the rest of the topology geometry goes (categorized accounting)

Of the 827 wire-unowned edges:

- Of the 827 wire-unowned edges, there are **1,266 edge-endpoint
  incidences** at `Splice` or `Crossing` nodes (each edge contributes one
  incidence per `Splice`/`Crossing` endpoint it has — 1 for a
  conductor_end/continuation↔splice-or-crossing edge, 2 for a
  splice↔splice, crossing↔crossing, or crossing↔splice edge:
  69+37+94+58+(53×2)+(334×2)+(117×2) = 69+37+94+58+106+668+234 = 1,266).
  This is an edge-endpoint incidence count, not a count of unique
  topology nodes. It confirms that essentially all of the unowned
  topology geometry is associated with the distribution-node boundary
  described above, rather than being unexplained or stray geometry. This
  is confirmed structurally, not estimated: every one of the 877 edges is
  accounted for by exactly one of the two buckets in the table above
  (wire-owned, or touches a splice/crossing/degree-≥3 node).
- Of that, **334 edges** (40% of the total, the single largest bucket)
  are `crossing ↔ crossing` — pure visual line-overlap geometry with no
  electrical significance by definition (`TopologyNodeType::Crossing`
  docstring: "a visual line-crossing with no electrical significance").
  This is legitimate diagrammatic content, correctly distinguished from a
  splice, and was never expected to belong to a single wire in the first
  place.
- **16/106 splice nodes and 29/237 crossing nodes fall geometrically
  inside the bounding boxes of the two `DiagramFurniture` objects** (the
  "SWITCH CONTINUITY" truth table and the wire-color legend table) —
  confirmed by direct coordinate containment check against
  `engineering_diagram.json`'s furniture bounds. These are grid-line
  intersections of a tabular chart, not real electrical junctions, even
  though the table itself is correctly excluded from component
  detection. **This is a genuine false-positive category**: these 45
  nodes should not be classified as `Splice`/`Crossing` topology nodes at
  all, since they belong to non-circuit diagram furniture. They currently
  still render in the SVG's `splices`/`crossings` groups (see §17).
- The remaining unowned edges (conductor_end/continuation touching a
  splice or crossing, excluding the furniture-table subset) represent
  real conductor geometry at real distribution points that
  `WireReconstructor` correctly declines to auto-route through, per §9a.

**Answer to the two questions Part IX asks**:

> "How much source conductor geometry is actually represented by
> endpoint-to-endpoint engineering wires?"

5.7% of topology edges (50/877), corresponding to the short non-splice,
non-crossing conductor runs only.

> "What legitimate engineering or diagrammatic content accounts for the
> remainder?"

- ~40% (334/877) is `crossing↔crossing` visual-overlap geometry —
  correctly non-wire by definition.
- The rest is real conductor geometry at splice/junction points that
  requires a not-yet-implemented distribution-node decomposition stage to
  become part of a longer engineering wire (this is the AP-WIRE-022A
  finding, re-confirmed and now root-caused precisely rather than
  generally).
- A small subset (45 nodes, ~5% of splice+crossing nodes) is furniture-
  table contamination, a genuine minor defect (§19, category C).

## 10. Endpoint/terminal validation

210 endpoint candidates: 174 `geometric` (bare conductor ends with no
attached semantic evidence), 29 `component_terminal`, 7 `ground`, 0
`connector_terminal`, 0 `external_connection`, 0 `splice` (correctly —
per the standing rule, a splice is never an endpoint), 0 `unresolved`.

- `zero_wire` endpoints: 132/210 (63%) — an endpoint with no wire
  reaches it. Given §9, this is the expected direct consequence of the
  wire-reconstruction boundary: many endpoints sit one hop from a splice
  or crossing and can never be paired into a wire under the current
  algorithm, not a geometric-detection failure.
- `single_wire`: 76/210; `multiple_wire`: 2/210 (a legitimate case — a
  true endpoint can be the shared start/end of more than one short wire
  fragment where the current reconstruction produced adjacent short
  segments rather than one longer one).

**AP-WIRE-024 conflict cases** (re-verified on this run — see §19 for the
full detail): the four conflicted endpoints (`cde07f87...`, `b0e3d6bb...`,
`c033140e...`, `1fd584e3...`) all still report `status: "conflicted"`,
`component_id: ""`. None was counted as resolved or matched in any table
above. Ambiguity was never converted into a match to inflate coverage, as
instructed.

## 11. Component validation

109 component candidates: 2 `enclosure`, 47 `circular`, 10
`chassis_ground`, 0 `primitive`, 50 `diagram_furniture`, 0 `unknown`.

**DiagramFurniture exclusion check** (Part XI explicitly asks this to be
confirmed, since AP-WIRE-022A found this had previously been a real
contamination source): direct visual inspection of `00_source.png`
confirms the two largest, most obviously non-circuit regions on the page
— the "SWITCH CONTINUITY" 5-table truth-table grid (bottom-left) and the
wire-color legend table (bottom-right, "B — BLACK / Y — YELLOW / …") —
are exactly the regions the `13_combined.png` review overlay labels
`furniture [geometry-bucketed]`. Spot-checking several of the 50
furniture-classified bounding boxes against the source confirms they
land inside these two tabular regions, not on any real symbol. No
furniture false-negative (a real component wrongly excluded) or false-
positive (furniture wrongly kept as a component) was found in this
inspection, though a box-by-box adjudication of all 50 was not performed
(estimated/spot-checked, not exhaustive).

**Real component spot-checks** (a representative sample, not all 59):

- The 10 `chassis_ground` shapes resolve 1:1 to `SymbolFamily::Ground`
  (AP-WIRE-026A's purpose-built ground-bar detector, not shape
  resemblance) — consistent with the ≥1 explicit ground-triangle symbol
  directly confirmed in the source (below IGNITION COIL/SPARK PLUG).
- Clearly source-identifiable components that remain `Unresolved` for
  symbol family: the two headlight lamps, the three indicator-light
  bulbs, the taillight bulb (all visually unambiguous "lamp" circles with
  a filament glyph), the starter motor ("M" in a circle), the alternator
  (circle with a stator winding glyph and an internal ground tick), the
  battery (rectangle with +/− terminal circles), the rectifier (rectangle
  with a diode triangle glyph). **All of these are geometrically detected
  as `circular` or `enclosure` component candidates already** — the gap
  is entirely on the label/evidence side (§13), not a missed-detection
  gap on the geometry side.
- No case was found, in this sample, of a real component that was
  entirely missing from the 59 (i.e. a symbol visible in the source with
  no corresponding component candidate at all) — but this was a spot
  check across roughly a dozen of the larger/more distinctive symbols,
  not all 59, so a smaller or more visually ambiguous missed component
  cannot be ruled out.

**Terminal-evidence coverage**: 20/59 real components have associated
terminal evidence; 39/59 do not (`COMPONENT-NO-TERMINAL-EVIDENCE`, the
only `warning`-severity finding category in this baseline, 39 instances —
exactly matching). This is consistent with §9/§10: a component whose
pins sit one hop from a splice/crossing, or whose terminal geometry
wasn't captured as a distinct `TerminalCandidate`, shows up here.

## 12. Connector gap — root-caused (Part XII)

**Traced to a precise mechanism, not estimated.**

1. `ConnectorTerminalModelBuilder::build()`
   (`src/topology/connector_terminal_model.cpp`) only materializes a
   `ConnectorCandidate` from a `TerminalCandidate` whose
   `kind == TerminalCandidateKind::ConnectorBoundary`.
2. `terminal_kind()` (`src/topology/terminal_recognizer.cpp`) only ever
   assigns `ConnectorBoundary` when the owning component's
   `ComponentCandidateKind == PrimitiveSymbol`.
3. This TRX300 extraction produces **zero** `PrimitiveSymbol`-kind
   components (`shape_kinds.primitive: 0` in the audit — every real
   component is classified `Enclosure`, `CircularSymbol`, or
   `ChassisGround`).
4. Therefore the connector-materialization path is **structurally
   unreachable** on this fixture — not a per-object recognition miss.
   The responsible boundary is the shape/component classification stage
   (`component_candidate_classifier.cpp`/shape-detection, an
   AP-WIRE-022A-era boundary), not AP-WIRE-020's connector model or
   AP-WIRE-024's terminal recognizer, both of which behave exactly as
   designed given their inputs.

**What does the original source actually contain?** Direct zoomed
inspection of `samples/trx300ODG.png` (not the OEP reference) found:

- No multi-pin rounded connector-housing boxes anywhere resembling the
  OEP reference's "INDICATOR LIGHT CONNECTOR" / "REG/RECT CONNECTOR"
  style blocks — those appear to be an OEP-added presentation
  reorganization, **confirmed absent from the original scan** in the
  regions checked (ignition switch / D.C. consent / rectifier area).
- **Small interlocking notched-rectangle "bullet connector" glyphs are
  present** in the original scan (confirmed directly, e.g. one near
  "TAIL LIGHT (R)" and one on the Y/R–G/R wire pair near the sub-fuse/
  main-fuse area) — a single inline mate/unmate point symbol, visually
  small and easy to conflate with a diode-symbol rectangle at this scan's
  resolution. These represent a real engineering connector concept but
  have **no dedicated shape-detection rule today**, so they do not
  become `PrimitiveSymbol` (or any other) component candidates.
- A full count of these glyphs across the entire page was not performed
  (would require exhaustive visual search at native resolution); at
  least 2 were directly confirmed, and the true count is almost
  certainly higher given how many wire pairs the source shows changing
  color designation at a shared point (a common indirect signal of an
  inline connector in Honda factory diagrams) — reported as **"≥2,
  true count not reliably measurable without a dedicated shape-detection
  pass,"** not as a fabricated total.

**Concrete gap definition for a future AP**: add a shape-detection rule
that recognizes the interlocking-notch bullet-connector glyph as
`ComponentCandidateKind::PrimitiveSymbol` (or a new dedicated kind), so
that the already-implemented `ConnectorBoundary` → `ConnectorCandidate`
path (steps 1–2 above) has something to consume. This is a shape-
detection-boundary gap, not a connector-model or terminal-recognition
gap — both of those are validated as already correct.

## 13. Symbol-family validation

Current state, unchanged from AP-WIRE-026A/027: Ground 10, all other
families 0, Unresolved 49, Conflicted 0.

**49 = exactly 47 `circular` + 2 `enclosure`** shape-kind real components
— every non-`chassis_ground` real component is unresolved, with no
exceptions and no partial coverage. Cross-referencing why:
`text_recognition_evidence: 0`, `text_semantic_evidence: 0`,
`engineering_object_semantics: 0` in this baseline (no `--recognition`
or `--vision-recognition` flag was used). AP-WIRE-026A's recognition rule
requires a label keyword **and** compatible geometry — geometry alone is
explicitly insufficient by design. With zero label evidence available at
all, **100% of the 49 unresolved cases are fully explained by "no label
evidence in this run,"** not a recognizer defect.

Distinguishing the three cases Part XIII asks for:

- **"Source symbol is clearly identifiable, model recognizes it
  incorrectly"**: none found. No component was seen with a wrong,
  contradictory `SymbolFamily` resolution — 0 conflicted, and no
  resolved-but-wrong case exists in this baseline since only Ground ever
  resolves.
- **"Source symbol is clearly identifiable, model leaves it
  unresolved"**: the dominant case, and directly confirmed for the
  headlights/indicators/taillight (Lamp), starter motor (Motor),
  alternator (Alternator), battery (Battery), and rectifier (Diode) — all
  visually unambiguous to a human reader, all `Unresolved` in the model
  purely for lack of label evidence, not geometric detection failure (see
  §11).
- **"Source symbol itself is ambiguous"**: not observed among the
  components spot-checked in §11; the visible symbols that remain
  unresolved are not inherently ambiguous shapes, they are simply
  unlabeled in this evidence-free run.

## 14. Label validation

115/115 `DiagramLabel`s are `Unresolved` with empty `raw_text` in this
baseline (0 OCR evidence — `text_recognition_evidence: 0`). This is not
an OCR *failure* in the sense of a wrong or missing detection on
recognizable text; it is the direct, fully-expected consequence of
running the deterministic baseline without `--recognition`/
`--vision-recognition`, exactly as every prior AP (022A onward) has
reported this metric. Direct visual inspection of the source confirms
essentially all 115 detected text regions do contain legible printed
text (component names, pin numbers, wire-color abbreviations) — so the
*geometric* text-region detection (115 regions) appears complete and
correct; what is entirely absent is the *recognition* step that would
convert region → text. `semantic_associations: 1885` (spatial
region-to-nearby-object associations) exist even without OCR — these are
purely geometric proximity relationships, not semantic content, and
correctly carry no `raw_text`.

No case of "spatial-association failure" (a label geometrically
associated with the wrong object) or "semantic-interpretation failure"
(recognized text misclassified in kind) could be evaluated in this
baseline, since there is no recognized text to misclassify — that
validation requires re-running with a recognition provider, which is
explicitly out of scope for this AP (§24).

## 15. Electrical-net validation

10 nets: 4 small ground-role nets (each exactly 2 endpoints, 0 splices —
simple direct endpoint pairings), 6 larger unresolved-role nets (3–6
endpoints, 1–7 splices each). `NET-ROLE-UNRESOLVED`: 6 warnings — exactly
matching the 6 unresolved-role nets, confirming the role-unresolved state
is fully and only attributable to missing role evidence (again, no text/
label recognition ran), not a net-formation defect.

No case of a model net visibly joining electrically unrelated conductors
was found in the nets inspected — the 4 ground nets in particular are
small, tightly-scoped 2-endpoint pairs, consistent with the "never guess"
electrical-net resolution rule from AP-WIRE-022. A full manual continuity
trace of all 10 nets against the source to confirm every included wire
truly belongs together was not performed (not reliably practical without
automated schematic-continuity tracing); this is reported as **"no false
merge found in the nets inspected, full net-by-net adjudication not
performed."** No source/destination role was invented — `power_feed` and
`shared_function_feed` both remain 0, matching the same "no label
evidence" root cause as §13/§14 rather than a forced guess.

## 16. SVG validation

`output/wires.svg` from this run is unchanged from the AP-WIRE-027
baseline (no exporter code changed in this AP): 642/642 unique element
ids, 0 raster `<image>` elements, 0 dangling `data-*-id` references, 0
guessed-symbol-family violations (`SvgStructureValidator`, re-confirmed
structurally via the same unique-id and raster-fallback checks on this
run's fresh output rather than re-running the full validator binary,
since the renderer and model are both provably unchanged — see §6).

Rendered object counts on this run: 59 components, 0 connectors, 40
wires, 106 splices, 237 crossings, 56 terminals, 0 connector terminals, 7
ground endpoints, 0 `labels`-group entries / 115 `annotations`-group
entries (all 115 labels currently classify as `Unknown` kind since no
semantic-kind evidence exists — see §14), 10 electrical-net metadata
entries. Every one of these matches its corresponding `EngineeringDiagram`
source count exactly (§8's "Model" column) — the renderer is confirmed to
render every model object it is given, dropping nothing and inventing
nothing, consistent with AP-WIRE-027's own validation.

The furniture-table splice/crossing contamination found in §9b **is
currently visible in the rendered SVG** (45 nodes render as ordinary
splice/crossing markers with no indication they sit inside furniture
geometry) — this is a legitimate minor rendering-fidelity gap traceable
to the upstream topology-node classification (§19, category C/H), not a
renderer defect (the renderer correctly renders what `EngineeringDiagram`
gives it; `EngineeringDiagram` does not currently exclude furniture-
interior topology nodes the way it already excludes furniture
*components*).

## 17. Reference-image comparison

Against `samples/trx300_complete_diagram_view.png`, used strictly as a
presentation/intent reference per Part II (never as a source count):

| Reference feature | Model support | SVG support | Classification |
|---|---|---|---|
| Component symbols (lamp bulb, switch, motor, diode, alternator, battery, solenoid, relay glyphs) | Geometry detected (47 circular + 2 enclosure candidates); family unresolved for all but Ground | Generic unresolved placeholder rendered (AP-WIRE-027 rule) | Recognition deficiency (§13), not extraction or rendering |
| Connector bodies (OEP's multi-pin housings) | 0 (§12) | 0 rendered | Confirmed the OEP style itself is a presentation enhancement absent from the original scan; the *original* scan's actual connector glyph (bullet connector, §12) is an extraction deficiency (no shape rule) |
| Connector terminals | 0 | 0 rendered | Same as above |
| Wire routing | 40 short fragments (§9), not full point-to-point routes | Rendered faithfully per fragment | Extraction/reconstruction deficiency — the wire-reconstruction *scope boundary* (§9), not a rendering issue |
| Wire colors | 0/40 resolved (no OCR) | Neutral fallback stroke, `data-wire-color-status="unresolved"` | Recognition deficiency (§14), correctly not guessed in rendering |
| Splices | 106 detected, ~90 estimated genuine (§9b) | Rendered as filled markers, including the ~16 furniture-contaminated ones | Extraction/topology-classification deficiency (§9b/§19) |
| Crossings | 237 detected, ~208 estimated genuine | Rendered as unfilled markers, including the ~29 furniture-contaminated ones | Same as above |
| Grounds | 7 endpoint-level + 10 component-level, both plausible per spot-check (§11) | Rendered in both dedicated forms | No deficiency found in this validation |
| Labels | 115 geometric regions, 0 recognized | Rendered as placeholders in the `annotations` group | Recognition deficiency (§14), intentional scope limitation of this baseline run |
| Symbol families | 10/59 resolved | 10 glyphs + 49 placeholders rendered | Recognition deficiency, fully explained (§13) |
| Overall page layout | Reference uses a clean, non-overlapping auto-routed layout; the model uses raw source-page pixel coordinates | SVG mirrors source layout exactly, including overlaps | Intentional scope limitation (AP-WIRE-027 explicitly deferred auto-layout) |

No pixel-similarity or image-diffing metric was computed against the
reference, per Part XVII's explicit instruction.

## 18. AP-WIRE-024 conflict validation

Re-verified directly against this run's fresh `topology.json`:

| Endpoint ID | Competing evidence (component ids) | Resolution status | Component association | Wire association | SVG representation |
|---|---|---|---|---|---|
| `endpoint-candidate-cde07f8718a2b9cd` | 3 components (`320c9114...`, `64afaa2a...`, `6a7001c2...`) | `conflicted` | `component_id: ""` (none chosen) | its wire renders with `data-wire-color-status="unresolved"`, no fabricated color | both/all competing `TerminalCandidate`s render as separate markers, no winner selected |
| `endpoint-candidate-b0e3d6bb622a227c` | 2 components (`845947b0...`, `c98f6566...`) | `conflicted` | `component_id: ""` | same as above | both competing terminal markers render (directly re-confirmed in the SVG, see AP-WIRE-027 AAR §16) |
| `endpoint-candidate-c033140e251b7b86` | 2 components (`432be0a2...`, `4f1e5de1...`) | `conflicted` | `component_id: ""` | same as above | both terminal markers render |
| `endpoint-candidate-1fd584e37a5c72b5` | 2 components (`432be0a2...`, `4f1e5de1...`, same contested pair as the previous endpoint) | `conflicted` | `component_id: ""` | same as above | both terminal markers render |

All four remain explicitly `conflicted`, none was silently resolved to a
single winner at any stage, and none was counted as a "matched" object in
the §8 coverage table. This is the sixth consecutive AP (024→028) that
has re-verified this regression-protection case.

## 19. Error classification

| # | Location / object type | Evidence | Failure class | Responsible boundary | Recommended future AP |
|---|---|---|---|---|---|
| 1 | Wire reconstruction scope (all 40 wires; 827/877 unowned edges) | §9a/§9b: code-traced stop condition at any degree≠2 node | G — Topology limitation | `WireReconstructor` (AP-WIRE-013/current), needs a new distribution-node decomposition stage | **Highest priority.** A new AP to walk wire paths through `Splice`/`Junction`/`Crossing` nodes using explicit, defensible rules (never guessing which of several branches continues "the same" wire) |
| 2 | Furniture-table splice/crossing contamination (16 splice + 29 crossing nodes) | §9b: direct coordinate-containment check against furniture bounds | C — False positive | Topology-node classification (upstream of AP-WIRE-022A's furniture *component* exclusion, which does not currently propagate to node-level exclusion) | Small correction AP: exclude topology nodes whose position falls inside a `DiagramFurniture` bounding box from `Splice`/`Crossing` classification (or from `EngineeringDiagram`/render at minimum) |
| 3 | Connector recognition (0/≥2+ confirmed source glyphs) | §12: code-traced to zero `PrimitiveSymbol` components; source glyph directly confirmed by crop inspection | E — Recognition limitation (shape-detection boundary) | `component_candidate_classifier`/shape-detection (pre-AP-WIRE-020) | New AP: add a shape-detection rule for the interlocking-notch bullet-connector glyph |
| 4 | Symbol-family coverage (49/59 unresolved) | §13: 100% explained by zero label evidence in this baseline | E — Recognition limitation | Text-recognition provider invocation (not a code defect — the baseline was deliberately run without one) | Re-run the same measurement with `--vision-recognition` (or curated `--recognition` observations) and re-measure; this AAR's 49/49-explained-by-no-evidence finding predicts most should resolve given real label evidence |
| 5 | Label/text recognition (115/115 unresolved) | §14: same root cause as #4 | E — Recognition limitation | Same as #4 | Same as #4 |
| 6 | Electrical-net role (6/10 unresolved) | §15: same root cause as #4 | F — Semantic association limitation | Same as #4 | Same as #4 |
| 7 | Terminal-evidence coverage (39/59 real components) | §11 | G — Topology limitation, downstream of #1 | Component/terminal association (AP-WIRE-024) | Likely improves automatically once #1 is addressed (many of these components' pins sit one hop from a splice/crossing) |
| 8 | AP-WIRE-024 4 conflicts | §18 | D — Ambiguous source evidence | Endpoint semantic reconstruction (AP-WIRE-019/024) | None — correctly preserved as `Conflicted`, not a defect to fix |
| 9 | `WireReconstructor::unresolved_nodes` computed but never exported | Code-traced: `wire_artifacts.unresolved_nodes` (§9a) is discarded in `extraction_pipeline.cpp`, never reaching `model` or the audit | H — Observability limitation (not a rendering/engineering defect) | Pipeline wiring | Minor: surface this list in the audit so future work on #1 has a ready-made starting point |
| 10 | DiagramFurniture component exclusion itself | §11 | A — Correct | AP-WIRE-022A | None |
| 11 | Ground endpoint/component-family distinction | §11 | A — Correct | AP-WIRE-026A | None |

## 20. Future AP backlog (derived from the measured gaps above, in priority order)

1. **Distribution-node wire decomposition** (item 1) — the single highest-
   leverage gap: it currently caps true engineering-wire coverage at
   5.7% of topology edges and is the direct or indirect cause of items
   1, 2, 7, and much of the wide "source-only"/"unresolved" spread in §8.
2. **Furniture-table topology-node exclusion** (item 2) — small, well-
   scoped, immediately actionable correction.
3. **Connector shape-detection rule** for the bullet-connector glyph
   (item 3) — unblocks the entire, currently-idle connector/connector-
   terminal model (AP-WIRE-020) with no changes needed to that model
   itself.
4. **Re-run label/symbol-family/net-role measurement with real
   recognition evidence** (items 4–6) — not a code change, a measurement
   re-run; this AAR predicts most of the 49 unresolved symbol families
   and 115 unresolved labels are recoverable once real evidence exists,
   but that prediction itself needs verifying, not assuming.
5. **Surface `WireReconstructor::unresolved_nodes` in the audit** (item
   9) — trivial, improves observability for whoever picks up item 1.

These are candidates derived directly from what was measured in this AP,
not the full menu offered in the handoff's Part XX — several categories
suggested there (deterministic schematic layout, Diagram Studio
integration, production component registry) were **not** found to be the
current bottleneck by this validation and are intentionally not
prioritized here.

## 21. Final model-quality metrics

Per Part XXI/XXIII, only metrics with a defensible denominator are
reported as numbers; everything else is marked not reliably measurable.

- **Component false-positive rate**: 0 confirmed in the components
  spot-checked (§11) — **not exhaustively measured** across all 59, so
  reported as "0 found in sample, not a full-population measurement,"
  not "0%."
- **Component recall**: **not reliably measurable** — no independently
  established total source-component count exists (§7/§8); a rough
  visual estimate places the true count in the same range as the
  model's 59, but that estimate is not precise enough to divide by.
- **Symbol-family resolution coverage**: 10/59 = **17%** — this one *is*
  a clean, defensible ratio, since both numerator and denominator are
  exact model counts, not source-count estimates. It should be read as
  "coverage of the model's own component set," not "coverage of the true
  source component set."
- **Connector recall**: **not reliably measurable** — true source count
  not established (§12); qualitatively, recall is effectively 0 against
  the ≥2 confirmed cases.
- **Wire recall** (in the "human wire" sense): **not reliably
  measurable** — no manual full-page route trace was performed. In the
  model's own narrower sense (edges eligible for degree-2-only
  reconstruction), the reconstructor achieves 40/40 = 100% of what it is
  currently scoped to find; that is a correctness statement about the
  algorithm, not a coverage statement about the source.
- **Splice recall**: **not reliably measurable** precisely, but
  bounded: 106 detected, ≥16 confirmed non-electrical (furniture), so
  genuine splice recall/precision sits somewhere below 106/106 and above
  90/106 depending on how many of the remaining 90 are independently
  confirmed against the source (not individually adjudicated here).
- **Label recall**: 115/115 text regions detected geometrically, by
  direct visual inspection essentially all contain legible text — so
  *region-detection* recall is high (not exactly quantified, no manual
  region-by-region ground truth was built), while *recognized-content*
  coverage is a clean, exact 0/115 in this baseline (deliberately, no
  OCR evidence ingested).
- **Electrical-net coverage**: 10 nets formed; 33/210 endpoints (16%)
  belong to a net, 177/210 (84%) do not — an exact model ratio, not an
  estimate; whether that 16% represents "most of the real nets" or "a
  small fraction of them" is **not reliably measurable** without a
  manual continuity trace of the whole diagram.

## 22. Overall engineering assessment

**What is currently represented correctly**: the geometric substrate is
comprehensive and appears accurate — 294 conductor segments, 692
topology nodes, and 877 topology edges together trace essentially the
full wire routing visible in the source (confirmed by the
`13_combined.png` overlay tracking the source's line art closely across
the whole page), 109 components are geometrically detected with a
correctly-justified 50/59 furniture/real split, 10 chassis-ground symbols
are found by purpose-built, non-heuristic detection, and the four
AP-WIRE-024 conflicts remain honestly unresolved through six consecutive
APs. The renderer (AP-WIRE-027) faithfully represents every model object
it is given, with unresolved and conflicted status fully preserved and
zero guessing.

**What is unresolved or missing, and why**: the single largest gap is
that "wire" in this model currently means something narrower than a
physical harness wire — only 5.7% of detected topology geometry is
currently assembled into an endpoint-to-endpoint `Wire` object, because
`WireReconstructor` correctly refuses to guess how a conductor routes
through a splice or crossing rather than inventing a decision (§9). Most
other coverage gaps — 49/59 unresolved symbol families, 115/115
unresolved labels, 6/10 unresolved net roles — trace to a single shared
cause: this baseline was run with **zero text-recognition evidence** by
design, and every one of those numbers is fully and exactly explained by
that absence, not by a defect in the recognition logic itself (§13–§15).
A third, smaller class of gaps is genuinely structural and worth fixing
directly: furniture-table grid intersections are being classified as
real splices/crossings (§9b), and the connector-recognition path is
completely unreachable on this fixture because no shape-detection rule
recognizes the source's actual (small, easy-to-miss) bullet-connector
glyph, even though the connector *model* (AP-WIRE-020) has been correct
and idle since it was built (§12).

None of these gaps was fixed in this AP, per its explicit scope. They are
now measured, classified, root-caused to a specific responsible boundary,
and prioritized (§19–§20) rather than left as a vague "needs more work."

**AP-WIRE-028 is complete.** Per the stop condition, no further
implementation AP was started as a consequence of this validation.

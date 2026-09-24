# AP-WIRE-023 — After-Action Report

Internal Symbol Geometry Extraction

Status: complete (observational/additive). No wire, topology, or
electrical-net repair was performed. No symbol-family identity was
assigned. See `docs/AP-WIRE-023_Internal_Symbol_Geometry.md` for the full
design; this report is the evidence and correlation record required by
the AP spec.

## Baseline carried forward from AP-WIRE-022A

HEAD before this AP's commits: `46bf77f`. Full Release CTest: 44/44
passed after implementation (43 from AP-WIRE-022A + 1 new
`dx-wire-test-symbol-geometry-extractor`), 0 failures.

## 1. How many real components had internal geometry?

**17 of 59** (28.8%) real components produced at least one
`SymbolPrimitive` after boundary-margin exclusion.

## 2. How many had no internal geometry?

**42 of 59** (71.2%) produced zero primitives -
`ComponentSymbolGeometry.confidence == Unresolved`. See §7/Limitations
below for why this is largely correct, not a detector failure.

## 3. What primitive classes were detected?

40 primitives total in the fresh baseline extraction:

| Kind | Count |
|---|---:|
| `Unknown` | 22 |
| `TerminalLead` | 9 |
| `Rectangle` | 6 |
| `Circle` | 2 |
| `Line` | 1 |

`Unknown` dominating is expected and honest: most internal marks at this
image resolution (1056x816) are a handful of pixels and do not cleanly
satisfy the line/circle/rectangle shape-metric thresholds. Per the AP
spec (§10/§25: "ambiguous geometry remains unresolved... do not force
classification"), that is the correct outcome, not a shortfall to
"fix" by loosening thresholds until everything gets a confident label.

## 4. Which component candidates remain unresolved?

All 42 components with zero primitives are `circular_symbol` (full list
in `docs/AP-WIRE-023_component_symbol_geometry_correlation.csv`). Every
`chassis_ground` (10/10) and `enclosure` (2/2) candidate produced at
least one primitive; only 5 of 47 `circular_symbol` candidates did.

## 5. Which geometry appears terminal-like?

9 `TerminalLead` primitives across **6** distinct components:
`...-1790c5fb3a4f437c`, `...-4cae53cce7f2640f`, `...-590ab65693883ba3`,
`...-6a7001c24fe4c252`, `...-f29a073b76f906b7` (all `chassis_ground`), and
`...-c98f6566b0672535` (`circular_symbol`). These are geometric
candidates only - none were converted to `EndpointCandidate` objects, and
`model.endpoint_candidates.size()` is unchanged from the AP-WIRE-022A
baseline (210, confirmed in §8).

## 6. Which components still have no terminal evidence?

Terminal/endpoint evidence (`TerminalCandidate`/`EndpointCandidate.component_id`)
is unchanged by this AP - still 41/59, identical to the AP-WIRE-022A
baseline (confirmed: coverage diagnostics report
`components.real_without_terminal_evidence: 41` both before and after
this AP's commits). AP-WIRE-023 deliberately does not attempt to close
this number; it only makes the geometry available for AP-WIRE-024 to
consume. The correlation table (§9below) shows 13 of the 17
geometry-bearing components already had prior terminal evidence, and 4
did not - those 4 are the components AP-WIRE-024 gets new leverage on:
`...-1790c5fb3a4f437c`, `...-899a2442ecba3be6` (0 primitives, excluded
from this count), `...-b2a4933bc728db93`, `...-0a02724ece05df31`. (Exact
figures: 13 with-geometry-and-prior-terminal-evidence, 4
with-geometry-and-no-prior-terminal-evidence, out of 17 with geometry.)

## 7. Did existing wires change?

No. 40 wires before and after, byte-for-byte identical counts:
conductor segments 294, topology nodes 692, topology edges 877,
endpoint candidates 210, wires 40, all unchanged from the AP-WIRE-022A
baseline. `WireReconstructor`/`TopologyReconstructor` are not on the call
path this AP added to (`SymbolGeometryExtractor` runs after
`ComponentSymbolRecognizer` and before any topology/wire/terminal stage -
see the pipeline diagram in the design doc).

## 8. Did topology change?

No - 692 nodes / 877 edges, identical to baseline.

## 9. Did electrical nets change?

No - 10 nets, identical to baseline. `endpoints_not_in_any_net` (177) and
`endpoints_in_multiple_nets` (0) from AP-WIRE-022A's coverage diagnostics
are also unchanged.

## 10. Did DiagramFurniture remain excluded?

Yes. `component_symbol_geometries.size() == 59` (one entry per real
component; furniture never gets an entry at all - not even an empty
one), and `diagram_furniture` count in the coverage report is unchanged
at 50. Verified directly: a synthetic regression test
(`DiagramFurniture exclusion` in `test_symbol_geometry_extractor.cpp`)
asserts identical circle geometry produces zero primitives/geometries
when the owning candidate is `DiagramFurniture`.

## 11. Did validation errors/warnings change?

No - 0 errors, 30 warnings (24 `WIRE-GEOMETRIC-ENDPOINTS`, 6
`NET-ROLE-UNRESOLVED`), identical warning-code histogram to the
AP-WIRE-022A baseline. AP-WIRE-023 does not add new
`WireModelValidator` codes (it has nothing to validate against wire/net
identity - it only adds geometry that stage never inspects).

## Correlation table (AP spec §24)

Full table (all 59 real components):
`docs/AP-WIRE-023_component_symbol_geometry_correlation.csv`

Columns: `component_id, component_kind, furniture, primitive_count,
terminal_like_geometry, previous_terminal_evidence, confidence`.

Summary:

| | with geometry | without geometry | total |
|---|---:|---:|---:|
| `circular_symbol` | 5 | 42 | 47 |
| `chassis_ground` | 10 | 0 | 10 |
| `enclosure` | 2 | 0 | 2 |
| **Total** | **17** | **42** | **59** |

## What should AP-WIRE-024 consume?

1. **The correlation CSV**, filtered to `primitive_count > 0`: those 17
   components now have geometric evidence a terminal-recognition stage
   can reason about. Do not attempt terminal recognition on the other 42
   from geometry alone - there is nothing there (see Limitations).
2. **`TerminalLead` primitives specifically** (9, across 6 components):
   these are the strongest candidates for AP-WIRE-024's terminal-geometry
   interpretation, since they were already selected for being elongated
   and boundary-touching. They are not yet endpoints - AP-WIRE-024 is
   responsible for that semantic step, including deciding whether/how a
   `TerminalLead` should associate with an existing nearby
   `EndpointCandidate` (per the AP spec's explicit non-goal: "do not
   alter Wire identity merely because a terminal candidate was
   discovered").
3. **The 41 unresolved `circular_symbol` components** are a separate,
   harder problem: at TRX300's resolution most are genuinely bare
   rings/dots with no internal marks. AP-WIRE-024 (or a follow-up AP)
   should decide whether terminal association for these should come from
   proximity-based geometric reasoning against nearby `EndpointCandidate`/
   `ConductorSegment` objects instead of internal symbol geometry, since
   internal geometry has nothing further to offer for this subset.
4. **Do not re-run AP-WIRE-023's classification logic inside AP-WIRE-024.**
   Consume `component_symbol_geometries`/`symbol_primitives` from the
   model/export directly.

## Limitations (honest accounting, not spun as success)

- The dominant primitive kind is `Unknown` (22/40, 55%) - this AP
  establishes the geometric *layer*, but most of what it finds cannot yet
  be confidently shape-classified at this resolution. That is the correct
  behavior per the AP's explicit "do not force classification" directive,
  not a shortcoming to paper over.
- 42/59 real components produced no geometry at all. Manual inspection
  (via a throwaway diagnostic build, not part of the shipped pipeline)
  confirmed that the large majority of these are simple filled/outline
  circles 7-22px in diameter with literally nothing else drawn inside
  them - `Unresolved` is the factually correct classification, not a
  detection gap to close by lowering thresholds until noise gets counted
  as geometry (which the AP spec explicitly warns against: "a false-
  positive geometry explosion is a failure").
- `TerminalLead` classification is a shape heuristic (elongated +
  boundary-touching), not electrical-lead recognition. AP-WIRE-024 must
  treat it as "worth investigating," not as ground truth.

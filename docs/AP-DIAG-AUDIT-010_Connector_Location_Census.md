# AP-DIAG-AUDIT-010 — Connector Location Census & Representation Gap Audit

## 1. Executive Summary

This audit extends AP-DIAG-AUDIT-009's partial connector sweep (10
instances) into a systematic, region-by-region census covering all 12
areas the governing task named, plus the connecting gaps between them.
It finds **12** distinct instances of the diagram's "notched
pass-through" connector symbol convention — **2 more** than
AP-DIAG-AUDIT-009 reported, both newly found near the **Rectifier** and
**Regulator/Rectifier** enclosures. Only **1 of 12** (`CONN-001`, =
`bfae05a427189376`) has any current model representation, and that
representation is via the ordinary `ComponentBoundary` terminal path,
not any connector-specific model element. The manual "46 connector
terminals" figure is traced to its origin (an externally-supplied
assertion in AP-DIAG-AUDIT-008B's task prompt, never derived from the
raster or model) and is **explicitly invalidated** as an unsupported
baseline: this audit's raster-grounded count is 33 one-side pins across
12 connectors, which does not reconcile with 46 under any tested
convention. No production code was modified. No connector architecture
was designed.

## 2. Starting State

- `git rev-parse HEAD`: `139333da6ed7dbc23b4f621744c497413c18c42a`
  (matches the task's expected `139333d`).
- `git branch --show-current`: `main`; `git status --short`: empty.
- **Baseline correction**: the task prompt's "Known Baseline" section
  listed "Resolved components: 28, Unresolved components: 53, Rejected
  components: 0". Direct verification against a clean rebuild confirms
  the true values are **Resolved=0, Unresolved=28, Rejected=53** —
  matching every prior AP's baseline since AP-DIAG-FIX-008 exactly. This
  is treated as a transcription error in the task prompt, not a
  regression; all other prompt baseline values (tests, warnings, wires,
  nets, ground endpoints, validation errors, `ComponentCandidate` total)
  are confirmed correct against the rebuild. Full baseline re-verified:
  61/61 tests, assertions active, 8 compiler warnings, 34 runtime
  warnings, 37 physical wires, 12 electrical nets, 6/6 ChassisGround
  endpoints, 81/0/28/53 ComponentCandidate/resolved/unresolved/rejected,
  0 validation errors.

## 3. Connector Definition

Per AP-DIAG-AUDIT-009 and re-confirmed directly against the raster in
this audit: the diagram's "notched pass-through" symbol — a small
enclosed rectangle or oval body drawn directly on a wire bundle, with a
step-notch or interlocking staircase split across its middle
(representing the male/female halves of a pluggable connector), with the
same wire-color labels preserved on both sides confirming pass-through
continuity rather than any signal transformation. **Two visual
sub-variants** were found and are inventoried separately rather than
silently merged:

- **Simple single-notch body** (rectangle or oval): `CONN-001, 002, 003,
  004, 005, 006, 009, 010, 011, 012`.
- **Interlocking multi-step staircase-split body** (rectangle only):
  `CONN-007, 008`.

Both variants satisfy the same engineering definition (opposing-side
terminals, conductor entry+exit, inline position, not an outer-edge
termination) and are treated as the same connector *class* for census
purposes, with their variant recorded per-connector.

## 4. Census Methodology

Systematic visual sweep of `samples/trx300ODG.png` in overlapping crops,
covering all 12 named areas (Alarm Unit, CDI Unit, Relay cluster, Pulse
Generator/Reverse Switch area, Tail-light/fuse area, Ignition Switch,
Rectifier, Regulator/Rectifier, Battery/Starter Relay, Headlight harness,
Indicator harness) plus the connecting gaps between them (Ignition-Switch-
to-Rectifier gap, relay-cluster-to-switch-row gap, the full right-column
top-to-tail-light path, and the Ignition-Switch-to-D.C.-Consent gap).
Every wire-mounted small symbol encountered — bullet-splice marks, diode
triangles, ground bars, lamp bases, switch/sensor circles, all
independently characterized in AP-DIAG-AUDIT-007/008/008B/009 — was
checked against the connector definition and excluded when it did not
match. The most common exclusion was the single-wire bullet-splice mark
(diode-triangle + small rectangle on one wire, no shared enclosing
housing), found repeatedly on the headlight and indicator harnesses. The
"OPTIONAL" P/G accessory pigtail was re-confirmed excluded (one open,
unterminated end, per AP-DIAG-AUDIT-009's Connector Boundary Rule
application).

## 5. Diagram-Wide Connector Inventory

| ID | Region | Bounds (x,y,w,h) | Variant | Label | Pins |
|---|---|---|---|---|---:|
| CONN-001 | Tail-light/fuse | 829,436,21,17 | simple notch (rect) | none visible | 2 |
| CONN-002 | Tail-light/fuse | 865,388,20,12 | simple notch (rect) | `(R)` | 2 |
| CONN-003 | CDI Unit | 213,178,45,15 | simple notch (rect) | unresolved | 4 |
| CONN-004 | CDI Unit | 258,178,20,15 | simple notch (rect) | unresolved | 2 |
| CONN-005 | Alarm Unit | 295,175,35,15 | simple notch (rect) | unresolved | 3 |
| CONN-006 | Alarm Unit | 335,175,25,15 | simple notch (rect) | unresolved | 2 |
| CONN-007 | Relay cluster | 410,318,40,42 | interlocking staircase | `(B) (MINI)` | 4 |
| CONN-008 | Relay cluster | 400,412,40,42 | interlocking staircase | unresolved (partial) | 3 |
| CONN-009 | Pulse Gen/Reverse Switch | 625,398,45,35 | simple notch (oval) | `[MINI] (G)` | 3 |
| CONN-010 | Pulse Gen/Reverse Switch | 695,398,45,35 | simple notch (oval) | unresolved | 3 |
| CONN-011 | Rectifier | 595,142,25,13 | simple notch (rect) | unresolved | 2 |
| CONN-012 | Regulator/Rectifier | 695,158,45,20 | simple notch (rect) | unresolved | 3 |

**CONN-011 and CONN-012 are newly discovered in this AP** — not reported
by AP-DIAG-AUDIT-009's partial sweep.

**Areas swept with zero connector instances found**: Ignition Switch
(only plain switch-terminal circles, no notched body), Battery/Starter
Relay (direct wiring, no notched body — ground symbols independently
covered in AP-DIAG-AUDIT-008B), Headlight harness (individual
bullet-splice marks only), Indicator harness (individual bullet-splice
marks only).

## 6. Connector-by-Connector Evidence

Full per-connector evidence (bounding box, pin/entry/exit/through
counts, wire-color relationship, current model representation) is
recorded in `artifacts/trx300/connector_location_census.json`. Two
representative entries:

**CONN-001** — the only connector with model representation. 2 wires
(`Y/W`, `G/W`) enter the top, pass through a rounded/notched body, and 2
wires exit the bottom, colors preserved. `ComponentCandidate`
`bfae05a427189376` exists, `ShapeKind: Circle`,
`ComponentCandidateKind: CircularSymbol`, 2 owned `SymbolPrimitive`s
(kind `Unknown`), 7 `TerminalCandidate`s (all `ComponentBoundary` kind),
7 `EndpointCandidate`s, 2 physical Wires terminate there
(`wire-0a3ab2e5ace89210`, `wire-e5f7ff766561e9b7`), 0 `ConnectorCandidate`,
0 `ConnectorTerminal`, 0 `ConnectorBoundary`, 0 `ElectricalNet`
membership.

**CONN-012** — representative of the 11 unrepresented connectors. 3 of
the Regulator/Rectifier's 6 leads pass through a notched 3-pin body
(bounds ~695,158,45,20); the other 3 leads continue straight down with
no connector visible along their traced path. `NO REPRESENTATION`: no
`ComponentCandidate` exists at this location (confirmed by exhaustive
bounding-box search of all 81 current candidates), hence no `ShapeKind`,
`SymbolPrimitive`, `TerminalCandidate`, `EndpointCandidate`, `Wire`,
`ConnectorCandidate`, `ConnectorTerminal`, `ConnectorBoundary`, or
`ElectricalNet` relationship of any kind.

## 7. Pin/Conductor Accounting

| Quantity | Value |
|---|---:|
| A. Visible connector-side terminal count (sum) | 33 |
| B. Conductor entry count (sum) | 33 |
| C. Conductor exit count (sum) | 33 |
| D. Through-pass-through count (sum) | 33 |
| E. Externally terminated conductor count | 0 |
| F. Visually shared conductor sections | 0 |
| G. Ambiguous/occluded pin count | 0 |

A=B=C=D for all 12 connectors because every one was directly observed to
be a genuine pass-through (equal wires enter and exit, on the opposing
side, colors preserved where legible) — an **observed fact for these
12**, not an assumption applied uniformly across an unverified set.

**These are deliberately distinct engineering concepts, never
conflated in this accounting**:

- **Connector pins** — physical contact positions on the connector body
  (counted above).
- **Wire endpoints** — `EndpointCandidate` positions in the extraction
  model; a different, model-layer concept that may not exist at all for
  a given connector (11 of 12 have none).
- **Conductor entries/exits** — raster-ink observations at a specific
  connector (counted above), independent of whether any model element
  represents them.
- **Physical wires** — reconstructed `Wire` records with independent
  endpoint-to-endpoint identity (AP-WIRE-029); a connector's presence
  does not imply a `Wire` exists on either side of it (10 of 12 have
  none at all; `CONN-001` has 2).

## 8. Pass-Through Analysis

For all 12 connectors: every conductor observed enters the connector
body, exits the connector body on the directly opposing side, and is
never interrupted by labeling (wire-color labels sit beside the wire,
not across it) where legible. No conductor was observed terminating
*at* a connector (i.e., entering but not continuing) — every one found
in this census is a genuine pass-through, consistent with the governing
definition. No connector-side conductor was found to share geometry with
another connector's conductor (F=0 above). No pin's continuity was
established from geometric crossing alone — every determination in this
section came from directly reading the preserved wire-color label on
each side of each connector, or (where a label was not legible in the
crop taken) recording the pin as visually consistent with pass-through
solely from the drawn conductor continuing on both sides.

## 9. Current Model Representation

Per connector, checked directly against the current `WireModel`:

| | CONN-001 | CONN-002 – CONN-012 (11 connectors) |
|---|---|---|
| `ComponentCandidate` | Yes | **NO REPRESENTATION** |
| `ComponentCandidateKind` | `CircularSymbol` | n/a |
| `ShapeKind` | `Circle` | n/a |
| `SymbolPrimitive` | 2 (kind `Unknown`) | n/a |
| `TerminalCandidate` | 7 (`ComponentBoundary`) | n/a |
| `EndpointCandidate` | 7 (`component_terminal`) | n/a |
| `Wire` | 2 | n/a |
| `ConnectorCandidate` | 0 | 0 |
| `ConnectorTerminal` | 0 | 0 |
| `ConnectorBoundary` | 0 | 0 |
| Topology relationship | 2 edges, 2 nodes | n/a |
| Electrical-net relationship | 0 | n/a |

No representation was manufactured for any of the 11 unrepresented
connectors — every "n/a"/"NO REPRESENTATION" above reflects a direct,
exhaustive query against the current model finding nothing.

## 10. Representation Gap Matrix

| Connector | Source Evidence | Candidate | Shape | Primitive | Terminal Candidates | ConnectorBoundary | ConnectorTerminal | Wires | Electrical Net | Status |
|---|---|---|---|---|---|---|---|---|---|---|
| CONN-001 | Yes | Yes | Circle | 2 | 7 (ComponentBoundary) | No | No | 2 | 0 | 3 — component correct-ish, terminal model wrong |
| CONN-002 | Yes | No | — | — | — | No | No | 0 | 0 | 1 — model absent |
| CONN-003 | Yes | No | — | — | — | No | No | 0 | 0 | 1 — model absent |
| CONN-004 | Yes | No | — | — | — | No | No | 0 | 0 | 1 — model absent |
| CONN-005 | Yes | No | — | — | — | No | No | 0 | 0 | 1 — model absent |
| CONN-006 | Yes | No | — | — | — | No | No | 0 | 0 | 1 — model absent |
| CONN-007 | Yes | No | — | — | — | No | No | 0 | 0 | 1 — model absent |
| CONN-008 | Yes | No | — | — | — | No | No | 0 | 0 | 1 — model absent |
| CONN-009 | Yes | No | — | — | — | No | No | 0 | 0 | 1 — model absent |
| CONN-010 | Yes | No | — | — | — | No | No | 0 | 0 | 1 — model absent |
| CONN-011 | Yes | No | — | — | — | No | No | 0 | 0 | 1 — model absent |
| CONN-012 | Yes | No | — | — | — | No | No | 0 | 0 | 1 — model absent |

**11 of 12 fall in gap category 1 (source exists, model absent). 1 of 12
(CONN-001) falls in gap category 3 (source exists, component exists but
terminal model is wrong — `ComponentBoundary` instead of a
connector-specific classification). 0 of 12 fall in categories 2, 4, or
5.**

## 11. ShapeDetector Findings

**CONN-001**: `ShapeDetector`'s circle-detector triggered and produced a
`ShapeKind::Circle` region for this symbol's rounded/notched outline;
`ComponentCandidateClassifier` mapped it unconditionally to
`ComponentCandidateKind::CircularSymbol`. No rejection occurred.

**CONN-002 through CONN-012**: no `ShapeDetector`-produced
`ComponentCandidate` exists for any of these 11 locations. This audit
did **not** instrument or modify `ShapeDetector` to determine the
precise internal reason for each (the current model exposes no
rejection-reason evidence trail for a shape that was never proposed at
all, as distinct from one proposed-then-rejected) — this is explicitly
left open as a question for the next AP's forensic pass, not guessed at
here. What **is** established: `ShapeDetector` has no purpose-built
detector for the notched/interlocking pass-through symbol family — unlike
its purpose-built `ChassisGround` bar-pattern detector — and its
available shape buckets (`Circle`, `Rectangle`-as-`Enclosure`,
`Rectangle`-as-`PrimitiveSymbol`) are not documented anywhere in the
codebase as targeting this connector convention. This is an
**architectural gap** (no detector concept exists for it), independently
re-derived here rather than assumed from AP-DIAG-AUDIT-009.

## 12. ConnectorBoundary Findings

Architecture trace (re-confirmed unchanged, not modified):
`ComponentCandidateKind` → `TerminalCandidateKind` → `EndpointKind` →
`ConnectorCandidate` → `ConnectorTerminal`. `TerminalCandidateKind::ConnectorBoundary`
is assigned only when the owning `ComponentCandidateKind` is
`PrimitiveSymbol`, itself only reachable from a `ShapeKind::Rectangle`
shape that received a non-`Enclosure` `ShapeRole`.

**CONN-001** has `ShapeKind::Circle` — incompatible with this
requirement (confirmed directly, re-derived from AP-DIAG-AUDIT-009).
**CONN-002 through CONN-012** have no `ShapeKind` at all, so the
compatibility question does not yet formally arise for them. However,
their directly-observed shapes (rounded/notched rectangle, oval, or
interlocking-staircase rectangle) are not a plain non-`Enclosure`
`Rectangle` either, so the same incompatibility very likely recurs for
all 12 — stated here as a strong inference from the observed shape
family, **not** independently proven for each of the 11 candidateless
locations (doing so would require them to have a `ShapeKind` to
evaluate, which they do not).

## 13. Wire Continuity Findings

**CONN-001**: 2 topology edges, 2 physical Wires
(`wire-0a3ab2e5ace89210`, `wire-e5f7ff766561e9b7`) terminate at its 2
`EndpointCandidate`s — each Wire ends at its own endpoint there per
AP-WIRE-029's hard-boundary rule; no merge made or implied.

**CONN-002 through CONN-012**: no `ConductorSegment`, topology node, or
Wire references any of these 11 locations. Direct raster inspection
confirms real, continuous, unbroken ink entering and leaving each of
them on both sides — the underlying wire runs continue as ordinary
conductor geometry elsewhere in the model, unbroken by any
connector-boundary evidence, because no candidate exists there to anchor
one.

No finding in this audit relied on a diagonal wire, a direction change
at an intersection, an X-crossing transfer, or treating a crossing as a
splice/endpoint. No physical Wire identity was inferred from electrical
connectivity — every continuity observation came directly from raster
wire-color labels and connector-symbol geometry.

## 14. 28-vs-46 Pin Reconciliation

- **Origin of 46**: traced directly to AP-DIAG-AUDIT-008B's task prompt,
  under "TRX300 MANUAL ENGINEERING GROUND TRUTH" — supplied as an
  **external manual assertion**, never derived from the raster, the
  model, or any prior AP's own computation.
- **Origin of 28**: AP-DIAG-AUDIT-009's sum of one-side pin counts across
  the 10 connector instances its partial sweep found.
- **This AP's finding**: the more complete, 12-connector sweep sums to
  **33** one-side pins — closer to 46 than 28 was, but still
  unreconciled under any tested convention (33 as observed, 66 if both
  sides of every pin are double-counted).
- **Conclusion**: 46 is **not source-grounded** by any evidence produced
  across AP-DIAG-AUDIT-008B, AP-DIAG-AUDIT-009, or this AP. Per this
  AP's explicit instruction, **46 is invalidated as an unsupported
  baseline** for connector-terminal reconciliation — not forced to
  match, not assumed correct. The evidence-grounded figures available
  today are: 33 one-side pins across 12 located connector instances, in
  a sweep assessed as substantially complete but not proven exhaustive
  (Section 15).

## 15. Completeness Assessment

**SUBSTANTIALLY COMPLETE BUT WITH SPECIFIC UNCERTAINTIES.**

All 12 named regions were visually swept, plus the connecting gaps
between them. No further instances were found beyond the 12 listed.
However:

- Several connector labels were only partially visible or unresolved in
  the crops taken and were not re-verified at additional zoom/angle.
- Individual wire colors were read for only 3 of 12 connectors (those
  with the clearest labels); the other 9 are recorded as "unresolved —
  not individually read" rather than guessed.
- The switch-continuity legend table and color-key legend (both
  independently established as `diagram_furniture` in prior audits) were
  **not** re-swept pixel-by-pixel on the assumption that a documentation
  table cannot contain a wire-bundle connector — this assumption itself
  was not independently re-verified in this AP.

This inventory is explicitly **not** claimed exhaustive.

## 16. Findings by Severity

**MEDIUM — F1**: 2 additional connector instances (`CONN-011`,
`CONN-012`) found beyond AP-DIAG-AUDIT-009's reported 10. No production
consequence; relevant to scoping any future connector-representation
design against the full 12, not 10.

**LOW — F2**: the manual "46 connector terminals" figure is not
source-grounded and does not reconcile with either audit's raster-based
count. No production consequence; relevant to not building a future
validation target against an unverified number.

**LOW — F4**: 11 of 12 located connectors have zero model representation;
the 1 exception is represented via the ordinary component path, not a
connector-specific one, and its shape is architecturally incompatible
with the current `ConnectorBoundary` requirement. Confirms and extends
AP-DIAG-AUDIT-009's finding across the complete inventory — not a
critical defect against current production behavior (0
`ConnectorCandidate`s was already the accepted, understood baseline),
but the central evidence gap the next design AP must close.

**INFORMATIONAL — F3**: `CONN-003`/`CONN-004` (CDI Unit) and
`CONN-005`/`CONN-006` (Alarm Unit) each present as adjacent twin
housings; a human reviewer could plausibly count each pair as one
physical connector location rather than two. Affects only reporting/
labeling, not any production behavior.

No CRITICAL or HIGH finding was identified — a source connector having
no model representation was evaluated for downstream consequence (none
currently, since connector recognition was already known and accepted
as absent) rather than assumed critical by default.

## 17. Architectural Implications

This audit intentionally does **not** design the replacement connector
architecture. It establishes, as ground truth for that future design:

- The diagram uses (at least) two visual sub-variants of one connector
  symbol *class*, not one uniform shape — a future `ShapeDetector`
  addition would need to recognize both the simple-notch and
  interlocking-staircase variants, or explain why only one is in scope.
- The one connector that does reach the model today does so via an
  *existing* shape bucket (`Circle`) that is semantically unrelated to
  connector identity — meaning shape-bucket incompatibility, not merely
  absence, is part of the gap the next design must address.
- Pin counts vary per connector (2–4 observed) — any future terminal/pin
  model must support a variable pin count per connector instance, not a
  fixed arity.
- Wire-color preservation across the connector is the strongest
  available non-OCR evidence source for confirming pass-through
  identity, and could inform (without this audit deciding) how a future
  `ConnectorTerminalModelBuilder`-equivalent stage corroborates a
  candidate.

None of `ShapeKind`, `ComponentCandidateKind`, `PrimitiveSymbol`
structure, terminal schema, boundary semantics, pin numbering, or
pass-through Wire semantics were decided here — those remain for the
subsequent design AP.

## 18. Explicit Non-Fixes

No change to: `ShapeDetector`, `SymbolGeometryExtractor`,
`ConnectorBoundary`, `TerminalRecognizer`, `TerminalLocationDetector`,
`ConductorBoundaryResolver`, `PhysicalWireIdentityReconstructor`,
`ElectricalNetResolver`, the Wire model, the Component model, or the
Connector model. No threshold was tuned. No connector candidate was
added to the production model to make the inventory match the raster.
No generated production artifact was altered to imply connectors exist
where the current pipeline does not represent them.

## 19. Recommended Next AP

A **`ShapeDetector` forensic instrumentation AP** (audit-only, no
modification) that traces, for each of the 12 `CONN-*` locations
individually, the exact internal reason no `ComponentCandidate` is
proposed (which candidate detector(s) could apply, whether one triggered
and was rejected vs. never triggered at all, the specific rejection
condition or threshold if any, and whether `SymbolGeometryExtractor` sees
the shape at all) — closing the gap this AP explicitly left open (Section
11) — **before** any connector architecture (new
`ComponentCandidateKind`, `ConnectorBoundary` re-keying, or pin/
pass-through model) is designed.

## Files Changed

`docs/AP-DIAG-AUDIT-010_Connector_Location_Census.md` (new),
`artifacts/trx300/connector_location_census.json` (new). No other file
was touched.

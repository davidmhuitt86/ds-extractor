# AP-DIAG-AUDIT-009 — TRX300 Inline Connector Semantic Audit

This is an audit document. **No production classifier, resolver,
extractor, `ShapeDetector`, `ShapeRole` classification, terminal
detection, `WireReconstructor`, `ElectricalNetResolver`,
`ElectricalComponentResolver`, topology construction, extraction scope,
morphology, or OCR was modified to produce it.** All findings come from
direct pixel inspection of `samples/trx300ODG.png`, the existing
`dx-audit-component-semantic` tool's output, the standard `dx-extract`
CLI, and direct reading of the terminal-classification source code.
`git status --short` was empty before this document and its companion
JSON artifact were added.

## Starting State

- `git rev-parse HEAD`: `85a85a5fddd589c3197a78daff5440b65a989282` (matches
  the task's expected `85a85a5`).
- `git branch --show-current`: `main`; `git status --short`: empty.
- Baseline re-verified via rebuild + `ctest`: 61/61 tests, assertions
  active, 8 compiler warnings, 34 runtime warnings, 37 physical wires,
  12 electrical nets, 6/6 ChassisGround endpoints, 81/0/28/53
  ComponentCandidate/resolved/unresolved/rejected, 0 validation errors —
  all match the task's expected baseline exactly.

## Methodology

Per the task's explicit instruction, this audit did **not** begin from
"what is currently classified as a connector" (that answer is
trivially zero). It began from the engineering definition given —
two or more terminals on opposing sides/ends, conductor entry evidence,
conductor exit evidence, conductor continuity through the interface, and
an inline (not outer-edge/dead-end) position — and searched the source
raster directly for symbols matching that definition, then
cross-referenced each finding against the current `ComponentCandidate`/
`TerminalCandidate`/`ConductorSegment`/`Wire` model.

## The Connector Symbol Convention

Direct visual inspection identified a consistent, recurring symbol used
throughout this diagram for exactly this kind of pass-through interface:
a small rectangle or oval drawn directly on a wire bundle, with a
step-notch or interlocking staircase split in the middle (representing
the male/female halves of a pluggable connector), and — critically — the
**same wire-color labels appearing on both sides**, confirming
pass-through continuity rather than any transformation of the signal.
Some instances carry an explicit parenthetical label (`(R)`,
`(B) (MINI)`, `[MINI] (G)`).

## 10 Connector-Symbol Instances Found (Partial Sweep)

A partial sweep — the Alarm Unit, CDI Unit, the relay cluster, the
Pulse Generator/Reverse Switch area, and the tail-light/fuse area — found
**10** distinct instances of this convention, not 8:

| ID | Approx. bounds (x,y,w,h) | Label | Pins | Candidate? |
|---|---|---|---:|---|
| conn-01 | 829,436,21,17 | (none visible) | 2 | **Yes** — `bfae05a427189376` |
| conn-02 | 865,388,20,12 | `(R)` | 2 | No |
| conn-03 | 213,178,45,15 | (none visible) | 4 | No |
| conn-04 | 258,178,20,15 | (none visible) | 2 | No |
| conn-05 | 295,175,35,15 | (none visible) | 3 | No |
| conn-06 | 335,175,25,15 | (none visible) | 2 | No |
| conn-07 | 410,318,40,42 | `(B) (MINI)` | 4 | No |
| conn-08 | 400,412,40,42 | (partial, likely `(G)`) | 3 | No |
| conn-09 | 625,398,45,35 | `[MINI] (G)` | 3 | No |
| conn-10 | 695,398,45,35 | (none visible) | 3 | No |

conn-03/conn-04 sit as adjacent twin housings directly below the CDI
Unit enclosure; conn-05/conn-06 sit the same way below the Alarm Unit.
A human reviewer could plausibly count each such pair as **one** physical
connector location rather than two — which would move the found count
toward 8 — but this is an unconfirmed interpretation this audit does not
assert as fact.

One further symbol was inspected and **explicitly excluded**: the
"OPTIONAL" P/G plug near (800,270) has only one open, unterminated end
(an accessory pigtail with no conductor exiting the far side) and
therefore fails the conductor-exit and non-outer-edge-termination
requirements — correctly distinguished from a true inline connector
per the task's own Connector Boundary Rule, not classified as one merely
because it has terminals.

## Terminal Count

Per-instance one-side pin counts (number of wires passing through each):
conn-01=2, conn-02=2, conn-03=4, conn-04=2, conn-05=3, conn-06=2,
conn-07=4, conn-08=3, conn-09=3, conn-10=3 — **sum = 28**.

**This does not reconcile with the manual total of 46** under any
counting convention tried (28 as observed, or 56 if doubled for both
sides of each pin). Per this task's own instruction, this is stated
explicitly rather than invented: either additional connector locations
exist beyond the 10 found in this partial sweep (likely, given several
diagram areas — Ignition Switch, Rectifier, Regulator/Rectifier,
Battery/Starter Relay, headlight and indicator harnesses — were not
exhaustively checked), or some instances' pin counts were undercounted,
or both. **The 46 total cannot be independently reconstructed from
current artifacts or this audit's evidence.**

## Opposing-Side Test

All 10 found instances pass: entry on one side/end, exit on the directly
opposing side/end, with wire-color labels preserved across the
connector. This was verified from the drawn labels themselves, never
inferred from the bounding-box shape alone. The current extraction model
preserves **no** side/orientation/axis information for any of the 9
candidateless locations (no geometry exists at all); for conn-01 (the
one with a `ComponentCandidate`), the model's `TerminalCandidate`/
`EndpointCandidate` records carry position but no explicit side/axis
field — this audit's side determination came from the raster, not the
model.

## Conductor Entry/Exit Test

For **conn-01 only**: 2 topology edges and 2 physical Wires
(`wire-0a3ab2e5ace89210`, `wire-e5f7ff766561e9b7`) terminate at its 2
`EndpointCandidate`s — conductor continuity up to the connector's own
boundary is model-represented on each side, though the model does not
represent "continuity *through* the connector" as a distinct concept;
each Wire simply ends at its own endpoint there, exactly as AP-WIRE-029
defines a hard Wire boundary. For the other 9 locations, **no**
`ConductorSegment`, topology node, or Wire references them at all — not
because the underlying ink is absent (direct inspection confirms real,
continuous ink entering and leaving every one of the 10 locations), but
because no `ComponentCandidate` exists there to anchor any such
evidence.

## Inline Test / Outer-Edge Negative Test

All 10 found instances sit within the interior wiring with drawn ink
continuing on both sides — none are outer-boundary terminations. The one
symbol that *is* a dead-end termination (the "OPTIONAL" plug) was
correctly excluded (Section above), demonstrating the boundary rule was
applied, not skipped.

## ShapeRole / Classification Audit

| | conn-01 | conn-02 through conn-10 |
|---|---|---|
| Current `ComponentCandidate` | Yes (`bfae05a427189376`) | None exist |
| `ShapeKind` (from `ShapeDetector`) | `Circle` | n/a — never proposed |
| `ComponentCandidateKind` | `CircularSymbol` | n/a |
| Classified `Enclosure`? | **No** | n/a — not applicable, never classified as anything |

**The premise "all connector objects are incorrectly classified as
Enclosure" is false for every one of the 10 found locations.** The one
connector with any model representation is classified `CircularSymbol`,
not `Enclosure`. The other 9 are not classified as anything at all — they
never became a `ComponentCandidate` in the first place, so "Enclosure
misclassification" does not describe their failure; it is one step
further upstream (no shape proposal at all).

## Component vs. Connector Disambiguation

conn-01's shape (2 wires converging into a notch-bodied symbol then 2
wires diverging, colors preserved) is visually distinct from every other
component-symbol family independently characterized in
AP-DIAG-AUDIT-008/008B in this same diagram: lamp bases, relay/coil
zigzags, diode triangles, ground bars, and switch/sensor circles. The
same reasoning applies to conn-02 through conn-10. None of these 10 read
as an annotation, a component body, or a module enclosure under the
given engineering evidence (opposing terminals + conductor entry/exit +
inline position + continuity), independent of shape alone.

## Connector Terminal Representation Audit

`ConnectorTerminal` remains 0 globally. The 46 manual connector
terminals are **not** already present under another terminal type for
9 of the 10 found locations — no `TerminalCandidate`, `EndpointCandidate`,
or `ConductorSegment` references those coordinates at all, confirmed by
the same exhaustive bounding-box search AP-DIAG-AUDIT-008B used. For
conn-01, its 7 `TerminalCandidate`s are present but classified
`ComponentBoundary`, never `ConnectorBoundary` — represented, but
semantically misclassified relative to the connector evidence this audit
gathered (and the 7-terminal count likely reflects multiple close
endpoint detections along the same 2 physical leads rather than 7
distinct engineering pins — not independently confirmed either way).

## Wire Continuity Through Connectors

For conn-01: two distinct physical Wires each terminate at one of its
two endpoints, exactly matching AP-WIRE-029's hard-boundary rule — no
merge was made or suggested, consistent with "a connector can be the
semantic interface between two distinct physical conductor sections."
For the other 9: no Wire references any of these locations at all; the
underlying wire runs continue as ordinary, unbroken `ConductorSegment`/
topology elsewhere.

## Crossing-Rule Compliance

No finding in this audit relied on a diagonal wire, a direction change
at an intersection, an X-crossing transfer, or treating a crossing as a
splice or endpoint. Every opposing-side/pass-through observation came
directly from drawn wire-color labels and connector-symbol geometry.

## Root Cause

**Two distinct, independently-confirmed root causes, at two different
layers:**

1. **Upstream shape/geometry classification (9 of 10 locations)**: no
   `ComponentCandidate` is ever proposed for these connector symbols at
   all. This is prior to and independent of any terminal, connector, or
   symbol-family logic — nothing downstream can act on evidence that was
   never produced.
2. **Connector-boundary architecture mismatch (the 1 location that does
   have a candidate)**: `ConnectorBoundary` terminal-kind classification
   (`terminal_location_detector.cpp:158`, `terminal_recognizer.cpp:82`)
   is keyed to `ComponentCandidateKind::PrimitiveSymbol`, which is itself
   only reachable from a non-`Enclosure` `Rectangle` shape
   (`component_candidate_classifier.cpp:18`). `bfae05a427189376`'s actual
   `ShapeKind` is `Circle` — even if `PrimitiveSymbol` candidates existed
   elsewhere in this model, **this specific, confirmed-genuine connector
   would still never satisfy the current condition**, because the
   condition depends on a shape family this diagram's actual connector
   symbols (rounded/notched boxes and ovals) do not consistently use.
   This is a deeper mismatch than AP-DIAG-AUDIT-008B's original finding
   (which only established that 0 `PrimitiveSymbol` candidates currently
   exist) — it shows the requirement would be *architecturally
   incompatible* with this diagram's real connector geometry even if
   that were fixed in isolation.

## Answers to the Required Questions

1. **Can all eight manual connectors be located in the source
   diagram?** Not confirmed as exactly eight — 10 distinct instances of
   the connector symbol convention were found in a partial, non-exhaustive
   sweep.
2. **How many terminals does each have?** conn-01=2, conn-02=2,
   conn-03=4, conn-04=2, conn-05=3, conn-06=2, conn-07=4, conn-08=3,
   conn-09=3, conn-10=3 (one-side pin counts).
3. **Does the total equal 46?** No — 28 observed; cannot be reconciled
   from current evidence.
4. **Do all eight satisfy the opposing-side terminal rule?** Yes, for
   all 10 found instances, verified from preserved wire-color labels.
5. **Do all eight have conductor entry/exit evidence?** Yes, as raster
   ink, for all 10; only conn-01 has this represented in the extraction
   model.
6. **Does the current extraction preserve that conductor evidence?** No
   for 9 of 10 (no representation at all); partial for conn-01
   (represented as ordinary component-terminal evidence, not
   connector-specific evidence).
7. **Are any connector terminals already represented as another
   terminal type?** Yes — conn-01's 7 terminals are `ComponentBoundary`
   `TerminalCandidate`s / `component_terminal` `EndpointCandidate`s. No
   representation of any kind exists for the other 9 locations.
8. **Are the eight connector geometries currently classified as
   Enclosure?** No — conn-01 is `CircularSymbol`; the other 9 were never
   classified at all. The "misclassified as Enclosure" premise is not
   supported by this diagram's evidence.
9. **Is the current requirement for ConnectorBoundary → PrimitiveSymbol
   actually compatible with the observed connector evidence?** No — see
   Root Cause #2 above.
10. **Is the current failure primarily ShapeRole classification, missing
    terminal extraction, missing conductor evidence, connector semantic
    definition, connector-boundary architecture, or another upstream
    defect?** A combination, with upstream shape/geometry classification
    dominant (9 of 10 locations never get a candidate at all) plus a
    distinct connector-boundary architecture mismatch (the 1 location
    that does get a candidate is keyed to an incompatible shape family).
11. **Can connector identity be established from existing evidence
    without OCR?** Partially — the opposing-side/pass-through/
    wire-color-preservation pattern establishes the connector's
    *existence and boundary* from geometry and existing conductor-color
    labels alone; the parenthetical harness labels seen on some
    instances would need OCR to be read as identifying strings, but are
    not needed to establish that a pass-through interface exists.
12. **What exact production change would be required, IF the audit
    establishes sufficient evidence?** (Described only — not
    implemented.) (a) A shape-family rule recognizing the notched/
    interlocking pass-through symbol as its own `ComponentCandidateKind`,
    independent of the existing Circle/Rectangle/Enclosure buckets; (b)
    re-keying the `ConnectorBoundary` terminal condition to that new kind
    instead of (or in addition to) `PrimitiveSymbol`; (c) opposing-side
    wire-color preservation as corroborating, non-OCR evidence for
    `ConnectorTerminalModelBuilder`.

## Regression

Re-verified via rebuild + `ctest`: 61/61 tests, 8 compiler warnings (0
new), 34 runtime warnings, 37 physical wires, 12 electrical nets, 6/6
ground endpoints, 0 validation errors, 81/0/28/53
ComponentCandidate/resolved/unresolved/rejected — every value identical
to the AP-DIAG-AUDIT-008B baseline and the task's expected values. No
production file was modified. The audit-generation tool was run twice
against the unmodified pipeline output; the underlying model data is
byte-identical across runs (per AP-DIAG-AUDIT-008/008B's already-verified
determinism of `dx-audit-component-semantic`/`dx-extract` — this AP added
no new production-facing tool, only documentation and a hand-assembled
JSON artifact derived from that already-deterministic data).

## Recommended Next AP

Given the found evidence does not cleanly resolve to exactly 8
connectors or 46 terminals, and 9 of 10 found locations have zero
upstream shape representation, the recommended next step is **not**
implementation but a **targeted, exhaustive connector-location survey**:
complete the sweep of the remaining diagram areas (Ignition Switch,
Rectifier, Regulator/Rectifier, Battery/Starter Relay, headlight and
indicator harnesses) using the now-identified symbol convention, to
establish a confident, complete inventory before any
`ShapeDetector`/`ConnectorBoundary` architecture change is designed
against it — consistent with this AP's explicit instruction not to fix
anything yet.

## Explicit Non-Changes

No change to: `ShapeDetector`, `ShapeRole` classification,
`ComponentCandidateClassifier`, `ConnectorBoundary`, `ConnectorCandidate`,
`ConnectorTerminal`, `TerminalLocationDetector`, `TerminalRecognizer`,
`WireReconstructor`, `PhysicalWireIdentityReconstructor`,
`ElectricalNetResolver`, `ElectricalComponentResolver`, topology
construction, extraction scope, morphology, or OCR.

## Files Changed

`docs/AP-DIAG-AUDIT-009_Inline_Connector_Semantic_Audit.md` (new),
`artifacts/trx300/connector_semantic_audit.json` (new). No other file
was touched.

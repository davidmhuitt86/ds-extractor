# AP-WIRE-030 — Conductor Boundary / Terminal Resolution Specification

## 1. Status

**Specification only. No implementation.** This document defines the
engineering architecture for turning a detected geometric conductor end
into a classified Conductor Boundary, and for associating that boundary
with a specific engineering terminal, using AP-WIRE-029's Conductor
Boundary/Wire vocabulary as its foundation. It does not change
`WireReconstructor`, topology reconstruction, endpoint reconstruction,
terminal recognition, connector recognition, component recognition,
`ElectricalNetResolver`, `WireModel`, `EngineeringDiagram`, SVG
rendering, extraction behavior, tests, CMake, or build configuration.
Implementation against this specification (and AP-WIRE-029) is future
work — AP-WIRE-031 for Wire reconstruction, and whatever AP eventually
implements the boundary/terminal-resolution stage this document defines
— neither of which is started here.

## 2. Purpose

AP-WIRE-029 established *what* a Wire and a Conductor Boundary are,
formally, and *which* engineering object kinds are hard Wire boundaries.
It deliberately left open *how* a future implementation determines,
from existing extraction evidence, that a specific detected conductor
end actually reaches one of those boundary kinds.

This document answers that question:

> **When a conductor line reaches a location in the extracted diagram,
> what evidence allows the system to classify that location as a
> Conductor Boundary, and what evidence allows that boundary to be
> associated with a specific engineering terminal?**

It separates eight distinct operations that must never be collapsed into
one:

1. Detection of a geometric conductor end.
2. Detection of a candidate engineering boundary.
3. Association with a component.
4. Association with a specific terminal.
5. Classification of terminal type.
6. Resolution/conflict handling.
7. Confidence/provenance.
8. Preservation of unresolved evidence.

## 3. Scope

In scope:

- The conceptual lifecycle from geometric observation to boundary/
  terminal resolution (§8).
- The engineering boundary taxonomy and its mapping onto existing model
  vocabulary (§9).
- Evidence rules for component-terminal, connector-terminal, ground-
  terminal, and external-connection resolution (§10–§13).
- The architectural meaning of `TerminalLead` evidence (§14).
- The separation of component association from terminal association
  (§15).
- Rules for spatial association, text/label evidence, wire-color
  evidence, symbol geometry, and symbol-family evidence as boundary/
  terminal evidence sources (§16–§20).
- Evidence provenance, conflict handling, and confidence/status
  semantics requirements (§21–§23).
- A formal boundary-resolution decision matrix (§24).
- Explicit prohibitions on topology mutation, Wire reconstruction, and
  electrical-net-driven inference (§25–§27).
- The current TRX300 baseline and known limitations this specification
  must be read against (§28–§29).
- Worked examples (§30).
- The explicit handoff boundary to AP-WIRE-031 (§31).

Out of scope (this document does not do any of the following):

- Changing `WireReconstructor`, `TopologyReconstructor`,
  `EndpointSemanticReconstructor`, `TerminalRecognizer`,
  `ConnectorTerminalModelBuilder`, `ComponentCandidateClassifier`,
  `ElectricalNetResolver`, `WireModel`, `EngineeringDiagram`,
  `StructuredSvgExporter`, or any other implementation file.
- Adding, removing, or modifying tests, CMake targets, or build
  configuration.
- Running extraction, rendering, or validation.
- Deciding how resolved boundaries are assembled into physical Wire
  paths (§31 — that is AP-WIRE-031's question, not this one).
- Deciding how or whether electrical-net resolution changes (it does
  not, per §27).
- Introducing new model types, enum values, or fields (§6's constraint).

## 4. Non-goals

- This is not a claim that any existing resolver (`TerminalRecognizer`,
  `EndpointSemanticReconstructor`, `ConnectorTerminalModelBuilder`) is
  wrong. Their current behavior is documented as historical
  implementation behavior to be evaluated against this specification
  later, not blessed or condemned here (§16, §28).
- This is not a redefinition of AP-WIRE-028's measurements or
  AP-WIRE-029's architecture. Every AP-WIRE-029 invariant is treated as
  fixed input (§5); every AP-WIRE-028 count is treated as fixed baseline
  (§28).
- This is not a promise that implementing this specification will
  increase terminal or connector coverage on the TRX300 fixture. Coverage
  is an empirical outcome of implementation and measurement, not a claim
  this document is entitled to make (§28, §29).
- This is not a new taxonomy of `EndpointKind`, `TerminalCandidateKind`,
  `TerminalRole`, `ComponentCandidateKind`, or `SymbolPrimitiveKind`.
  Existing enum names are used and mapped as-is throughout (§6, §9); any
  place the existing model cannot represent a distinction this
  specification needs is named as an open question (§33), not silently
  patched by inventing a new field.

## 5. Relationship to AP-WIRE-029

AP-WIRE-029 (`docs/AP-WIRE-029_Conductor_Boundary_and_Wire_Identity.md`,
commit `a4d0c25`) is authoritative input to this document and is not
revised here. In particular, this specification takes as given and does
not re-derive:

- **The formal Wire definition**: *"A Wire is a uniquely identifiable
  physical conductor path extending from one conductor boundary to the
  next conductor boundary."*
- **The Conductor Boundary definition**: *"the point at which the
  physical conductor represented by a Wire terminates,"* with its three
  states — Engineering Boundary, Geometric Boundary, Unresolved Boundary.
- **The four hard Wire boundaries**: component terminal, connector
  terminal, ground terminal, explicit external connection.
- **The critical invariant**: `Splice ≠ Conductor Boundary`,
  `Splice ≠ Wire endpoint` — and, by the same AP-WIRE-029 rules, Junction
  and Crossing are likewise never Conductor Boundaries by themselves
  (AP-WIRE-029 §9–§11).
- **The separation of electrical connectivity from physical Wire
  identity** (AP-WIRE-029 §13, §20 as corrected).

This document does not implement any of those definitions. It specifies
*how existing extraction evidence can establish them* — i.e. it is the
evidence-and-process layer that a future implementation of AP-WIRE-029's
architecture must follow. Nowhere in this document is Splice, Junction,
or Crossing treated as a boundary category (§9), Wire reconstruction
logic proposed (§26), or physical Wire identity inferred from electrical-
net membership (§27) — all three would directly contradict AP-WIRE-029
and are explicitly prohibited.

## 6. Existing model inputs

Reviewed before writing this specification, and used throughout without
modification:

| Existing concept | Where | Relevance here |
|---|---|---|
| `EndpointCandidate` / `EndpointKind` | `core/model.hpp` | The geometric-observation object (§7); `EndpointKind` already distinguishes `GeometricConductorEnd`, `ComponentTerminal`, `ConnectorTerminal`, `Splice`, `Ground`, `ExternalConnection`, `Unresolved` — this is the closest existing analog to §9's boundary taxonomy, and §9 documents the mapping rather than replacing it. |
| `TerminalRole` | `core/model.hpp` | `Unknown` / `ComponentTerminal` / `ConnectorTerminal` / `GroundTerminal` / `PowerSource` / `ExternalConnection` — an orthogonal "what electrical role" classification carried alongside `EndpointKind` on `EndpointCandidate`. |
| `TerminalCandidate` / `TerminalCandidateKind` | `core/model.hpp` | The component-association object (§10); `TerminalCandidateKind` has `ComponentBoundary`, `ConnectorBoundary`, `GroundConnection`, `Unknown` — produced today by `TerminalRecognizer` (AP-WIRE-024). |
| `ComponentCandidate` / `ComponentCandidateKind` | `core/model.hpp` | The component-identity object a terminal may associate with; `Enclosure` / `CircularSymbol` / `ChassisGround` / `PrimitiveSymbol` / `DiagramFurniture` / `Unknown`. |
| `ComponentSymbolGeometry` / `SymbolPrimitive` / `SymbolPrimitiveKind` | `core/model.hpp` (AP-WIRE-023) | Internal component geometry evidence; `SymbolPrimitiveKind::TerminalLead` is the specific evidence §14 addresses. |
| `ConnectorCandidate` / `ConnectorTerminal` / `ConnectorTerminalStatus` | `core/model.hpp` (AP-WIRE-020) | The connector-identity and connector-terminal-identity objects (§11); produced today by `ConnectorTerminalModelBuilder` only from a `TerminalCandidate` of kind `ConnectorBoundary`. |
| `ComponentIdentityResolution` / `ComponentIdentityCanonicalization` | `core/model.hpp` (AP-WIRE-015/018) | Component-identity evidence, distinct from terminal identity (§15). |
| `EndpointSemanticReconstruction` / `EndpointSemanticReconstructionStatus` | `core/model.hpp` (AP-WIRE-019) | The Resolved/Unresolved/Conflicted component-association-per-endpoint object §15 and §22 build on; already carries `evidence_component_ids` for the conflicting-evidence case. |
| `TerminalRecognizer` (AP-WIRE-024) | `src/topology/terminal_recognizer.cpp` | Today's only source of `TerminalCandidate`s; uses component-kind-derived `TerminalCandidateKind`, a directional alignment score, and a distance-based confidence combiner — treated here as historical implementation behavior (§16, §28), not blessed as this specification's final rule. |
| `EndpointSemanticReconstructor` (AP-WIRE-019) | `src/topology/` | Produces the per-endpoint Resolved/Conflicted/Unresolved component association §15 discusses. |
| `ConnectorTerminalModelBuilder` (AP-WIRE-020) | `src/topology/connector_terminal_model.cpp` | Today's only source of `ConnectorCandidate`/`ConnectorTerminal`; materializes a connector only from a `TerminalCandidateKind::ConnectorBoundary` terminal (AP-WIRE-028 §12's traced root cause for the 0-connector baseline). |
| `SymbolFamilyResolution` (AP-WIRE-026A) | `core/model.hpp` | Contextual evidence §20 addresses; never a terminal-identity source by itself. |
| AP-WIRE-028 findings | `docs/AP-WIRE-028_AAR.md`, commit `0dca7a7` | The fixed measurement baseline this specification must acknowledge (§28–§29), not solve. |
| AP-WIRE-029 | `docs/AP-WIRE-029_Conductor_Boundary_and_Wire_Identity.md`, commit `a4d0c25` | Authoritative architectural input (§5). |

No new model type is introduced. Where the existing model cannot
represent a distinction this specification needs, that is recorded as an
open question (§33) or an implementation requirement (§21), never
silently patched here.

## 7. Geometric conductor end

A **geometric conductor end** is the purely geometric fact that a
detected conductor line stops at a location — today, an
`EndpointCandidate` whose position and incident-edge structure come from
topology construction (AP-WIRE-005/013), independent of any semantic
interpretation. This is AP-WIRE-029 §3.B's "geometric boundary" restated
at the evidence level:

> Conductor geometry ending at position X establishes only *"conductor
> geometry ends at X."* It does **not** establish *"X is terminal 3 of
> component Y"* — or any other engineering meaning — without independent
> evidence (§4 of the handoff; AP-WIRE-029 §3, §17).

**Proximity-only promotion is explicitly prohibited**: a geometric
conductor end must never be reclassified as an engineering terminal
merely because it is spatially near a component, connector, or ground
symbol. §16 states this as a binding rule; §24's decision matrix states
the concrete cases where proximity alone yields `GeometricConductorEnd`/
`Unresolved` rather than a terminal.

## 8. Conductor Boundary lifecycle

The conceptual (not implemented) lifecycle is:

```
GEOMETRIC OBSERVATION
        ↓
BOUNDARY CANDIDATE
        ↓
ENGINEERING BOUNDARY RESOLUTION
        ↓
TERMINAL ASSOCIATION
        ↓
RESOLVED / UNRESOLVED / CONFLICTED
```

Each arrow is a distinct decision with its own evidence requirement
(§2's eight operations map onto these stages: 1↔Geometric Observation,
2↔Boundary Candidate, 3–5↔Engineering Boundary Resolution/Terminal
Association, 6↔the final status, 7–8 apply throughout). **No stage is
required to reach the next.** A diagram may legitimately, and
permanently, contain:

- unresolved conductor ends (geometry detected, no engineering meaning
  ever established),
- external boundaries (engineering meaning established, no further
  identity beyond "leaves the represented system," per §13),
- conductor ends in damaged/ambiguous scan regions,
- conductor ends belonging to incompletely-drawn symbols, and
- conductor ends whose terminal identity cannot be determined even
  though a component or connector association was established.

All of these must remain **represented**, not discarded, per AP-WIRE-029
§3.B/C and the project's standing "insufficient evidence → Unresolved"
rule. A future implementation must not treat "stage N wasn't reached" as
equivalent to "this object doesn't exist" — an unresolved conductor end
is a real, retained model object at whatever stage it stopped, not a
dropped one.

## 9. Boundary taxonomy

The future engineering-boundary resolution categories are:

1. `ComponentTerminal`
2. `ConnectorTerminal`
3. `GroundTerminal`
4. `ExternalConnection`
5. `GeometricConductorEnd`
6. `Unresolved`

**Splice, Junction, and Crossing are not, and must never become, boundary
categories** — this list intentionally excludes them, consistent with
AP-WIRE-029 §5, §9–§11. A future implementation that adds a "Splice" (or
"Junction"/"Crossing") value to this taxonomy would directly contradict
AP-WIRE-029's critical invariant.

**Mapping to existing model vocabulary** (documented, not changed):

| This taxonomy | Closest existing model vocabulary | Note |
|---|---|---|
| `ComponentTerminal` | `EndpointKind::ComponentTerminal`, `TerminalRole::ComponentTerminal`, `TerminalCandidateKind::ComponentBoundary` | Three existing enums already carry a version of this category on different objects (`EndpointCandidate`, `EndpointCandidate.terminal_role`, `TerminalCandidate`) — this specification does not unify them, it documents that all three currently point at the same engineering concept. |
| `ConnectorTerminal` | `EndpointKind::ConnectorTerminal`, `TerminalRole::ConnectorTerminal`, `TerminalCandidateKind::ConnectorBoundary`, `ConnectorTerminal` (the struct) | Same pattern; `ConnectorTerminal` the struct additionally carries `ConnectorTerminalStatus` (Resolved/Unresolved/Conflicted), which is the closest existing precedent for §22's status requirement. |
| `GroundTerminal` | `EndpointKind::Ground`, `TerminalRole::GroundTerminal`, `TerminalCandidateKind::GroundConnection` | See §12 for the separate, non-overlapping `ComponentCandidateKind::ChassisGround`/`SymbolFamily::Ground` concept this must not be merged with. |
| `ExternalConnection` | `EndpointKind::ExternalConnection`, `TerminalRole::ExternalConnection` | Exists in the model today; no extraction stage currently populates it on the TRX300 baseline (AP-WIRE-028 §6: 0 instances). |
| `GeometricConductorEnd` | `EndpointKind::GeometricConductorEnd` | The default/unpromoted state (§7). |
| `Unresolved` | `EndpointKind::Unresolved`, or a `Conflicted`/`Unresolved` status on any of the resolver structs above (`EndpointSemanticReconstructionStatus`, `ConnectorTerminalStatus`) | Covers both "no evidence yet" and "contradictory evidence" (§22) — existing structs already separate these via their own status enums rather than collapsing to one `Unresolved` value; this specification preserves that separation rather than flattening it. |

This table is a **documentation of correspondence**, not an instruction
to rename, merge, or refactor any existing enum. A future implementation
of this specification would most likely continue to use the existing
`EndpointKind`/`TerminalRole`/`TerminalCandidateKind` enums exactly as
they are; this taxonomy exists so that this specification's prose has
stable, boundary-scoped vocabulary independent of which specific struct
ends up carrying a given fact.

## 10. Component terminal resolution

A conductor end may be associated with a component terminal only when
supported by defensible evidence. Potential evidence sources (per §7 of
the handoff), none of which is by itself always sufficient:

- an existing `TerminalCandidate` (today, `TerminalRecognizer`'s output),
- `TerminalLead` `SymbolPrimitive` evidence (§14),
- `ComponentSymbolGeometry`,
- endpoint semantic evidence (`EndpointSemanticReconstruction`),
- component identity evidence (`ComponentIdentityResolution`/
  `ComponentIdentityCanonicalization`),
- terminal labels / recognized text (§17),
- explicit source geometry (e.g. a conductor visibly touching a drawn
  terminal pin).

**"Belongs to component" and "belongs to a specific terminal of that
component" are not equivalent decisions** — this is §15's core rule,
restated here in the component-terminal-specific case. A conductor end
may be confidently `ComponentTerminal`-classified (component association
resolved) while the exact pin/terminal identity within that component
remains `Unresolved`. This is a valid, permanent end state, not an
intermediate one awaiting further resolution. **Terminal numbers or
terminal names must never be invented** to fill that gap — if the source
does not independently establish which specific terminal, the terminal
identity stays `Unresolved` regardless of how confident the component
association is.

## 11. Connector terminal resolution

Connector terminals are hard Wire boundaries under AP-WIRE-029 §5.
However, per the handoff, three separate decisions must be distinguished
and never collapsed:

- **Connector**: *"Which connector object?"*
- **Connector terminal**: *"Which specific terminal/pin?"*
- **Wire boundary**: *"Does this physical conductor terminate at that
  connector terminal?"*

**Pin numbers, terminal numbers, mating relationships, and connector-
housing identity must never be invented from geometry alone.** If the
source only establishes "this conductor terminates at this connector"
but not which pin, the correct, permanent representation is:

```
Connector    = Resolved
Terminal/pin = Unresolved
```

**wherever existing model semantics support that separation.** Today,
`ConnectorTerminal` (the struct) already carries an independent
`ConnectorTerminalStatus` alongside a `terminal_name` field that may be
empty — this is the existing mechanism capable of representing
"connector resolved, pin unresolved," and this specification does not
require a new field for it. Where a future implementation finds the
existing structure insufficient to represent connector-resolved/pin-
unresolved cleanly, that is recorded as an implementation requirement,
not solved by inventing a value here (§21).

Recall AP-WIRE-028 §12's traced finding: today, a `ConnectorCandidate` is
only ever materialized from a `TerminalCandidate` of kind
`ConnectorBoundary`, which is itself only ever assigned to a component of
`ComponentCandidateKind::PrimitiveSymbol` — a kind that has zero
instances in the current TRX300 baseline. This specification does not
change that chain; it specifies the rules any future connector-boundary-
detection stage feeding that chain must obey.

## 12. Ground terminal resolution

Ground requires special handling because the existing system already has
**two non-overlapping concepts** that must not be merged:

- **`EndpointKind::Ground`** (a wire-terminus ground evidence kind on
  `EndpointCandidate`) — this can be a Wire boundary (`GroundTerminal`,
  §9).
- **`ComponentCandidateKind::ChassisGround`** / `SymbolFamily::Ground`
  (AP-WIRE-023's purpose-built ground-bar shape detector and
  AP-WIRE-026A's resulting symbol-family resolution) — this is an
  engineering **component/symbol**, not a Wire endpoint by itself.

A chassis-ground *symbol* is a drawn component (the ground-bar/triangle
glyph); a ground *endpoint* is where a conductor terminates at a ground
connection. These frequently co-occur in a real diagram (a wire runs to a
ground symbol) but are **not the same fact**: not every ground symbol
necessarily has an associated wire-terminus endpoint recorded as
`EndpointKind::Ground` in the current model, and not every
`EndpointKind::Ground` endpoint necessarily sits at a drawn chassis-
ground symbol (it could equally be a ground pin on an ordinary
component, per AP-WIRE-028 §11's TRX300 spot-checks). This specification
does not assert either direction as a universal rule — it requires that
a future implementation keep the two facts represented independently, per
the existing AP-WIRE-026A distinction, and associate them only where
specific evidence (e.g. spatial coincidence plus corroborating geometry)
actually supports the association, following the same "no proximity-only
promotion" rule as every other boundary kind (§16).

## 13. External connection resolution

An external connection is a hard Wire boundary when explicitly
established. Evidence may include:

- an explicit external label ("OPTIONAL," an accessory name, etc.),
- a connector/interface drawn as leaving the represented system,
- a source-defined harness boundary, or
- an explicit diagram convention the source itself uses.

**The ultimate external destination must never be inferred.** Valid:
*"External connection — destination not represented."* **Invalid**:
*"External connection — probably [some specific component]."* The
latter requires independent evidence establishing that specific
destination; absent that evidence, only the boundary classification
itself (external, unknown destination) is defensible. This restates
AP-WIRE-029 §8 at the evidence level.

## 14. TerminalLead evidence

AP-WIRE-023 established `SymbolPrimitiveKind::TerminalLead` as purely
geometric evidence: *"a line-like blob that touches the component's own
boundary margin"* — explicitly documented in the model itself as "not an
`EndpointCandidate`" and "must not be treated as one." AP-WIRE-024
subsequently used this geometry (via `TerminalRecognizer`'s primitive-
count/alignment logic) as one input toward `TerminalCandidate`
production.

This specification states the architectural meaning explicitly:

> **`TerminalLead` evidence may support a terminal association. It must
> not, by itself, automatically invent: a terminal number, a terminal
> name, an electrical function, a connector pin identity, or a component
> identity — unless that information is independently established** by
> some other evidence source (§10's list, or §17's label evidence).

A `TerminalLead` is **stronger** evidence than a bare component-boundary
shape (§19) precisely because it explicitly represents a drawn lead —
but "stronger" means "better evidence that a terminal-shaped feature
exists here," not "sufficient to assert what that terminal *is*." The
distinction is the same one §10 draws for component-vs-terminal
association: `TerminalLead` geometry is good evidence for "a terminal-
like boundary candidate exists at this location," weak-to-absent evidence
for "this is specifically pin 3" or "this is the ground pin."

## 15. Component-vs-terminal association

These are **separate decisions**, each with its own status, and must
never be collapsed into one combined result:

```
Endpoint E
   │
   ├── Component association = Resolved
   │
   └── Terminal identity = Unresolved
```

This is a **valid, stable state** — not an error, not an intermediate
step awaiting completion. Likewise:

```
Endpoint E
   │
   ├── Component association = Conflicted
   │
   └── Terminal identity = must remain Conflicted (or Unresolved)
```

**Terminal identity must never be forced to a resolved value merely
because component identity is known** (or vice versa — a resolved
terminal identity is never grounds to silently upgrade an unresolved or
conflicted component association). Each decision is evaluated on its own
evidence, per its own row in §24's decision matrix. `EndpointSemantic-
Reconstruction` already models the component-level decision
independently (with its own `EndpointSemanticReconstructionStatus` and
`evidence_component_ids` for the conflicting case) separately from
`TerminalCandidate`'s own confidence — this specification's requirement
is that any future implementation preserve that independence rather than
deriving one status from the other.

## 16. Spatial association

Spatial proximity may **associate evidence** (i.e. it is legitimate input
to a resolution decision) but **must never independently establish
engineering identity**. Four distinct relationships must be kept
explicitly separate and never conflated:

- **NEAR** — spatial proximity alone; the weakest relationship, evidence
  of nothing beyond "these two things are close on the page."
- **CONNECTED TO** — a drawn conductor geometrically links two things;
  stronger than NEAR, but still not, by itself, a specific terminal
  identity.
- **TERMINATES AT** — a conductor's geometric end coincides with a
  specific boundary location; this is what promotes a geometric
  observation to a boundary *candidate* (§8), not yet a resolved
  engineering terminal.
- **IDENTIFIED AS TERMINAL** — the boundary has been associated with a
  specific engineering terminal identity through defensible evidence
  (§10, §14, §17); this is the only relationship that licenses a
  `Resolved` terminal-identity status.

**"Nearest component wins" is explicitly prohibited as a universal
rule.** Existing `TerminalRecognizer` behavior — distance thresholds, an
alignment-cosine score against candidate components, and a
distance-derived confidence combiner (`combine_confidence()` in
`src/topology/terminal_recognizer.cpp`) — is **historical implementation
behavior**, documented here as an existing input to be evaluated against
this specification later, **not automatically blessed as final
AP-WIRE-030 architecture**. A future implementation may retain, refine,
or replace that specific distance/alignment scheme; this document takes
no position on which, beyond requiring that whatever scheme is used obey
§16's four-relationship distinction and §22's conflict rules.

## 17. Text/label evidence

Recognized text may provide terminal evidence — e.g. `"1"`, `"2"`,
`"B+"`, `"IGN"`, `"GND"`, `"A"`, `"PIN 3"` — but **only once it has been
spatially and semantically associated** with a specific conductor
boundary; text sitting elsewhere on the page is not terminal evidence for
an unrelated boundary merely because both exist. **Unknown text remains
unknown** — this specification does not implement OCR, and does not
require or assume any particular OCR/recognition provider's output; it
only states that *if* recognized text is available and properly
associated, it is valid terminal evidence. A recognized label must never
automatically establish component identity, connector identity, or
terminal identity **by itself** — the relationship (which object the
label actually describes) must be independently evidenced (e.g. via
existing `SemanticAssociation`/`TextSemanticEvidence`-style spatial-
association evidence), not assumed from mere co-presence on the page.

## 18. Wire-color evidence

Wire-color evidence (AP-WIRE-025's domain) may help **identify a
conductor** — it must never independently establish a terminal boundary.
Same color on two nearby conductors does **not** mean "same terminal," 
"same Wire," or "same component." Wire color is conductor-identity/
labeling evidence, never terminal-identity evidence, and never overrides
contradictory conductor geometry (this restates AP-WIRE-029 §17's table
row for wire color, applied specifically to boundary/terminal
resolution). The distinction AP-WIRE-025 already establishes between
wire-color resolution and component/terminal association is preserved
unchanged here — this document does not touch `WireSemanticResolution`.

## 19. Symbol geometry evidence

AP-WIRE-023 geometry (`ComponentSymbolGeometry`, `SymbolPrimitive`) may
**support** boundary resolution but must be distinguished from terminal
evidence specifically:

- **Symbol shape evidence** (an `Enclosure`'s outline, a `CircularSymbol`,
  a bare `PrimitiveSymbol`) establishes *what is drawn*, not *where its
  terminals are*. A component enclosure does not automatically identify a
  terminal. A circle does not automatically identify a terminal. A bare
  `PrimitiveSymbol` does not automatically identify a terminal.
- **Terminal evidence** specifically is `SymbolPrimitiveKind::TerminalLead`
  (§14) — geometrically stronger because it explicitly represents a
  drawn lead reaching the symbol's boundary — but even `TerminalLead`
  evidence does not invent terminal semantics (name, number, function) by
  itself, per §14.

This is the same shape-vs-terminal distinction AP-WIRE-026A already
applies to symbol-*family* recognition (geometry alone is never
sufficient for a resolved family; a label keyword plus compatible
geometry is required) — §19 states the analogous rule for terminal
resolution: geometry alone is never sufficient for a resolved terminal
identity either.

## 20. Symbol-family contextual evidence

AP-WIRE-026A `SymbolFamilyResolution` may provide **contextual** evidence
— e.g. a resolved `Battery` symbol family may make a nearby terminal
interpretation more meaningful (a terminal near a resolved Battery symbol
is more plausibly a battery terminal than an arbitrary unlabeled pin
would otherwise be). But symbol family must **never automatically
invent** pin numbering, terminal identity, or wire association merely
from the family being known. AP-WIRE-026A's existing never-guess boundary
(geometry alone is never sufficient; label keyword + compatible geometry
required for a resolved family) **remains authoritative and unmodified**
— this section only states that *once* a symbol family is legitimately
resolved, it is available as one more piece of context for a boundary/
terminal decision, subject to every other rule in this document (it is
still never sufficient by itself, per §16's and §19's "not automatic"
rules).

## 21. Evidence provenance

Every future resolved boundary/terminal association must be
**traceable** to the evidence that established it. At minimum, a future
implementation's provenance record should conceptually retain:

- the source endpoint/geometric observation (an `EndpointCandidate` id),
- the evidence type (`TerminalLead`, recognized text, existing
  `TerminalCandidate`, component identity, etc.),
- the evidence source object id,
- the component id, if applicable,
- the connector id, if applicable,
- the terminal identifier, if known,
- raw text, if applicable,
- normalized text, if applicable,
- confidence/status, and
- conflict evidence, where applicable (the competing candidates, as
  `EndpointSemanticReconstruction.evidence_component_ids` already does
  for the component-association case).

**No field is added to any model type in this AP.** This section
documents what a future implementation of boundary/terminal resolution
will need to represent; it is a requirement for that later work, not a
change made here. Where existing structures already carry an equivalent
field (e.g. `EndpointSemanticReconstruction.evidence_component_ids`,
`Provenance.stage`), that precedent is the model to extend when
implementation happens, not something this specification needs to
restate as new.

## 22. Conflict handling

The project's existing `Resolved`/`Unresolved`/`Conflicted` semantics are
preserved without modification. Examples, restated in boundary/terminal
terms:

- **Endpoint with two competing component candidates** (Component A
  evidence and Component B evidence, both plausible): **Component
  association = `Conflicted`.** Do not select the closer component. This
  is exactly the AP-WIRE-024 conflict pattern already present in the
  TRX300 baseline (§28) and must not be resolved differently by a future
  implementation of this specification.
- **Endpoint with two competing terminal candidates** (Terminal 1
  evidence and Terminal 2 evidence within the same component): **Terminal
  identity = `Conflicted`.** Do not select the lower-distance candidate.
- **Endpoint with ground evidence and component-terminal evidence both
  present and contradictory**: preserve the conflict — do not silently
  prefer one interpretation — **unless** explicit evidence establishes
  that the two are actually compatible (e.g. the component-terminal *is*
  a documented ground pin on that component, which is itself a resolved
  fact requiring its own evidence, not an assumption).

No stage in this specification is permitted to pick a "most likely"
winner among plausible-but-unproven alternatives; `Conflicted` (or
`Unresolved`, if evidence is merely absent rather than contradictory) is
always the correct terminal state for such cases, per AP-WIRE-029 §18's
never-guess rule, which this section applies specifically to boundary/
terminal resolution.

## 23. Confidence/status semantics

**Geometric detection, boundary resolution, and terminal-identity
resolution may each carry a different status**, and must never be forced
into a single combined value. Per §13 of the handoff:

```
Geometry:          Resolved
Boundary:          Resolved as ComponentTerminal
Terminal identity: Unresolved
```

or:

```
Geometry:          Resolved
Boundary:          Engineering boundary
Terminal identity: Conflicted
```

Both are valid, and a future implementation must be able to represent
either without forcing all three facts into one status field.

**No numeric scoring system is introduced.** The project's existing
architecture uses categorical `Resolved`/`Unresolved`/`Conflicted`
status, generally alongside a separate categorical `ConfidenceClass`
(`High`/`Medium`/`Low`/`Unresolved`) — never a single blended numeric
score — and this specification requires that any future boundary/
terminal resolution follow the same pattern. Where an existing structure
already represents confidence numerically internally (e.g. `Terminal-
Candidate.distance_to_component` as a raw geometric measurement, not a
confidence score), this document does not reinterpret that field's
meaning or introduce a threshold for it; any such interpretation is
implementation work for later, to be documented at that time, not
asserted here.

## 24. Decision matrix

| Case | Result |
|---|---|
| Geometric end + explicit component terminal evidence | `ComponentTerminal` |
| Geometric end + `TerminalLead` + one compatible component | Candidate/Resolved, according to evidence sufficiency (§10, §14) — not automatic from `TerminalLead` presence alone |
| Geometric end + nearby component only (proximity, no other evidence) | `GeometricConductorEnd` / engineering association `Unresolved` (§16) |
| Geometric end + two equally plausible components | `Conflicted` (§22) |
| Geometric end + explicit connector terminal evidence | `ConnectorTerminal` |
| Geometric end + connector identity established but unknown pin | Connector boundary resolved; terminal/pin identity `Unresolved` (§11) |
| Geometric end + explicit ground evidence | `GroundTerminal` |
| Geometric end + external-boundary evidence | `ExternalConnection` |
| Geometric end + conflicting ground/component evidence | `Conflicted` (§22) |
| Line reaches a `Splice` | **Not** a Conductor Boundary merely because it reaches a Splice (AP-WIRE-029 §9) |
| Line reaches a `Crossing` | **Not** a Conductor Boundary (AP-WIRE-029 §11) |
| Line passes through a `Continuation` | No boundary — simply traversed (AP-WIRE-029 §12) |

All terminology in this table is intentionally identical to AP-WIRE-029's
own vocabulary; no case here introduces a result category AP-WIRE-029
does not already define or explicitly exclude.

## 25. Topology mutation prohibition

A future boundary/terminal resolution stage may **annotate/associate**
existing topology objects (attach a resolved boundary or terminal
identity to an existing `EndpointCandidate`, for example). It must
**never silently**:

- create topology edges,
- delete topology edges,
- convert a `Crossing` node to a `Splice` node or vice versa,
- create components,
- create terminals from nothing (i.e. fabricate a `TerminalCandidate`
  with no corresponding evidence),
- merge endpoints, or
- split wires.

Those operations, if ever legitimate, belong to other architectural
stages (topology reconstruction, splice/crossing classification,
component detection) that this specification does not touch and does not
authorize this stage to reach into. AP-WIRE-030's implementation scope is
boundary/terminal resolution only.

## 26. Wire reconstruction prohibition

This specification must not, and does not, decide *"this endpoint
connects to that endpoint, therefore this is one Wire."* That is
AP-WIRE-031's question (§31). AP-WIRE-030 answers exactly two questions
per conductor end:

1. *"What is this conductor boundary?"*
2. *"What engineering terminal, if any, does it represent?"*

AP-WIRE-031 will later answer: *"Which resolved boundaries are connected
by the same physical Wire path?"* These responsibilities are strictly
separated; nothing in §7–§24 proposes, implies, or requires a specific
branch-pairing or path-reconstruction algorithm, including at
distribution nodes (splices/junctions/crossings) — that remains entirely
open for AP-WIRE-031.

## 27. Electrical-net independence

Boundary/terminal resolution must **not** use electrical-net membership
to invent a Conductor Boundary. This restates AP-WIRE-029 §13/§20 (as
corrected) at the boundary-resolution level:

**Valid**: electrical-net resolution says A and B are electrically
connected; boundary resolution (independently) says A is
`ComponentTerminal` #1.

**Invalid**: electrical-net resolution says A and B are connected;
*therefore* A is asserted to be the same physical-Wire boundary as B.
That inference would violate AP-WIRE-029's separation of electrical
connectivity from physical Wire identity, and this specification
prohibits it explicitly at the boundary-resolution layer as well as the
Wire-identity layer AP-WIRE-029 already covers.

Electrical-net resolution (AP-WIRE-022) remains an independent
architecture branch, unmodified, per AP-WIRE-029 §20.

## 28. TRX300 baseline

This specification is written against, and does not alter, the
authoritative AP-WIRE-028 measurement baseline (commit `0dca7a7`):

```
210 endpoint candidates
  174 geometric
   29 component-terminal
    7 ground
    0 connector-terminal
    0 external-connection
    0 splice
    0 unresolved
 59 real components
 56 terminal candidates, referenced by 20/59 real components
  0 connectors
  0 connector terminals
```

AP-WIRE-024 added 8 `TerminalCandidate`s beyond the pre-existing
baseline and produced 4 explicit conflicts from competing claims (the
endpoints `endpoint-candidate-cde07f8718a2b9cd`, `...b0e3d6bb622a227c`,
`...c033140e251b7b86`, and `...1fd584e37a5c72b5` — re-verified through
AP-WIRE-028). **This specification requires that a future implementation
preserve these existing conflicts rather than selecting winners** — none
of §10–§24's rules license resolving them, and §22 explicitly forbids
picking a closer/more-convenient candidate to make them disappear.

**This specification does not claim that implementing it will increase
terminal or connector coverage.** Any coverage change is an empirical
outcome of a future implementation and measurement pass, not a promise
made here.

## 29. Known limitations

The following AP-WIRE-028 findings are explicitly acknowledged as
architectural inputs this specification is not attempting to solve:

1. Connector recognition is currently zero in the TRX300 deterministic
   baseline (AP-WIRE-028 §12).
2. Many real components (39/59) have no terminal evidence at all
   (AP-WIRE-028 §11).
3. Existing terminal evidence (56 `TerminalCandidate`s) is incomplete
   relative to the source.
4. Furniture contamination exists upstream, in topology-node
   classification (AP-WIRE-028 §9b) — outside this specification's scope.
5. Recognition-assisted text evidence (OCR/vision) can change terminal
   evidence in future runs; this baseline was measured with zero such
   evidence (AP-WIRE-028 §14).
6. The current geometric endpoint count (210) does not equal any true
   engineering-terminal count — no such count has been reliably
   established (AP-WIRE-028 §8, §21).
7. The current endpoint model contains no connector-terminal population
   in the TRX300 baseline (0/0).

These are **inputs to this specification**, not problems §7–§27 are
required, or attempt, to solve implicitly. A future implementation should
expect these numbers to move once it exists and is measured — this
document does not predict by how much.

## 30. Worked examples

**A. Component terminal**

```
Component A
    terminal
       │
       │
      wire
       │
       │
    terminal
Component B
```

Result: two engineering boundaries (`ComponentTerminal` at each end), one
future Wire (once AP-WIRE-031 exists to assemble it — not decided here).

**B. Connector**

```
Wire A
   │
Connector Terminal 1
   │
Connector
   │
Connector Terminal 2
   │
Wire B
```

Result: two physical Wires (per AP-WIRE-029 §5), potentially one
electrical net (per AP-WIRE-029 §13/§27) — the connector terminals are
each independently resolved `ConnectorTerminal` boundaries; the
connector interface itself is never part of either Wire's path.

**C. Ground**

```
Component terminal
       │
      Wire
       │
Ground terminal
```

Result: `GroundTerminal` is one hard boundary (§9, §12), distinct from
any chassis-ground *symbol* that may or may not be drawn nearby.

**D. Splice**

```
        B
        │
A ──────●────── C
```

Result: the Splice itself is **not** a boundary (§9, AP-WIRE-029 §9).
Physical Wire identity through the splice remains a separate question,
left to AP-WIRE-031 (§26, §31).

**E. Crossing**

```
A ─────────────── B
        ╳
C ─────────────── D
```

Result: no boundary and no connectivity created by the crossing (§9,
AP-WIRE-029 §11).

**F. Proximity trap**

```
Component A       Component B
    ○                  ○
     \                /
      \              /
       \            /
        endpoint
```

Result: nearest component does not automatically win (§16). Absent
further evidence, the engineering association is `Unresolved`, not a
guess at whichever component happens to be closer.

**G. Conflicting evidence**

```
Endpoint E
  ├── Component A / Terminal 1
  └── Component B / Terminal 2
```

Result: `Conflicted`. No winner (§22).

**H. Connector identity without pin identity**

```
Endpoint E
   │
Connector J
   │
unknown pin
```

Result: connector association may be `Resolved` (Endpoint E terminates at
Connector J) while terminal/pin identity remains `Unresolved`, provided
existing model semantics permit that separation (§11) — which, per §11,
`ConnectorTerminal`'s existing independent `ConnectorTerminalStatus` and
optional `terminal_name` already do.

## 31. AP-WIRE-031 boundary

**AP-WIRE-030 establishes:**

- WHERE conductor boundaries are conceptually resolved (§8).
- WHAT evidence establishes them (§10–§20).
- WHAT constitutes a terminal association (§15, §22).
- WHAT remains unresolved, and that it must stay represented (§8, §22,
  §23).
- HOW conflicts are preserved (§22).

**AP-WIRE-031 will establish:**

- HOW resolved boundaries and conductor topology are assembled into
  physical Wire identities — i.e. the "semantic decomposition stage" at
  distribution nodes that AP-WIRE-028 found missing from the current
  `WireReconstructor`, and that AP-WIRE-029 §19 named without designing.

**AP-WIRE-030 does not prescribe the branch-pairing algorithm for
distribution nodes.** Nothing in §7–§30 proposes how a future
implementation should decide, at a splice with three incident conductor
runs, which pairs of resolved boundaries belong to the same Wire — that
question, and its answer, belong entirely to AP-WIRE-031.

## 32. Decision record

- The eight-operation separation in §2 was adopted verbatim from the
  handoff and used as the organizing structure for §7–§23, rather than
  restated as a separate summary, so that each operation maps
  traceably onto exactly the section(s) that answer it.
- The boundary taxonomy (§9) was documented as a **correspondence table**
  against existing enums rather than as a new standalone enum, because
  the handoff explicitly required "if existing enum names differ,
  document the mapping rather than changing the enums," and three
  existing enums (`EndpointKind`, `TerminalRole`, `TerminalCandidateKind`)
  already independently encode a version of this taxonomy on different
  objects — inventing a fourth, unified enum here would itself be an
  unauthorized model change.
- `TerminalRecognizer`'s existing distance/alignment logic (§16) was
  deliberately described as "historical implementation behavior" rather
  than either endorsed or criticized, because the handoff explicitly
  required "do not silently bless existing thresholds as engineering
  truth" — this specification takes no position on whether that specific
  scheme should be kept, tuned, or replaced.
- Ground (§12) was kept as two independent, non-merged concepts
  (`EndpointKind::Ground` and `ComponentCandidateKind::ChassisGround`/
  `SymbolFamily::Ground`) exactly as AP-WIRE-026A left them, because
  AP-WIRE-028's own spot-checks (§11 of that AAR) found no evidence that
  every ground symbol has a corresponding ground endpoint or vice versa —
  asserting a 1:1 relationship here would be an unsupported claim this
  specification is not entitled to make.
- Provenance requirements (§21) were written as a list of fields a future
  implementation should retain, not as new struct definitions, per the
  handoff's explicit "do not add these fields to the model yet."

## 33. Open questions

- **OPEN — Ground endpoint/symbol association evidence.** §12 requires
  that `EndpointKind::Ground` and `ComponentCandidateKind::ChassisGround`
  be associated "only where specific evidence... actually supports the
  association," but this document does not define what that evidence
  concretely consists of (spatial coincidence within what tolerance;
  whether a `TerminalLead` from the chassis-ground symbol reaching the
  endpoint would suffice; whether text evidence is required). This is
  left for whichever future AP actually implements ground-boundary
  resolution to define and justify.
- **OPEN — Whether `ConnectorTerminal`'s existing fields are sufficient
  for §11's "connector resolved, pin unresolved" state in every case.**
  §11 states the existing struct "already carries an independent
  `ConnectorTerminalStatus` alongside a `terminal_name` field that may be
  empty" as the existing mechanism, but whether an empty `terminal_name`
  string is an adequate, unambiguous representation of "genuinely
  unresolved" versus "resolved to the empty string" (a distinct concern
  from AP-WIRE-029's Open Question about Junction, but a similar shape)
  is not resolved here — a future implementation may find it needs a more
  explicit unresolved/resolved marker than a nullable-by-convention
  string.
- **OPEN — Provenance representation mechanism.** §21 lists what
  provenance a future implementation should retain but does not specify
  *how* — as a new field per resolved-object struct (following
  `EndpointSemanticReconstruction.evidence_component_ids`'s precedent),
  as a separate provenance-record type, or via the existing `Provenance`
  struct extended with new fields. This is deliberately left to whichever
  AP performs that implementation, per the handoff's "do not add these
  fields to the model yet."

No other item raised by the handoff's 34 sections was left unresolved —
each has an explicit rule in §7–§31, consistent with AP-WIRE-029, and
traceable to existing model vocabulary per §6.

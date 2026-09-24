# AP-WIRE-029 — Conductor Boundary & Wire Identity Specification

## Status

**Specification only. No implementation.** This document establishes the
formal engineering definition of Wire, Conductor Boundary, Terminal,
Connector, Splice, Junction, Crossing, and Continuation, and the
separation between physical Wire identity and electrical-net
connectivity. It does not change any extraction, topology, terminal,
component, connector, wire-reconstruction, electrical-net, rendering, or
model code. Implementation against this specification is future work
(AP-WIRE-030+), not part of this AP.

## Purpose

AP-WIRE-028 measured that the current `WireReconstructor` assembles only
50/877 (5.7%) of topology edges into `Wire` objects, and traced this to
an algorithmic scope boundary: the reconstruction walk follows degree-2
`Continuation` nodes and stops at any node of different degree (a
`Splice`, `Junction`, or `Crossing`), because "a separate semantic
decomposition stage" — not yet built — is required to go further without
guessing.

Before that stage is designed or built, the project needs an agreed,
durable, written answer to the question the current implementation has
been silently answering by omission: **what is a Wire, exactly, and
where does one begin and end?** This document is that answer. It is
written to be implementable later without re-litigating the architecture
at that time, and to be checked against later without ambiguity about
what was actually decided here versus merely implied.

## Scope

In scope:

- The formal definition of Wire, Conductor Boundary, and each boundary
  kind (component terminal, connector terminal, ground terminal,
  external connection).
- The behavior of Splice, Junction, Crossing, and Continuation with
  respect to Wire identity.
- The relationship between physical Wire identity and electrical-net
  connectivity.
- A decision-scoped evidence priority table for future Wire-
  reconstruction decisions (there is no single universal evidence
  ranking that applies identically to every decision).
- Never-guess invariants for Wire reconstruction.
- Worked examples covering the ambiguous cases (splice, crossing,
  connector, shared conductor).
- Explicit answers to the 21 questions in Part 21 of the handoff.

Out of scope (this document does not do any of the following):

- Changing `WireReconstructor`, `TopologyReconstructor`,
  `EndpointSemanticReconstructor`, `TerminalRecognizer`,
  `ConnectorTerminalModelBuilder`, `ElectricalNetResolver`,
  `StructuredSvgExporter`, `EngineeringDiagramBuilder`, or any other
  implementation file.
- Changing `WireModel`/`EngineeringDiagram` C++ types.
- Adding, removing, or modifying tests, CMake targets, or build
  configuration.
- Running extraction, rendering, or validation.
- Designing the "conductor boundary / terminal resolution" or "physical
  wire identity reconstruction" stages shown in Part 20's conceptual
  pipeline — this document defines what those stages must respect, not
  how they are built.
- Deciding whether/when AP-WIRE-030 begins.

## Non-goals

- This is not a redefinition that invalidates AP-WIRE-028's measurements.
  40/877-edge-derived `Wire` objects, 106 splices, 237 crossings, and
  every other AP-WIRE-028 count remain exactly as measured; this document
  explains *why* the current algorithm stops where it does relative to
  the newly-formal definition, it does not recompute anything.
- This is not a claim that the current implementation is wrong. §19
  states explicitly that the current scope boundary is a legitimate,
  never-guess-compliant implementation choice given that the
  decomposition stage does not exist yet — not a defect to be blamed.
- This is not a new taxonomy of `ComponentCandidateKind`,
  `TerminalCandidateKind`, or `TopologyNodeType`. Existing enum names are
  used as-is throughout; no new model vocabulary is introduced.

## Relationship to the existing "endpoint-to-endpoint" rule

Every prior AP (013 onward) has stated the project's standing wire rule
informally as "Wire = endpoint-to-endpoint engineering object" and "a
splice is never a wire endpoint." That rule is **not contradicted or
discarded here — it is refined and made formal.** "Endpoint" in that
informal phrasing was always doing double duty for two things this
document now names separately: the *geometric* fact of where a detected
conductor line stops (`EndpointCandidate`, §3.B "geometric boundary"),
and the *engineering* fact of what that stopping point means
(§3.A "engineering boundary"). "Conductor Boundary" (§3) is the
formalization of what "endpoint" meant in the informal rule; "hard Wire
boundary" (§4) is the formalization of which specific engineering
boundary kinds actually terminate a Wire. The informal rule's other half
— "a splice is never a wire endpoint" — is preserved verbatim as §9's
opening sentence. Nothing in §1–§21 permits a splice, junction, or
crossing to become a Wire boundary; the refinement only adds precision
about *which* engineering objects do terminate a Wire (§4–§8) and how
much evidence is required before a bare geometric conductor end may be
treated as one (§3, §17).

## 1. Formal Wire definition

> **A Wire is a uniquely identifiable physical conductor path extending
> from one conductor boundary to the next conductor boundary.**

A Wire is therefore fully defined by:

1. Physical conductor geometry (the drawn line(s) it corresponds to).
2. A start conductor boundary.
3. An end conductor boundary.
4. The topology path connecting those two boundaries.
5. Supporting evidence/provenance for both the geometry and the
   boundaries.
6. A resolution/confidence state (`Resolved` / `Unresolved` /
   `Conflicted`), applied independently to the Wire's identity as a
   whole and, where relevant, to each of its two boundaries.

**"Next" is a physical-traversal relationship, not an electrical one.**
It means: the next conductor boundary encountered while following the
drawn conductor path outward from the Wire's other end. It explicitly
does **not** mean:

- the ultimate circuit destination,
- the final electrical destination,
- the furthest endpoint in the same electrical net,
- the furthest component in the same circuit, or
- the last object reachable by unconstrained graph traversal.

A Wire is **physical/conductive identity**. It is not electrical-net
identity (§13 makes this a foundational, standalone rule, because it is
the single most consequential distinction in this document).

## 2. Bidirectional Wire identity

A Wire is **physically non-directional**. "Terminal A → Terminal B" and
"Terminal B → Terminal A" name the same physical conductor and therefore
the same Wire — there are not two Wires here, regardless of which
boundary a reconstruction process happens to discover first.

Wherever the model uses `start_endpoint`/`end_endpoint` (or
`start_component_id`/`end_component_id`, etc.) field names, those names
denote a **deterministic representation ordering** (today, established by
comparing endpoint IDs so the same pair always produces the same
`start`/`end` assignment — see `WireReconstructor`'s
`if (endpoint.id > other_endpoint) continue;` de-duplication rule) — they
carry no claim about current-flow direction.

Current flow/direction, if it is ever represented, must be a **separate,
explicitly-derived presentation or electrical-model property**, layered
on top of (never substituting for) the physically-symmetric Wire
identity this document defines. Nothing in this specification introduces
such a property; none exists in the model today.

## 3. Conductor Boundary

> **A Conductor Boundary is the point at which the physical conductor
> represented by a Wire terminates.**

Three boundary states are distinguished, and must never be silently
collapsed into one another:

**A. Engineering boundary** — a boundary whose engineering identity is
known. Examples: a component terminal, a connector terminal, a ground
terminal, an external connection. (§4 makes each of these, when
established, a *hard* Wire boundary.)

**B. Geometric boundary** — a detected physical conductor end for which
the engineering terminal identity is **not yet known**. Example: a
`GeometricConductorEnd` — the line simply stops there, and no terminal,
connector, ground, or external-connection evidence currently explains
why.

**C. Unresolved boundary** — a boundary for which available evidence is
insufficient or actively contradictory, so no boundary classification
(A or B) can be asserted with confidence. This is distinct from B:
"insufficient evidence" here means the reconstruction cannot even
confidently say "no engineering identity is known but the geometry ends
here" — e.g. the geometry itself is ambiguous, or two boundary
interpretations conflict.

**The core rule of this section**: *finding where a line ends is
geometric evidence; determining what engineering terminal that line
represents is semantic/evidence resolution.* These are different
questions with different evidence requirements, and a reconstruction
process must never treat "I found where the line stops" as equivalent to
"I know what this line connects to." A `GeometricConductorEnd` remains
state B until independent terminal/connector/ground/external-connection
evidence promotes it to state A — never automatically, never by
proximity alone (see §17).

## 4. Hard Wire boundaries

The following, **when explicitly established as such**, are hard Wire
boundaries — a Wire terminates there, unconditionally:

- Component terminal
- Connector terminal
- Ground terminal
- External connection

"Explicitly established" means the boundary has reached engineering-
boundary state (§3.A) through defensible evidence, not merely that
geometry happens to be near one of these object kinds. A Wire must never
be extended *through* one of these boundaries merely because the
surrounding conductor geometry is electrically continuous on the far
side — electrical continuity across a hard boundary is exactly what §13
says is not sufficient to establish shared physical Wire identity.

## 5. Connector model

**This is the most consequential rule in this document, given AP-WIRE-028's finding that the connector-recognition path is currently unreachable (0 `ConnectorCandidate`s) — this section defines the rule that stage must obey once built.**

A connector terminal is a **hard** Wire boundary.

```
Wire A
──────────────► Connector Terminal 1
                         │
                    CONNECTOR
                         │
                    Connector Terminal 2
                         │
Wire B ◄─────────────────
```

This is **two** physical Wires, not one:

- **Wire A**: previous boundary → Connector Terminal 1
- **Wire B**: Connector Terminal 2 → next boundary

The connector/mating interface itself is **not part of either Wire's
physical conductor path** — it is a separate mechanical/electrical
object (the `Connector`/`ConnectorCandidate`) that two distinct Wires
each terminate against.

Current may flow electrically from Wire A through the connector interface
into Wire B, and the electrical net both wires belong to may (and
typically will) be the same net — but that does **not** make Wire A and
Wire B the same physical Wire. This is the concrete instance of the
general rule:

> **PHYSICAL WIRE IDENTITY ≠ ELECTRICAL CONTINUITY**

Connector terminals terminate individual physical conductors even when
those conductors participate in the same electrical circuit on both
sides of the mating interface.

## 6. Component-terminal rules

A component terminal is also a hard Wire boundary.

```
Component A
    [Terminal 1]
          │
          │
        WIRE
          │
          │
    [Terminal 2]
Component B
```

This is **one** physical Wire: Component A's Terminal 1 → Component B's
Terminal 2. The component itself — its internal structure, its other
terminals, any internal component-symbol geometry (AP-WIRE-023) — is
**not part of the Wire**. The Wire terminates at the terminal; it must
never be extended through the component merely because the component has
another terminal on its far side. Component-internal electrical behavior
(e.g. a diode's directionality, a relay's coil-vs-contact separation)
belongs to the component/electrical model, never to physical Wire
identity.

## 7. Ground-terminal rules

A ground terminal is a hard Wire boundary. A Wire connecting a component
terminal to a ground terminal is therefore exactly:

```
Component Terminal → Ground Terminal
```

— one Wire, with a ground terminal as one of its two boundaries. The
ground network/electrical net that terminal participates in may contain
many such conductors (AP-WIRE-028 §15 found 4 small 2-endpoint ground
nets in the TRX300 baseline, for example), but **each physical conductor
remains its own distinct Wire** unless the source explicitly establishes
a different physical construction (e.g. a genuinely shared/common ground
bus drawn as a single conductor run visited by multiple component leads —
which would itself be represented as ordinary shared-conductor-section
geometry per §15, not as a special case of "ground"). Ground must never
be treated as a single infinite Wire that absorbs every conductor that
eventually reaches it.

## 8. External-connection rules

An explicitly identified external connection is a hard Wire boundary. If
the source indicates a conductor leaves the represented harness/system
boundary (e.g. a connector or wire stub labeled as going to an
accessory, another harness, or "optional" equipment not otherwise drawn —
the TRX300 source's "OPTIONAL" connector is a candidate example, subject
to the same evidence rules as any other boundary classification), the
Wire may terminate at that external boundary even though its ultimate
external destination is not represented anywhere in the model. **The
external destination must never be invented** — the boundary is
"external connection, destination not represented," not a guess at what
that destination is.

## 9. Splice rules

**A Splice is not automatically a Wire endpoint.** This restates and
formalizes the project's existing standing rule (established before
AP-WIRE-013 and reaffirmed in every subsequent AP) — this document does
not change it, it grounds it in the Conductor Boundary framework.

A splice is an **internal conductor-topology event**. A Wire *may* pass
through one or more splices on its way to its actual conductor boundary.

```
             ┌──────────── Terminal B
             │
Terminal A ──●
             │
             └──────────── Terminal C
```

The splice does not itself terminate the physical conductor. The
topology may establish that A, B, and C are electrically connected
through the splice, while the question of physical Wire identity through
it — is this "one wire A→B with a branch to C," or "wire A→C with a
branch to B," or "three separate short conductors all landing at the
splice," or something else the source doesn't disambiguate — remains a
**separate question** that electrical connectivity alone does not answer.

**Explicitly forbidden**: assuming "splice = wire split" and mechanically
creating one Wire object per splice branch merely because the topology
graph branches there. At a splice, multiple physical-Wire interpretations
may be simultaneously plausible. **If available evidence does not
establish physical conductor identity through the splice, that ambiguity
must be preserved** (`Unresolved` or `Conflicted`), not resolved by
picking any single interpretation — including the interpretation "one
Wire per branch," which is itself a guess if nothing in the source
justifies it.

**The binding invariant of this section**: a `Splice` topology node is
**not** itself a Conductor Boundary (§3), under any circumstance —

> **Splice ≠ Conductor Boundary, therefore Splice ≠ Wire endpoint.**

The Splice must not become a fourth or fifth kind of Wire boundary
alongside component/connector/ground/external-connection (§4). This holds
even when physical Wire identity through the splice is genuinely
unresolved (§14) — an unresolved Wire identity is represented by the
Wire's status, never by relocating the Wire's boundary onto the Splice
node itself. If a future engineering representation ever needs to
represent a physical conductor terminating *at* a splice assembly (as
opposed to passing through it), that termination must be represented by
an appropriate conductor-boundary concept/evidence tied to the actual
physical termination — a concept this document does not define and does
not assume exists today.

## 10. Junction rules

A Junction follows the same fundamental rule as a Splice, unless and
until future source-specific evidence establishes a distinct engineering
meaning for it (none has been established as of this document; the
model's `TopologyNodeType::Junction` exists as a distinct enum value from
`Splice` but no extraction stage currently produces it — `AP-WIRE-028`'s
baseline shows 0 junction nodes on the TRX300 fixture).

A Junction is a **topology event**, not automatically a Wire endpoint.
The same three-way distinction §9 draws for splices — topology branching,
physical conductor identity, and electrical connectivity are three
different questions — applies identically here. Graph degree alone
(however many edges meet at a node) must never be used to define Wire
identity; degree is topology evidence, not physical-conductor evidence.

## 11. Crossing rules

A Crossing is a purely internal **geometric/topological** event with no
electrical meaning by definition (this restates the existing
`TopologyNodeType::Crossing` contract, reaffirmed unchanged by
AP-WIRE-026 and AP-WIRE-028). A Crossing:

- does not terminate a Wire,
- does not create a Wire boundary,
- does not establish electrical connectivity, and
- may occur along one or more Wire paths without affecting their
  identity.

```
Wire A:
Terminal A ─────────────── Terminal B
                 ╳
Wire B:
Terminal C ─────────────── Terminal D
```

The crossing does **not** create any of A↔C, A↔D, B↔C, or B↔D as
electrical or physical relationships, unless explicit source evidence
(e.g. a drawn junction dot at that exact point, which would make it a
Splice, not a Crossing) establishes an electrical junction there. The
existing Crossing/Splice distinction — visual overlap vs. real electrical
event — is load-bearing for this rule and must remain exactly as it is
today; this document does not alter it.

## 12. Continuation rules

A Continuation is **not** a Wire boundary. It is geometric/topological
evidence that a conductor continues uninterrupted (a degree-2 node with
no engineering significance of its own — typically just a bend or a
drawing-tool segment break in an otherwise straight run). A Wire
reconstruction process may freely traverse Continuation nodes without
terminating the Wire there — this is exactly what the current
`WireReconstructor` already does correctly.

However: **"Continuation" is a topology classification, not proof of
physical Wire identity.** A chain of Continuation nodes tells a
reconstruction process "keep going, nothing engineering-significant
happens here" — it does not, by itself, establish or confirm that the
two ends of that chain are the two boundaries of one coherent physical
Wire (that conclusion still requires the chain to actually terminate at
two real Conductor Boundaries, per §3–§8).

## 13. Electrical net vs. physical Wire

This is the foundational distinction the rest of this document depends
on, stated on its own for emphasis, matching AP-WIRE-028 §15's framing:

- **Electrical connectivity** answers: *"Which terminals/conductors are
  electrically connected?"*
- **Wire identity** answers: *"Which physical conductor path is this?"*

These are different questions, answered from different evidence, and
must never be conflated.

One electrical net may legitimately contain multiple Wires, connector
terminals, components, splices, ground terminals, and other electrical
objects. Therefore:

> **ONE NET ≠ ONE WIRE**, and **ONE WIRE ≠ ONE NET.**

A Wire may participate in exactly one electrical net (its two boundaries
and everything physically between them are, by definition, electrically
one thing) — but **the electrical net must never be used to retroactively
redefine physical Wire identity.** Concretely: knowing that endpoint X and
endpoint Y belong to the same electrical net is never, by itself, license
to conclude "therefore X and Y are the two boundaries of one Wire" — the
net only tells you they are electrically connected *somehow*, possibly
through several Wires, a splice, and a connector, exactly as §5, §9, and
§10 already establish for each of those object kinds individually.

## 14. Splice worked example

```
             B
             │
A ───────────●────────── C
```

At minimum, the model can establish: **A, B, and C are electrically
connected through the splice** — this is ordinary electrical-net
resolution (AP-WIRE-022), unaffected by this document.

It must **not** automatically assert a physical-Wire decomposition such
as "A→B and A→C" or "A→C and B→C" (or any other specific pairing) unless
the source provides evidence that actually establishes that physical
construction (for example: a genuinely different line weight/style
distinguishing a "through" run from a "branch" tap, or an explicit
harness-diagram convention the source itself defines and follows). If the
source does not provide sufficient evidence to establish the physical-
conductor relationships among A, B, and C, then:

- the **electrical** topology (A, B, C mutually connected via the
  splice) may still be resolved, per AP-WIRE-022's existing rules, while
- **physical Wire identity through the splice remains `Unresolved`.**
  The Splice node itself is never a Conductor Boundary and therefore
  never a Wire endpoint (§9) — that does not change here. What the
  source may support instead, once evidence is available, is: a Wire
  continuing through the splice to a real conductor boundary on the far
  side (§9/§15); multiple physical conductor paths associated with the
  splice, each still terminating at its own real conductor boundary
  elsewhere, not at the splice; or physical Wire identity remaining
  unresolved indefinitely if the source never provides evidence for it.
  If a future stage needs to represent a conductor terminating at a
  splice assembly, that termination must be represented through an
  appropriate conductor-boundary concept associated with the actual
  physical termination point — not by treating the Splice topology node
  itself as that boundary. No such concept is introduced by this
  document; the specification deliberately does not pick one of these
  outcomes here, because the source in this worked example does not
  either.

## 15. Shared conductor sections

The project's existing rule is preserved unchanged:

> "A wire may pass through one or more splices/junctions and continue to
> its final endpoint."

— once a future decomposition stage is able to establish that a specific
physical conductor genuinely does continue through a specific splice
(per §9/§14, only when evidence supports it, never by default).

Also preserved: the AP-WIRE-022A/AP-WIRE-028 observation that shared
conductor **geometry** may legitimately participate in multiple
endpoint-to-endpoint Wire traces where topology branches at a splice
(AP-WIRE-028 measured exactly 1 `CONDUCTOR-SHARED` case in the TRX300
baseline: one conductor segment referenced by more than one topology
edge/wire path).

This document adds one clarification the existing rule left implicit:
**do not imply that two Wire objects sharing a geometric segment
necessarily represent two physically separate pieces of copper drawn on
top of each other.** A shared conductor section is a **representation
fact** (the same drawn line segment is geometrically relevant to more
than one reconstructed Wire's path) — it is not automatically a claim
about how many actual physical conductors exist at that point on the
page. Whether a shared section represents one physical conductor
serving two Wire-relevant traversals, or genuinely-separate parallel
conductors drawn coincident on paper, is exactly the kind of question §3
says must not be silently collapsed: it requires its own evidence, and
absent that evidence, the representation-level fact (segment is shared)
should be reported as such rather than promoted into an unsupported claim
about physical copper count.

## 16. Terminal boundary vs. geometric end — case table

| Case | Description | Result |
|---|---|---|
| A | Line ends directly at a clearly identified component terminal | Known hard Wire boundary (§4, §6) |
| B | Line ends geometrically near a component but terminal identity is unresolved | Geometric boundary evidence exists (§3.B); engineering terminal association remains `Unresolved` — never promoted to A by proximity alone (§17) |
| C | Line reaches a connector terminal | Hard Wire boundary (§4, §5) |
| D | Line reaches a splice | Not automatically a Wire boundary (§9) |
| E | Line crosses another conductor | Not a Wire boundary (§11) |
| F | Line terminates at an unidentified location | Preserve as unresolved geometric boundary (§3.B/C) unless evidence establishes its engineering meaning |

## 17. Decision-scoped evidence priority (documented for future use — not implemented here)

There is **no single universal evidence ranking** that applies
identically to every engineering decision this specification touches.
Different engineering questions are answered by different primary
evidence — treating one global list as authoritative for all of them
would itself be a kind of guessing (applying evidence that is strong for
one question as if it were equally strong for an unrelated one). Instead,
evidence priority is **decision-scoped**: for each engineering question a
future Wire-reconstruction/conductor-boundary-resolution stage must
answer, this table names the evidence that is primary for *that*
question. **This is documentation of intended priority per decision
only; no reconstruction logic implementing any of it exists yet.**

| Decision | Primary evidence | Notes |
|---|---|---|
| Where does the physical conductor exist? | Conductor geometry / topology (detected line segments, `TopologyNode`/`TopologyEdge`) | The geometric substrate; always the starting evidence for any conductor question. |
| Where does the physical conductor terminate? | Conductor geometry + established Conductor Boundary evidence (§3) | Geometry alone only ever yields a *geometric* boundary (§3.B) — never, by itself, an *engineering* one. |
| What engineering terminal does this boundary represent? | Terminal / component / connector / ground evidence (§4, §6–§8) | This is the promotion from geometric boundary to engineering boundary (§3); text/label evidence associated with the location strengthens this. |
| Does this conductor pass through a splice? | Physical conductor geometry + explicit source conventions/evidence (§9, §14) | Splice/junction node adjacency alone is topology evidence, not physical-continuity evidence — it can suggest the question but cannot answer it. |
| Does a crossing establish connectivity? | Explicit source topology only | Ordinary `Crossing` classification means no connectivity, by definition (§11); nothing promotes a Crossing to a Splice except explicit source evidence of a real junction there. |
| Which objects are electrically connected? | Electrical topology + explicit circuit-role semantics (AP-WIRE-022) | This is electrical-net resolution's own evidence domain, independent of physical Wire identity (§13, §20). |
| What is the wire color? | Recognized color evidence associated with the conductor (AP-WIRE-025's `WireSemanticResolution.wire_color`) | Color evidence identifies/labels a conductor; it is never sufficient by itself to establish that two geometrically distinct runs are physically the same conductor, and can never override contradictory conductor geometry. |
| Are two objects the same physical Wire? | Established conductor boundaries (§3, §4) + a continuous physical conductor path between them + any applicable source evidence (§9, §14, §17) | The narrowest and strictest question in this table — see §18's never-guess invariants; this is never answered by topology, color, or electrical-net membership alone. |

**Decision-scoped priority does not mean evidence may be arbitrarily
selected to reach a desired result.** For every decision in the table
above, the following invariants remain binding regardless of which row
is in play:

- Proximity, by itself, is never sufficient to establish engineering
  identity for any decision in this table, and can never override an
  explicit, contradictory terminal/connector/ground assignment.
- Topology (splice/junction adjacency, node degree, crossing geometry)
  cannot invent a terminal — it is never primary evidence for "what
  engineering terminal does this boundary represent?" (row 3), only for
  the topology-scoped questions it is actually listed against.
- Graph degree alone cannot define physical Wire identity (§9, §10) — it
  is not listed as primary evidence for "are two objects the same
  physical Wire?" anywhere in this table.
- Electrical-net membership cannot define physical Wire identity (§13,
  §20) — "which objects are electrically connected" and "are two objects
  the same physical Wire" are different rows with different primary
  evidence, on purpose.
- Wire-color continuity cannot override contradictory conductor geometry
  — color is evidence about identity/labeling, never a substitute for an
  actual continuous physical path.
- Renderer behavior cannot resolve extraction ambiguity for any decision
  in this table — a rendering stage (AP-WIRE-027 and any successor)
  consumes already-resolved status; it must never be the place any of
  these questions gets decided, consistent with AP-WIRE-027's own "no
  recognition in the renderer" rule extended to every row here.

Any future inference that touches one of these decisions must still be
**explicit, documented, engineering-justified, evidence-backed, and
provenance-preserving** — decision-scoping evidence changes *which*
evidence is authoritative for a given question, it does not relax how
rigorously that evidence must be applied (§18 is unchanged by this
section).

## 18. Never-guess rule for Wire reconstruction

The following are explicit Wire-reconstruction invariants, to bind any
future implementation of the stages named in §20:

> **If multiple physically plausible Wire identities remain possible for
> a given piece of conductor geometry, the status of that Wire identity
> question MUST be `Unresolved` or `Conflicted`.**

A winner must **never** be selected merely because one candidate
interpretation:

- is geometrically straighter,
- is shorter,
- is closer,
- is more visually aligned,
- shares a color with an adjacent segment,
- produces a cleaner SVG,
- produces fewer Wire objects,
- produces more Wire objects, or
- produces a more convenient electrical-net structure.

None of these properties is primary evidence for "are two objects the
same physical Wire?" under §17's decision-scoped table — they are
rendering-, aesthetic-, or convenience-driven, and every
one of them is exactly the kind of "guess to get a nicer-looking or
simpler-looking result" the project's standing golden rule already
forbids (explicit evidence → Resolved; conflicting evidence → Conflicted;
insufficient evidence → Unresolved; never guess).

Any future heuristic that *does* get added to resolve some class of
these cases must be justified as an **explicit, named, documented
engineering rule** (the way AP-WIRE-026A's "label keyword + compatible
geometry" rule is explicit and documented, not an implicit shape
heuristic) and must preserve full provenance back to the evidence that
justified it.

## 19. Current WireReconstructor limitation

Documented here without change, per AP-WIRE-028:

The current `WireReconstructor` (`src/topology/wire_reconstructor.cpp`)
effectively implements:

```
degree-1 endpoint → degree-2 Continuation* → degree-1 endpoint
```

(`*` zero or more Continuation nodes), and **stops** the moment it
reaches any node of a different degree — i.e. any `Splice`, `Junction`,
or `Crossing` node, all of which have degree ≥ 3 in the current topology
graph.

> **The current `WireReconstructor` algorithm must not be treated as the
> authoritative definition of Wire identity.**

It is a conservative, never-guess-compliant **implementation scope
boundary**: given that no decomposition stage yet exists to resolve
splice/junction/crossing ambiguity per §9/§10/§18, stopping there is the
*correct* behavior under this specification, not a bug — but it is a
narrower question than "what is a Wire" as formally defined in §1. This
document exists precisely to make that gap explicit and named, so that a
future implementation is built against the formal definition (§1–§18)
rather than against the current algorithm's incidental boundary.

## 20. Proposed conceptual pipeline (architectural target — not implemented in this AP)

Physical-Wire reconstruction and electrical-net resolution are **separate
reasoning domains**, not sequential stages of one another. AP-WIRE-022's
electrical-net resolution already operates from topology and explicit
circuit-role evidence today, entirely independent of the (not yet built)
physical-Wire-identity reconstruction stage this document specifies —
and nothing in this AP changes that. The conceptual pipeline is therefore
drawn as two independent branches from a shared topology, not as one
strict chain:

```
                    SOURCE GEOMETRY
                          ↓
                       TOPOLOGY
                          │
              ┌───────────┴───────────┐
              ↓                       ↓
      CONDUCTOR BOUNDARY /     ELECTRICAL CONNECTIVITY
     TERMINAL RESOLUTION           RESOLUTION
              ↓                   (AP-WIRE-022,
   PHYSICAL WIRE IDENTITY          existing today)
      RECONSTRUCTION                    │
              │                         │
              └───────────┬─────────────┘
                          ↓
              INTEGRATED ENGINEERING MODEL
              (EngineeringDiagram — AP-WIRE-026)
                          ↓
                STRUCTURED ENGINEERING DIAGRAM
                          ↓
                  SVG / DIAGRAM STUDIO
```

The exact visual arrangement is illustrative; the following statements
are the binding part and must hold regardless of how a future
implementation happens to sequence its own internal passes:

1. Electrical-net resolution and physical-Wire reconstruction are
   separate concerns, governed by separate rules (§13 for the former's
   relationship to the latter; §9/§10/§18 for how physical-Wire
   reconstruction itself must behave).
2. Electrical-net resolution may operate before, after, or independently
   of complete Wire reconstruction, wherever the available evidence
   permits — it is not architecturally required to wait for physical
   Wire identity to be resolved first (unlike the strictly sequential
   presentation in an earlier draft of this document, which this
   correction supersedes).
3. Physical Wire identity must never be inferred merely because two
   endpoints belong to the same electrical net (§13, §17).
4. Electrical-net membership may be consumed as contextual evidence by a
   future physical-Wire-reconstruction stage only where a future
   specification explicitly permits it — it must never become proof of
   physical Wire identity by itself.
5. AP-WIRE-022's existing electrical-net-resolution behavior is not
   invalidated, superseded, or required to change by this document. It
   already operates correctly, and independently of physical Wire
   identity, today.

**No part of this pipeline, and no part of AP-WIRE-022, is implemented,
reordered, or otherwise touched by this AP.** It is recorded here as the
target that AP-WIRE-030+ should be evaluated against.

## 21. Explicit answers to the required questions

1. **What terminates a Wire?** A hard Conductor Boundary that has reached
   engineering-boundary state (§3.A): a component terminal, a connector
   terminal, a ground terminal, or an explicit external connection (§4–§8).
2. **What does not terminate a Wire?** A Splice (§9), a Junction (§10),
   or a Crossing (§11) — none of these is, by itself, a Wire boundary. A
   bare Continuation node also does not terminate a Wire (§12) — it is
   simply traversed.
3. **Can a Wire pass through a splice?** Yes, when evidence establishes
   the specific physical continuity (§9, §14) — but this is not assumed
   by default, and is not yet implemented (§19).
4. **Can a Wire pass through a crossing?** Yes, trivially and always — a
   crossing has no electrical or boundary significance and does not
   interrupt any Wire's path (§11).
5. **Can a Wire pass through a junction?** Same answer as splices (§10):
   yes, when evidence establishes it; not by default.
6. **Does a connector terminal terminate a Wire?** Yes, always, when
   established (§4, §5) — this is a hard boundary.
7. **Does a component terminal terminate a Wire?** Yes, always, when
   established (§4, §6) — hard boundary.
8. **Does a ground terminal terminate a Wire?** Yes, always, when
   established (§4, §7) — hard boundary.
9. **Does a geometric conductor end automatically constitute an
   engineering terminal?** No (§3) — a `GeometricConductorEnd` is state B
   (geometric boundary) until independent evidence promotes it to state A
   (engineering boundary). Geometry alone never performs that promotion.
10. **Can multiple Wires share a physical conductor segment in the
    representation?** Yes, as a representation fact (§15) — this does not
    by itself imply two physically separate conductors exist there; that
    is a separate question requiring its own evidence.
11. **Is electrical continuity sufficient to establish physical Wire
    identity?** No (§5, §13) — this is the central rule of the whole
    document. Physical Wire identity ≠ electrical continuity.
12. **Is topology alone sufficient to establish physical Wire identity?**
    No (§10, §17) — graph degree and adjacency are topology evidence, and
    §17's decision-scoped table does not list topology as primary
    evidence for "are two objects the same physical Wire?"; it is primary
    only for narrower questions such as "does this conductor pass through
    a splice?" and "which objects are electrically connected?".
13. **What happens when physical Wire identity is ambiguous?** Status
    must be `Unresolved` (insufficient evidence) or `Conflicted`
    (contradictory evidence) — never resolved by picking a convenient
    interpretation (§9, §14, §18).
14. **What happens when terminal evidence conflicts?** The boundary is
    `Conflicted` (§3.C), and — per the standing AP-WIRE-024 precedent,
    unchanged by this document — no single interpretation is selected as
    a winner; the conflict is preserved and remains traceable.
15. **Is Wire direction equivalent to current direction?** No (§2). A
    Wire is physically non-directional; `start`/`end` is representation
    ordering only. Current direction, if ever modeled, is a separate
    derived property.
16. **Can a Wire belong to an electrical net?** Yes, typically exactly
    one (§13) — a Wire's two boundaries and the conductor between them
    are, by definition, electrically one thing.
17. **Can an electrical net contain multiple Wires?** Yes, routinely
    (§13) — a net is a broader electrical-connectivity concept that can
    span several Wires, splices, connectors, and components.
18. **Can a Wire contain internal topology events?** Yes — Continuation
    nodes always (§12), and Crossings always (§11, since a crossing never
    interrupts a Wire's path); Splices/Junctions only when evidence
    establishes the Wire genuinely continues through them (§9/§10).
19. **Does a crossing establish connectivity?** No, never, by definition
    (§11) — establishing connectivity at a crossing point requires
    explicit source evidence that would make it a Splice instead, not a
    Crossing.
20. **Does a splice establish connectivity?** Yes — a splice is precisely
    an electrical-connectivity event among its incident conductors (§9,
    §14); what it does *not* automatically establish is physical Wire
    identity through it.
21. **Can a connector electrically connect two Wires without making them
    one Wire?** Yes — this is exactly §5's rule, restated as the
    canonical example of §13's physical-identity-vs-electrical-
    continuity distinction.

## Decision record / rationale

- The formal Wire definition (§1) was adopted verbatim as specified in
  the handoff, including the explicit rejection of "ultimate circuit
  destination" as a meaning of "next" — this was a deliberate choice to
  keep Wire identity a purely local, physically-grounded concept,
  decoupled from whatever the electrical net eventually turns out to be
  (§13).
- Connector terminals and component terminals were both made **hard**
  boundaries (§5, §6) rather than "soft" or "sometimes" boundaries,
  because the alternative — extending a Wire through a terminal whenever
  the far side looks electrically continuous — is precisely the kind of
  convenience-driven inference §18 forbids, and would make Wire identity
  depend on electrical-net computation instead of the reverse (§13).
- Splices, Junctions, and Crossings were deliberately **not** given a
  uniform rule ("all topology events behave the same") even though §9
  and §10 turn out to share the same rule in practice — they are
  documented as separate sections because they are separate model
  concepts (`TopologyNodeType::Splice`/`Junction`/`Crossing`) with
  different underlying evidence (a real conductor junction vs. an
  unimplemented-but-distinct node type vs. pure visual overlap), and a
  future reader should not have to infer that Junction inherits Splice's
  rule only by analogy.
- The evidence priority in §17 was documented as a **decision-scoped
  table**, not a single universal ranking, without assigning numeric
  confidence weights or a scoring formula — no such formula currently
  exists anywhere in the codebase (every existing resolver — AP-WIRE-019,
  024, 025, 026A — uses discrete Resolved/Unresolved/Conflicted branching
  on categorical evidence presence/absence, never a weighted score) and
  inventing one here would be exactly the kind of unimplemented,
  undemonstrated mechanism this document is supposed to avoid asserting.

## Correction record (this pass, commit `303c888` → this commit)

An architecture review of `303c888` accepted this specification's
architecture but required three corrections before it could be treated
as final:

1. **§14 no longer describes the Splice node itself as a Wire
   boundary.** The prior wording — "there may be three short Wires each
   terminating at the splice as a boundary of its own kind" — implicitly
   made Splice a fifth kind of Wire boundary alongside §4's four,
   contradicting §9's own opening sentence. §14 now describes the
   possible outcomes (a Wire continuing through the splice to a real
   boundary elsewhere, multiple conductor paths each terminating at their
   own real boundary elsewhere, or indefinite `Unresolved` status)
   without ever placing a Wire's endpoint at the Splice node. §9 gained an
   explicit standalone invariant, **Splice ≠ Conductor Boundary, therefore
   Splice ≠ Wire endpoint**, so this rule no longer depends on inference
   from the worked example alone.
2. **§20's pipeline is no longer strictly sequential between physical-
   Wire reconstruction and electrical-net resolution.** The prior diagram
   placed "ELECTRICAL NET RESOLUTION" after "PHYSICAL WIRE IDENTITY
   RECONSTRUCTION" in a single chain, which risked being read as
   "electrical-net resolution must wait for Wire reconstruction to
   finish" — untrue today (AP-WIRE-022 already resolves nets from
   topology and circuit-role evidence independently of any Wire-
   reconstruction stage) and not a requirement this document intends to
   impose on the future either. §20 now shows the two as independent
   branches from shared topology, with five explicit statements binding
   the independence regardless of how a future implementation happens to
   sequence its own passes internally.
3. **§17 is now a decision-scoped evidence-priority table, not one
   universal nine-level ranking.** The prior single list risked being
   read as "level 1 evidence always outranks level 9 evidence for any
   question," which is false — e.g. electrical topology is *primary*
   evidence for "which objects are electrically connected" but is not
   listed at all for "are two objects the same physical Wire." The table
   now states, per decision, what evidence is primary for that decision
   specifically, while preserving every never-guess invariant the prior
   section stated (proximity/topology/degree/net-membership/renderer
   behavior can still never establish or override physical Wire
   identity) — decision-scoping changes which evidence is authoritative
   for a question, it does not loosen how rigorously that evidence must
   be applied.

None of §1–§13, §15, §16, §18, §19, or §21 required correction; the
review found them architecturally sound as originally written. §21's
answer 12 and §18's cross-reference to §17 were updated only to match
§17's new decision-scoped structure, not to change their conclusions.

## Open questions

The handoff instructed that genuinely open items be marked OPEN rather
than resolved by assumption. Two remain:

- **OPEN — Junction's distinct engineering meaning.** §10 notes that
  `TopologyNodeType::Junction` exists as an enum value distinct from
  `Splice` but is never produced by any current extraction stage (0
  instances in the AP-WIRE-028 TRX300 baseline). Whether "Junction" is
  intended to eventually mean something engineering-distinct from
  "Splice" (e.g. a different physical construction, such as a bus bar vs.
  a twisted/soldered splice), or whether it is legacy/reserved
  vocabulary that should simply be treated as a Splice synonym going
  forward, is not established anywhere in the project's history available
  to this AP. This document takes the conservative position (same rule as
  Splice, §10) without resolving which of those two futures is correct.
- **OPEN — Shared-conductor physical-copper-count question (§15).**
  Whether a geometrically-shared conductor section ever needs to be
  resolved into "one physical conductor" vs. "coincidentally overlapping
  separate conductors" as a first-class model question (with its own
  evidence and status), or whether it is sufficient for the model to
  simply record which Wires reference a shared segment and leave the
  physical-copper-count question permanently unasked (because the source
  diagrams this project targets never need to answer it), is not decided
  here. §15 states the representation-level fact and the boundary of what
  it does and doesn't imply, but does not propose a resolution mechanism,
  because no evidence source for that mechanism has been identified yet.

No other item raised by the handoff's 21 required questions, worked
examples, or rule sections was left unresolved — each has an explicit
answer in §21 and a grounding rule in §1–§18.

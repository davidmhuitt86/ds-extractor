# WIRE-IDENTITY-001
# Endpoint-to-Endpoint Wire Identity

Status: Normative foundation rule  
Applies to: EKE-DX-WIRE reconstruction model

## 1. Rule

A **wire** is one continuous conductor traced from one true endpoint to
another true endpoint.

A detected pixel segment is not automatically a wire.

## 2. Junctions and splices

A splice or junction is an internal topology node.

It does not, by itself, terminate a wire or divide one wire object into
multiple wire objects.

A wire may therefore pass through one or more junctions while remaining one
endpoint-to-endpoint wire trace.

## 3. Branching example

Given:

    Terminal A ---------o--------- Terminal B
                        |
                        +--------- Terminal C

the intended wire objects are:

    Wire 1: Terminal A -> Terminal B
    Wire 2: Terminal A -> Terminal C

The conductor geometry from Terminal A to the junction is shared by both
wire traces.

The model must therefore allow one physical conductor section to be
referenced by more than one endpoint-to-endpoint Wire object.

## 4. Required separation of concepts

The implementation shall keep these concepts separate:

1. Conductor geometry — observable geometry reconstructed from image evidence.
2. Topology graph — connectivity, junctions, crossings, and continuation.
3. True endpoint — a semantic termination such as a component terminal or
   other recognized conductor endpoint.
4. Wire identity — endpoint-to-endpoint trace derived from topology and
   endpoint semantics.

A conductor detector shall never assign wire identity solely from pixels.

## 5. Consequence for topology algorithms

A junction node must remain present in the topology graph even when a wire
trace passes through it.

The path-chaining stage must operate on topology and endpoint semantics,
not on the number of raw image segments.

A splice/junction must not be treated as an endpoint merely because the
image detector created a segment boundary at that location.

## 6. Unknowns

If true endpoint semantics cannot be established, the engine shall preserve
the geometric/topological evidence and mark wire identity unresolved.

It shall not fabricate a component, terminal, or wire endpoint merely to
complete a path.

## 7. Serialization

Every resolved Wire shall identify:

- stable Wire ID;
- start endpoint node;
- end endpoint node;
- topology edges traversed;
- conductor segments contributing geometry;
- confidence state.

This representation is independent of SVG rendering.

## 8. Validation invariant

For every resolved Wire:

    start_endpoint != end_endpoint

and the wire path must be topologically connected from start to end.

Junction nodes may occur internally on the path.

## 9. Design rationale

This rule preserves the distinction between observed geometry and engineering
identity. It also permits shared conductor sections at branch points without
creating artificial wire boundaries at every splice or junction.

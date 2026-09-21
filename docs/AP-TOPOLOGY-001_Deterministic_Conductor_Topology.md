# AP-TOPOLOGY-001
# Deterministic Conductor Topology Reconstruction

Status: Implemented foundation
Revision: 0.1

## Purpose

Convert extracted ConductorSegment geometry into a deterministic topology
graph without prematurely assigning electrical wire identity.

## Processing boundary

    ConductorSegment
          |
          v
    intersection analysis
          |
          v
    split points
          |
          v
    topology nodes
          |
          v
    topology edges
          |
          v
    endpoint-to-endpoint wire reconstruction (later stage)

## Node rules

### Conductor end

A segment endpoint with no detected continuation or junction relationship is
represented as ConductorEnd.

This is a geometric endpoint, not necessarily a true electrical endpoint.

### Continuation

Two conductor segments meeting at their endpoints form a continuation node.
This preserves connectivity through a detected bend without declaring the node
to be a component terminal.

### Junction

An endpoint of one conductor meeting the interior of another conductor is
classified as a Junction.

A junction is electrically connective.

A junction is not a wire endpoint.

### Crossing

When two orthogonal conductors intersect through the interiors of both
segments, the location is classified as Crossing.

The crossing node is retained for geometric evidence, but
electrically_connective = false.

Therefore the topology graph cannot accidentally convert a visual crossing
into an electrical connection.

## Important limitation

This stage does not determine whether a geometric conductor end is a true
engineering endpoint.

That requires later evidence such as:

- component boundaries;
- connector/terminal geometry;
- recognized symbols;
- explicit source annotations;
- validated manual review.

Until that evidence exists, a geometric conductor end remains provisional.

## Wire identity

Wire identity is intentionally not generated here.

The later wire stage must trace from one true endpoint to another true endpoint.
Junctions and splices remain internal nodes.

A wire may pass through one or more junctions.

Shared conductor geometry may therefore participate in multiple endpoint-to-endpoint
wire traces at branch points.

## Determinism

For identical source geometry and configuration:

- node ordering is deterministic;
- node IDs are stable;
- edge IDs are stable;
- crossing connectivity is explicit;
- no SVG state is required to reconstruct topology.

## Test fixtures

The implementation currently verifies:

1. perpendicular interior crossing;
2. endpoint-to-interior T-junction;
3. crossing is non-electrically-connective;
4. junction is electrically connective.

The next topology increment should add collinear continuation, duplicate
suppression, and endpoint/component attachment.

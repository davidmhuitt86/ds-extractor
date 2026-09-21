# EKE-DX-WIRE-MDL-001
# Domain Model & Intermediate Representation

---

## 1. Design goal

The internal model must represent the reconstructed engineering evidence independently from SVG.

---

## 2. Primitive geometry

```cpp
struct Point {
    double x;
    double y;
};

struct Segment {
    Point a;
    Point b;
};

struct Polyline {
    std::vector<Point> points;
};

struct BoundingBox {
    double x;
    double y;
    double width;
    double height;
};
```

Coordinate system shall be explicitly defined.

---

## 3. Wire

```cpp
struct Wire {
    ObjectId id;
    Polyline geometry;
    double observed_width;
    WireClass classification;
    Confidence geometry_confidence;
    Confidence topology_confidence;
    Provenance provenance;
    NodeId endpoint_a;
    NodeId endpoint_b;
};
```

---

## 4. Topology node

```cpp
struct TopologyNode {
    NodeId id;
    Point position;
    NodeType type;
    std::vector<EdgeId> incident_edges;
    Confidence confidence;
    Provenance provenance;
};
```

Node types:

```text
Endpoint
Continuation
Junction
Crossing
ComponentBoundary
Unresolved
```

---

## 5. Topology edge

```cpp
struct TopologyEdge {
    EdgeId id;
    WireId wire;
    NodeId from;
    NodeId to;
    Polyline geometry;
};
```

---

## 6. Non-wire region

```cpp
struct NonWireRegion {
    RegionId id;
    BoundingBox bounds;
    RegionType type;
    Confidence confidence;
    Provenance provenance;
};
```

---

## 7. Provenance

```cpp
struct Provenance {
    SourceId source;
    int page;
    BoundingBox source_region;
    std::vector<ObjectId> parent_objects;
    std::string processing_stage;
    ParameterSet parameters;
};
```

---

## 8. Confidence

Confidence is multidimensional:

```cpp
struct Confidence {
    double geometric;
    double topological;
    double semantic;
};
```

The wire stage normally populates:

```text
geometric
topological
```

Semantic confidence remains unknown until semantic extraction.

---

## 9. Serialization

The intermediate representation should serialize to JSON during initial development.

Example:

```json
{
  "schema": "eke.dx.wire.1",
  "wires": [],
  "nodes": [],
  "edges": [],
  "regions": [],
  "provenance": {}
}
```

A future `.oep` package can contain this representation as a deterministic engineering object.

---

## 10. Stable identity

IDs shall be stable within a reconstruction artifact.

If deterministic content-addressed identity is later required, IDs may be derived from:

```text
normalized geometry
+
object type
+
source identity
+
parent identity
```

Human-readable IDs should remain available for editing/debugging.

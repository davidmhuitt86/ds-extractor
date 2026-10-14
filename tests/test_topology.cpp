#include "eke_dx_wire/topology/topology_reconstructor.hpp"

#include <algorithm>
#include <cassert>
#include <map>
#include <string>
#include <tuple>
#include <vector>

using namespace eke::dx::wire;

static ConductorSegment segment(
    const char* id, Point2D a, Point2D b) {
    ConductorSegment s;
    s.id = id;
    s.geometry = {a, b};
    return s;
}

using PointKey = std::tuple<double, double>;
using SegmentKey = std::tuple<double, double, double, double>;
using NodeSignature = std::tuple<double, double, int, bool>;
using EdgeSignature = std::tuple<SegmentKey, PointKey, PointKey>;

static PointKey point_key(Point2D point) {
    return {point.x, point.y};
}

static SegmentKey canonical_segment_key(const Segment2D& geometry) {
    Point2D a = geometry.a;
    Point2D b = geometry.b;
    if (point_key(b) < point_key(a)) {
        std::swap(a, b);
    }
    return {a.x, a.y, b.x, b.y};
}

static std::vector<NodeSignature> node_signature(
    const TopologyArtifacts& graph) {
    std::vector<NodeSignature> result;
    result.reserve(graph.nodes.size());
    for (const auto& node : graph.nodes) {
        result.emplace_back(
            node.position.x,
            node.position.y,
            static_cast<int>(node.type),
            node.electrically_connective);
    }
    std::sort(result.begin(), result.end());
    return result;
}

static std::vector<EdgeSignature> edge_signature(
    const TopologyArtifacts& graph,
    const std::vector<ConductorSegment>& segments) {
    std::map<std::string, PointKey> position_by_node;
    for (const auto& node : graph.nodes) {
        position_by_node.emplace(node.id, point_key(node.position));
    }

    std::map<std::string, SegmentKey> geometry_by_segment;
    for (const auto& conductor : segments) {
        geometry_by_segment.emplace(
            conductor.id, canonical_segment_key(conductor.geometry));
    }

    std::vector<EdgeSignature> result;
    result.reserve(graph.edges.size());
    for (const auto& edge : graph.edges) {
        const auto from = position_by_node.find(edge.from_node);
        const auto to = position_by_node.find(edge.to_node);
        const auto conductor = geometry_by_segment.find(edge.conductor_segment);
        assert(from != position_by_node.end());
        assert(to != position_by_node.end());
        assert(conductor != geometry_by_segment.end());

        PointKey a = from->second;
        PointKey b = to->second;
        if (b < a) {
            std::swap(a, b);
        }
        result.emplace_back(conductor->second, a, b);
    }
    std::sort(result.begin(), result.end());
    return result;
}

int main() {
    {
        std::vector<ConductorSegment> segments{
            segment("h", {0, 10}, {20, 10}),
            segment("v", {10, 0}, {10, 20})
        };

        const auto graph =
            TopologyReconstructor().reconstruct(segments, "fixture");

        assert(graph.nodes.size() == 5);
        assert(graph.edges.size() == 4);

        int crossings = 0;
        for (const auto& node : graph.nodes) {
            if (node.type == TopologyNodeType::Crossing) {
                ++crossings;
                assert(!node.electrically_connective);
            }
        }
        assert(crossings == 1);
    }

    {
        std::vector<ConductorSegment> segments{
            segment("main", {0, 10}, {20, 10}),
            segment("branch", {10, 10}, {10, 20})
        };

        const auto graph =
            TopologyReconstructor().reconstruct(segments, "fixture");

        assert(graph.nodes.size() == 4);

        int junctions = 0;
        for (const auto& node : graph.nodes) {
            if (node.type == TopologyNodeType::Junction) {
                ++junctions;
                assert(node.electrically_connective);
            }
        }
        assert(junctions == 0);

        int splices = 0;
        for (const auto& node : graph.nodes) {
            if (node.type == TopologyNodeType::Splice) {
                ++splices;
                assert(node.electrically_connective);
            }
        }
        assert(splices == 1);
        assert(graph.edges.size() == 3);
    }


    {
        // These orthogonal segments create distinct intersection coordinates
        // within snap tolerance. If topology reconstruction processes input
        // segments in caller order, the first encountered coordinate becomes
        // the representative and the resulting graph geometry changes.
        const std::vector<ConductorSegment> first_order{
            segment("h-main", {0, 10}, {100, 10}),
            segment("v-left", {50, 0}, {50, 20}),
            segment("h-offset", {20, 12}, {80, 12}),
            segment("v-right", {52, 0}, {52, 20})
        };
        auto reverse_order = first_order;
        std::reverse(reverse_order.begin(), reverse_order.end());

        const auto first_graph =
            TopologyReconstructor().reconstruct(first_order, "order-invariance");
        const auto reverse_graph =
            TopologyReconstructor().reconstruct(reverse_order, "order-invariance");

        assert(node_signature(first_graph) == node_signature(reverse_graph));
        assert(edge_signature(first_graph, first_order) ==
               edge_signature(reverse_graph, reverse_order));
    }

    return 0;
}

#include "eke_dx_wire/topology/topology_reconstructor.hpp"

#include <cassert>

using namespace eke::dx::wire;

static ConductorSegment segment(
    const char* id, Point2D a, Point2D b) {
    ConductorSegment s;
    s.id = id;
    s.geometry = {a, b};
    return s;
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
        assert(junctions == 1);
        assert(graph.edges.size() == 3);
    }

    return 0;
}

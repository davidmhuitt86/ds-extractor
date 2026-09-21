#include "eke_dx_wire/topology/endpoint_reconstructor.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cassert>

using namespace eke::dx::wire;

static TopologyNode node(
    const char* id,
    TopologyNodeType type,
    double x = 0.0,
    double y = 0.0) {
    TopologyNode n;
    n.id = id;
    n.type = type;
    n.position = {x, y};
    return n;
}

static TopologyEdge edge(
    const char* id, const char* from, const char* to) {
    TopologyEdge e;
    e.id = id;
    e.from_node = from;
    e.to_node = to;
    return e;
}

int main() {
    {
        std::vector<TopologyNode> nodes{
            node("a", TopologyNodeType::ConductorEnd),
            node("b", TopologyNodeType::Continuation),
            node("c", TopologyNodeType::ConductorEnd)
        };

        std::vector<TopologyEdge> edges{
            edge("e1", "a", "b"),
            edge("e2", "b", "c")
        };

        const auto result =
            EndpointReconstructor().reconstruct(
                nodes, edges, "fixture");

        assert(result.candidates.size() == 2);
        assert(std::all_of(
            result.candidates.begin(),
            result.candidates.end(),
            [](const EndpointCandidate& candidate) {
                return candidate.kind ==
                           EndpointKind::GeometricConductorEnd &&
                       candidate.confidence == ConfidenceClass::Low &&
                       candidate.incident_edges.size() == 1;
            }));
    }

    {
        std::vector<TopologyNode> nodes{
            node("a", TopologyNodeType::ConductorEnd),
            node("j", TopologyNodeType::Junction),
            node("b", TopologyNodeType::ConductorEnd),
            node("c", TopologyNodeType::ConductorEnd)
        };

        std::vector<TopologyEdge> edges{
            edge("e1", "a", "j"),
            edge("e2", "j", "b"),
            edge("e3", "j", "c")
        };

        const auto result =
            EndpointReconstructor().reconstruct(
                nodes, edges, "fixture");

        assert(result.candidates.size() == 3);
        assert(std::none_of(
            result.candidates.begin(),
            result.candidates.end(),
            [](const EndpointCandidate& candidate) {
                return candidate.node_id == "j";
            }));
    }

    {
        cv::Mat source(50, 50, CV_8UC1, cv::Scalar(255));
        cv::line(source, {5, 25}, {20, 25}, cv::Scalar(0), 1);
        cv::rectangle(source, {27, 22}, {32, 28}, cv::Scalar(0), cv::FILLED);

        std::vector<TopologyNode> nodes{
            node("a", TopologyNodeType::ConductorEnd, 20, 25),
            node("b", TopologyNodeType::Continuation, 5, 25)
        };

        std::vector<TopologyEdge> edges{
            edge("e1", "a", "b")
        };

        const auto result =
            EndpointReconstructor().reconstruct(
                nodes, edges, source, "fixture");

        assert(result.candidates.size() == 1);

        const auto& evidence = result.candidates.front().evidence;
        assert(evidence.local_pixel_count > 0);
        assert(evidence.local_ink_pixels > 0);
        assert(evidence.local_ink_density > 0.0);
        assert(evidence.forward_pixel_count > 0);
        assert(evidence.forward_ink_pixels > 0);
        assert(evidence.forward_ink_density > 0.0);
        assert(evidence.source_region.x == 10);
        assert(evidence.source_region.y == 15);
        assert(evidence.source_region.width == 21);
        assert(evidence.source_region.height == 21);
    }

    return 0;
}

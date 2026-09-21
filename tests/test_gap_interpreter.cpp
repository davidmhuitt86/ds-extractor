#include "eke_dx_wire/topology/gap_interpreter.hpp"

#include <opencv2/imgproc.hpp>

#include <cassert>

using namespace eke::dx::wire;

static TopologyNode node(
    const char* id,
    TopologyNodeType type,
    double x,
    double y) {
    TopologyNode n;
    n.id = id;
    n.type = type;
    n.position = {x, y};
    return n;
}

static TopologyEdge edge(
    const char* id,
    const char* from,
    const char* to) {
    TopologyEdge e;
    e.id = id;
    e.from_node = from;
    e.to_node = to;
    e.conductor_segment = "segment";
    return e;
}

int main() {
    cv::Mat source(40, 80, CV_8UC1, cv::Scalar(255));

    // Two collinear conductor fragments separated by an annotation-shaped
    // dark region. The gap interpreter should infer one continuation.
    cv::line(source, {5, 20}, {30, 20}, cv::Scalar(0), 1);
    cv::rectangle(source, {34, 16}, {45, 24}, cv::Scalar(0), cv::FILLED);
    cv::line(source, {49, 20}, {75, 20}, cv::Scalar(0), 1);

    std::vector<TopologyNode> nodes{
        node("a", TopologyNodeType::ConductorEnd, 30, 20),
        node("a-neighbor", TopologyNodeType::Continuation, 5, 20),
        node("b", TopologyNodeType::ConductorEnd, 49, 20),
        node("b-neighbor", TopologyNodeType::Continuation, 75, 20)
    };

    std::vector<TopologyEdge> edges{
        edge("e1", "a", "a-neighbor"),
        edge("e2", "b", "b-neighbor")
    };

    GapInterpretationConfig config;
    config.maximum_gap = 20.0;

    const auto result = GapInterpreter(config).interpret(
        nodes, edges, source, "fixture", 0);

    assert(result.inferred_edges.size() == 1);
    assert(result.inferred_edges.front().from_node == "a");
    assert(result.inferred_edges.front().to_node == "b");
    assert(nodes[0].type == TopologyNodeType::Continuation);
    assert(nodes[2].type == TopologyNodeType::Continuation);

    // An empty gap is deliberately not inferred as an annotation bridge.
    cv::Mat blank(40, 80, CV_8UC1, cv::Scalar(255));
    cv::line(blank, {5, 20}, {30, 20}, cv::Scalar(0), 1);
    cv::line(blank, {49, 20}, {75, 20}, cv::Scalar(0), 1);

    std::vector<TopologyNode> blank_nodes{
        node("a", TopologyNodeType::ConductorEnd, 30, 20),
        node("a-neighbor", TopologyNodeType::Continuation, 5, 20),
        node("b", TopologyNodeType::ConductorEnd, 49, 20),
        node("b-neighbor", TopologyNodeType::Continuation, 75, 20)
    };

    const auto blank_result = GapInterpreter(config).interpret(
        blank_nodes, edges, blank, "fixture", 0);

    assert(blank_result.inferred_edges.empty());

    return 0;
}

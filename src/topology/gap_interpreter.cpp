#include "eke_dx_wire/topology/gap_interpreter.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace eke::dx::wire {
namespace {

struct EndpointInfo {
    std::size_t node_index {};
    int degree {};
};

bool dark_pixel(const cv::Mat& image, int x, int y) {
    if (x < 0 || y < 0 || x >= image.cols || y >= image.rows)
        return false;

    if (image.channels() == 1)
        return image.at<std::uint8_t>(y, x) < 180;

    const auto* p = image.ptr<cv::Vec3b>(y);
    const cv::Vec3b& pixel = p[x];
    return (static_cast<int>(pixel[0]) +
            static_cast<int>(pixel[1]) +
            static_cast<int>(pixel[2])) / 3 < 180;
}

double gap_ink_density(
    const cv::Mat& image,
    Point2D a,
    Point2D b) {

    if (image.empty())
        return 0.0;

    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double length = std::hypot(dx, dy);

    if (length <= 0.0)
        return 0.0;

    const int samples =
        (std::max)(1, static_cast<int>(std::ceil(length)));

    int dark = 0;
    int total = 0;

    for (int i = 1; i < samples; ++i) {
        const double t = static_cast<double>(i) /
                         static_cast<double>(samples);

        const int x = static_cast<int>(
            std::lround(a.x + dx * t));
        const int y = static_cast<int>(
            std::lround(a.y + dy * t));

        ++total;
        if (dark_pixel(image, x, y))
            ++dark;
    }

    return total == 0
        ? 0.0
        : static_cast<double>(dark) / static_cast<double>(total);
}

bool collinear_facing(
    const TopologyNode& a,
    const TopologyNode& b,
    const TopologyNode* a_neighbor,
    const TopologyNode* b_neighbor,
    double tolerance) {

    const double dx = b.position.x - a.position.x;
    const double dy = b.position.y - a.position.y;

    const bool horizontal =
        std::abs(dy) <= tolerance && std::abs(dx) > tolerance;
    const bool vertical =
        std::abs(dx) <= tolerance && std::abs(dy) > tolerance;

    if (!horizontal && !vertical)
        return false;

    if (a_neighbor == nullptr || b_neighbor == nullptr)
        return false;

    const double a_dir_x = a.position.x - a_neighbor->position.x;
    const double a_dir_y = a.position.y - a_neighbor->position.y;
    const double b_dir_x = b.position.x - b_neighbor->position.x;
    const double b_dir_y = b.position.y - b_neighbor->position.y;

    if (horizontal) {
        return (a_dir_x * dx > 0.0) &&
               (b_dir_x * -dx > 0.0) &&
               std::abs(a_dir_y) <= tolerance &&
               std::abs(b_dir_y) <= tolerance;
    }

    return (a_dir_y * dy > 0.0) &&
           (b_dir_y * -dy > 0.0) &&
           std::abs(a_dir_x) <= tolerance &&
           std::abs(b_dir_x) <= tolerance;
}

} // namespace

GapInterpreter::GapInterpreter(GapInterpretationConfig config)
    : config_(config) {}

GapInterpretationArtifacts GapInterpreter::interpret(
    std::vector<TopologyNode>& nodes,
    const std::vector<TopologyEdge>& physical_edges,
    const cv::Mat& normalized_source,
    const std::string& source_id,
    int page) const {

    GapInterpretationArtifacts result;

    std::unordered_map<std::string, std::size_t> node_index;
    std::unordered_map<std::string, std::vector<std::string>> incident_ids;

    node_index.reserve(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i)
        node_index.emplace(nodes[i].id, i);

    for (const auto& edge : physical_edges) {
        incident_ids[edge.from_node].push_back(edge.id);
        incident_ids[edge.to_node].push_back(edge.id);
    }

    std::vector<EndpointInfo> endpoints;
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (nodes[i].type == TopologyNodeType::ConductorEnd) {
            endpoints.push_back({
                i,
                static_cast<int>(incident_ids[nodes[i].id].size())});
        }
    }

    std::vector<bool> used(endpoints.size(), false);

    for (std::size_t i = 0; i < endpoints.size(); ++i) {
        if (used[i] || endpoints[i].degree != 1)
            continue;

        const auto& a = nodes[endpoints[i].node_index];

        std::string a_edge_id = incident_ids[a.id].front();
        const auto edge_it = std::find_if(
            physical_edges.begin(), physical_edges.end(),
            [&](const TopologyEdge& edge) {
                return edge.id == a_edge_id;
            });
        if (edge_it == physical_edges.end())
            continue;

        const std::string a_neighbor_id =
            edge_it->from_node == a.id
                ? edge_it->to_node
                : edge_it->from_node;

        const auto a_neighbor_it = node_index.find(a_neighbor_id);
        if (a_neighbor_it == node_index.end())
            continue;

        const auto& a_neighbor = nodes[a_neighbor_it->second];

        std::size_t best = endpoints.size();
        double best_distance = config_.maximum_gap + 1.0;

        for (std::size_t j = i + 1; j < endpoints.size(); ++j) {
            if (used[j] || endpoints[j].degree != 1)
                continue;

            const auto& b = nodes[endpoints[j].node_index];
            const double gap = distance(a.position, b.position);

            if (gap < config_.minimum_gap ||
                gap > config_.maximum_gap ||
                gap >= best_distance)
                continue;

            std::string b_edge_id = incident_ids[b.id].front();
            const auto b_edge_it = std::find_if(
                physical_edges.begin(), physical_edges.end(),
                [&](const TopologyEdge& edge) {
                    return edge.id == b_edge_id;
                });
            if (b_edge_it == physical_edges.end())
                continue;

            const std::string b_neighbor_id =
                b_edge_it->from_node == b.id
                    ? b_edge_it->to_node
                    : b_edge_it->from_node;

            const auto b_neighbor_it = node_index.find(b_neighbor_id);
            if (b_neighbor_it == node_index.end())
                continue;

            const auto& b_neighbor = nodes[b_neighbor_it->second];

            if (!collinear_facing(
                    a, b, &a_neighbor, &b_neighbor,
                    config_.collinear_tolerance))
                continue;

            const double density =
                gap_ink_density(normalized_source, a.position, b.position);

            // An annotation occupying the gap is evidence for a visual
            // interruption. An empty gap is deliberately not bridged here;
            // that case requires separate missing-geometry reasoning.
            if (density < config_.minimum_ink_density)
                continue;

            best = j;
            best_distance = gap;
        }

        if (best == endpoints.size())
            continue;

        const auto& b = nodes[endpoints[best].node_index];

        std::ostringstream canonical;
        canonical << source_id << ":" << page << ":"
                  << a.id << ":" << b.id;

        TopologyEdge inferred;
        inferred.id = stable_id(
            "inferred-continuation-edge", canonical.str());
        inferred.from_node = a.id;
        inferred.to_node = b.id;
        inferred.conductor_segment = "";

        result.inferred_edges.push_back(std::move(inferred));

        nodes[endpoints[i].node_index].type =
            TopologyNodeType::Continuation;
        nodes[endpoints[best].node_index].type =
            TopologyNodeType::Continuation;

        used[i] = true;
        used[best] = true;
    }

    std::sort(
        result.inferred_edges.begin(),
        result.inferred_edges.end(),
        [](const TopologyEdge& a, const TopologyEdge& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

#include "eke_dx_wire/topology/endpoint_reconstructor.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace eke::dx::wire {
namespace {

constexpr int kLocalRadius = 10;
constexpr int kForwardStart = 4;
constexpr int kForwardEnd = 14;
constexpr int kForwardHalfWidth = 4;
constexpr int kTransverseHalfWidth = 7;

bool dark_pixel(const cv::Mat& image, int x, int y) {
    if (x < 0 || y < 0 || x >= image.cols || y >= image.rows)
        return false;

    return image.at<std::uint8_t>(y, x) < 180;
}

void sample_rect(
    const cv::Mat& image,
    double cx,
    double cy,
    int x0,
    int y0,
    int x1,
    int y1,
    int& dark,
    int& total) {

    x0 = (std::max)(0, x0);
    y0 = (std::max)(0, y0);
    x1 = (std::min)(image.cols - 1, x1);
    y1 = (std::min)(image.rows - 1, y1);

    if (x0 > x1 || y0 > y1)
        return;

    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            ++total;
            if (dark_pixel(image, x, y))
                ++dark;
        }
    }

    (void)cx;
    (void)cy;
}

EndpointEvidence measure_evidence(
    const EndpointCandidate& candidate,
    const TopologyNode& node,
    const TopologyNode* adjacent,
    const cv::Mat& image) {

    EndpointEvidence evidence;
    if (image.empty() || image.type() != CV_8UC1)
        return evidence;

    const int cx = static_cast<int>(std::lround(node.position.x));
    const int cy = static_cast<int>(std::lround(node.position.y));

    evidence.source_region = {
        cx - kLocalRadius,
        cy - kLocalRadius,
        2 * kLocalRadius + 1,
        2 * kLocalRadius + 1
    };

    sample_rect(
        image,
        node.position.x, node.position.y,
        cx - kLocalRadius, cy - kLocalRadius,
        cx + kLocalRadius, cy + kLocalRadius,
        evidence.local_ink_pixels,
        evidence.local_pixel_count);

    // Endpoint topology is currently orthogonal. The direction from the
    // adjacent node toward this node therefore identifies the outward side
    // of the endpoint. For non-orthogonal future inputs, fall back to local
    // evidence without inventing a direction.
    if (adjacent != nullptr) {
        const double dx = node.position.x - adjacent->position.x;
        const double dy = node.position.y - adjacent->position.y;

        if (std::abs(dx) >= std::abs(dy) && std::abs(dx) > 0.5) {
            const int sign = dx > 0.0 ? 1 : -1;
            sample_rect(
                image, node.position.x, node.position.y,
                cx + sign * kForwardStart,
                cy - kForwardHalfWidth,
                cx + sign * kForwardEnd,
                cy + kForwardHalfWidth,
                evidence.forward_ink_pixels,
                evidence.forward_pixel_count);

            sample_rect(
                image, node.position.x, node.position.y,
                cx - kTransverseHalfWidth,
                cy - 3,
                cx + kTransverseHalfWidth,
                cy + 3,
                evidence.transverse_ink_pixels,
                evidence.transverse_pixel_count);
        } else if (std::abs(dy) > 0.5) {
            const int sign = dy > 0.0 ? 1 : -1;
            sample_rect(
                image, node.position.x, node.position.y,
                cx - kForwardHalfWidth,
                cy + sign * kForwardStart,
                cx + kForwardHalfWidth,
                cy + sign * kForwardEnd,
                evidence.forward_ink_pixels,
                evidence.forward_pixel_count);

            sample_rect(
                image, node.position.x, node.position.y,
                cx - 3,
                cy - kTransverseHalfWidth,
                cx + 3,
                cy + kTransverseHalfWidth,
                evidence.transverse_ink_pixels,
                evidence.transverse_pixel_count);
        }
    }

    if (evidence.local_pixel_count > 0)
        evidence.local_ink_density =
            static_cast<double>(evidence.local_ink_pixels) /
            static_cast<double>(evidence.local_pixel_count);

    if (evidence.forward_pixel_count > 0)
        evidence.forward_ink_density =
            static_cast<double>(evidence.forward_ink_pixels) /
            static_cast<double>(evidence.forward_pixel_count);

    if (evidence.transverse_pixel_count > 0)
        evidence.transverse_ink_density =
            static_cast<double>(evidence.transverse_ink_pixels) /
            static_cast<double>(evidence.transverse_pixel_count);

    evidence.near_image_boundary =
        cx <= kLocalRadius ||
        cy <= kLocalRadius ||
        cx >= image.cols - 1 - kLocalRadius ||
        cy >= image.rows - 1 - kLocalRadius;

    (void)candidate;
    return evidence;
}

EndpointArtifacts reconstruct_impl(
    const std::vector<TopologyNode>& nodes,
    const std::vector<TopologyEdge>& edges,
    const cv::Mat& normalized_source,
    const std::string& source_id,
    int page) {

    EndpointArtifacts result;

    std::unordered_map<std::string, std::size_t> node_index;
    node_index.reserve(nodes.size());

    std::vector<std::vector<std::string>> incident(nodes.size());

    for (std::size_t i = 0; i < nodes.size(); ++i)
        node_index.emplace(nodes[i].id, i);

    for (const auto& edge : edges) {
        const auto from = node_index.find(edge.from_node);
        if (from != node_index.end())
            incident[from->second].push_back(edge.id);

        const auto to = node_index.find(edge.to_node);
        if (to != node_index.end())
            incident[to->second].push_back(edge.id);
    }

    for (std::size_t i = 0; i < nodes.size(); ++i) {
        const auto& node = nodes[i];

        if (node.type != TopologyNodeType::ConductorEnd ||
            incident[i].size() != 1) {
            continue;
        }

        const auto edge_it = std::find_if(
            edges.begin(), edges.end(),
            [&](const TopologyEdge& edge) {
                return edge.id == incident[i].front();
            });

        const TopologyNode* adjacent = nullptr;
        if (edge_it != edges.end()) {
            const std::string& adjacent_id =
                edge_it->from_node == node.id
                    ? edge_it->to_node
                    : edge_it->from_node;

            const auto adjacent_it = node_index.find(adjacent_id);
            if (adjacent_it != node_index.end())
                adjacent = &nodes[adjacent_it->second];
        }

        EndpointCandidate candidate;
        std::ostringstream canonical;
        canonical << source_id << ":" << page << ":" << node.id;
        candidate.id = stable_id("endpoint-candidate", canonical.str());
        candidate.node_id = node.id;
        candidate.position = node.position;
        candidate.kind = EndpointKind::GeometricConductorEnd;
        candidate.confidence = ConfidenceClass::Low;
        candidate.incident_edges = incident[i];

        if (!normalized_source.empty())
            candidate.evidence = measure_evidence(
                candidate, node, adjacent, normalized_source);

        result.candidates.push_back(std::move(candidate));
    }

    std::sort(
        result.candidates.begin(),
        result.candidates.end(),
        [](const EndpointCandidate& a, const EndpointCandidate& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace

EndpointArtifacts EndpointReconstructor::reconstruct(
    const std::vector<TopologyNode>& nodes,
    const std::vector<TopologyEdge>& edges,
    const std::string& source_id,
    int page) const {

    return reconstruct_impl(
        nodes, edges, cv::Mat {}, source_id, page);
}

EndpointArtifacts EndpointReconstructor::reconstruct(
    const std::vector<TopologyNode>& nodes,
    const std::vector<TopologyEdge>& edges,
    const cv::Mat& normalized_source,
    const std::string& source_id,
    int page) const {

    return reconstruct_impl(
        nodes, edges, normalized_source, source_id, page);
}

} // namespace eke::dx::wire

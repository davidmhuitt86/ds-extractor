#include "eke_dx_wire/topology/wire_reconstructor.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace eke::dx::wire {
namespace {

struct AdjacentEdge {
    std::string edge_id;
    std::string other_node;
};

bool is_terminal_candidate(const EndpointCandidate& endpoint) {
    if (endpoint.kind == EndpointKind::Splice) {
        return false;
    }
    return endpoint.kind != EndpointKind::Unresolved;
}

} // namespace

WireReconstructor::WireReconstructor(WireReconstructionConfig config)
    : config_(config) {}

WireReconstructionArtifacts WireReconstructor::reconstruct(
    const std::vector<TopologyNode>& nodes,
    const std::vector<TopologyEdge>& edges,
    const std::vector<EndpointCandidate>& endpoints,
    const std::vector<ConductorSegment>& conductors,
    const std::string& source_id,
    int page) const {

    WireReconstructionArtifacts result;

    std::unordered_map<std::string, std::size_t> node_index;
    node_index.reserve(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        node_index.emplace(nodes[i].id, i);
    }

    std::unordered_map<std::string, std::vector<AdjacentEdge>> adjacency;
    adjacency.reserve(nodes.size());

    std::unordered_map<std::string, const TopologyEdge*> edge_by_id;
    edge_by_id.reserve(edges.size());

    for (const auto& edge : edges) {
        edge_by_id.emplace(edge.id, &edge);
        adjacency[edge.from_node].push_back({edge.id, edge.to_node});
        adjacency[edge.to_node].push_back({edge.id, edge.from_node});
    }

    std::unordered_map<std::string, std::string> endpoint_by_node;
    endpoint_by_node.reserve(endpoints.size());
    for (const auto& endpoint : endpoints) {
        if (is_terminal_candidate(endpoint)) {
            endpoint_by_node.emplace(endpoint.node_id, endpoint.id);
        }
    }

    std::unordered_set<std::string> consumed_edges;

    auto node_is_distribution = [&](const std::string& node_id) {
        const auto it = node_index.find(node_id);
        if (it == node_index.end()) {
            return false;
        }

        return nodes[it->second].type == TopologyNodeType::Splice ||
               nodes[it->second].type == TopologyNodeType::Junction;
    };

    for (const auto& endpoint : endpoints) {
        if (!is_terminal_candidate(endpoint)) {
            continue;
        }

        const auto adjacency_it = adjacency.find(endpoint.node_id);
        if (adjacency_it == adjacency.end() || adjacency_it->second.size() != 1) {
            continue;
        }

        std::string current_node = endpoint.node_id;
        std::string previous_edge;
        std::vector<std::string> path_edges;
        std::unordered_set<std::string> visited_nodes;
        bool reached_endpoint = false;
        std::string other_endpoint;

        while (true) {
            if (!visited_nodes.insert(current_node).second) {
                break;
            }

            const auto terminal_it = endpoint_by_node.find(current_node);
            if (current_node != endpoint.node_id &&
                terminal_it != endpoint_by_node.end()) {
                reached_endpoint = true;
                other_endpoint = terminal_it->second;
                break;
            }

            const auto adj_it = adjacency.find(current_node);
            if (adj_it == adjacency.end()) {
                break;
            }

            const auto& incident = adj_it->second;

            // A true endpoint must have exactly one incident edge.
            // A continuation has exactly two. Any distribution node has
            // three or more and therefore requires a separate semantic
            // decomposition stage; do not invent a wire pairing here.
            if (current_node != endpoint.node_id &&
                incident.size() != 2) {
                if (node_is_distribution(current_node)) {
                    result.unresolved_nodes.push_back(current_node);
                }
                break;
            }

            const AdjacentEdge* next = nullptr;
            for (const auto& candidate : incident) {
                if (candidate.edge_id != previous_edge) {
                    next = &candidate;
                    break;
                }
            }

            if (next == nullptr) {
                break;
            }

            previous_edge = next->edge_id;
            path_edges.push_back(next->edge_id);
            current_node = next->other_node;
        }

        if (!reached_endpoint || other_endpoint.empty()) {
            continue;
        }

        // Emit each undirected endpoint-to-endpoint path once.
        if (endpoint.id > other_endpoint) {
            continue;
        }

        bool already_consumed = false;
        for (const auto& edge_id : path_edges) {
            if (consumed_edges.contains(edge_id)) {
                already_consumed = true;
                break;
            }
        }
        if (already_consumed) {
            continue;
        }

        Wire wire;
        wire.id = stable_id(
            "wire",
            source_id + ":" + std::to_string(page) + ":" +
            endpoint.id + ":" + other_endpoint);

        wire.start_endpoint = endpoint.id;
        wire.end_endpoint = other_endpoint;
        wire.topology_edges = path_edges;
        wire.confidence = ConfidenceClass::Medium;

        std::unordered_set<std::string> segment_seen;
        for (const auto& edge_id : path_edges) {
            const auto edge_it = edge_by_id.find(edge_id);
            if (edge_it == edge_by_id.end()) {
                continue;
            }

            const auto& segment_id = edge_it->second->conductor_segment;
            if (segment_id.empty() || !segment_seen.insert(segment_id).second) {
                continue;
            }

            wire.conductor_segments.push_back(segment_id);

            const auto segment_it = std::find_if(
                conductors.begin(), conductors.end(),
                [&](const ConductorSegment& segment) {
                    return segment.id == segment_id;
                });
            if (segment_it != conductors.end()) {
                wire.heavy_cable =
                    wire.heavy_cable || segment_it->heavy_cable;
            }
        }

        result.wires.push_back(std::move(wire));
        consumed_edges.insert(path_edges.begin(), path_edges.end());
    }

    std::sort(
        result.wires.begin(), result.wires.end(),
        [](const Wire& a, const Wire& b) {
            return a.id < b.id;
        });

    std::sort(result.unresolved_nodes.begin(), result.unresolved_nodes.end());
    result.unresolved_nodes.erase(
        std::unique(result.unresolved_nodes.begin(), result.unresolved_nodes.end()),
        result.unresolved_nodes.end());

    return result;
}

} // namespace eke::dx::wire

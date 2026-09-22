#include "eke_dx_wire/topology/distribution_decomposer.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace eke::dx::wire {
namespace {

struct Adjacent {
    std::string edge_id;
    std::string other_node;
};

bool semantic_endpoint(const EndpointCandidate& endpoint) {
    return endpoint.kind != EndpointKind::Unresolved &&
           endpoint.kind != EndpointKind::Splice;
}

bool anchor_endpoint(const EndpointCandidate& endpoint,
                     const DistributionDecompositionConfig& config) {
    if (endpoint.kind == EndpointKind::Ground) {
        return config.allow_ground_anchor;
    }
    if (endpoint.kind == EndpointKind::ExternalConnection) {
        return config.allow_external_anchor;
    }
    return false;
}

std::vector<std::string> unique_sorted(std::vector<std::string> values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

} // namespace

DistributionDecomposer::DistributionDecomposer(
    DistributionDecompositionConfig config)
    : config_(config) {}

DistributionDecompositionArtifacts DistributionDecomposer::decompose(
    const std::vector<TopologyNode>& nodes,
    const std::vector<TopologyEdge>& edges,
    const std::vector<EndpointCandidate>& endpoints,
    const std::vector<ConductorSegment>& conductors,
    const std::string& source_id,
    int page) const {

    DistributionDecompositionArtifacts result;

    std::unordered_map<std::string, const TopologyNode*> node_by_id;
    for (const auto& node : nodes) {
        node_by_id.emplace(node.id, &node);
    }

    std::unordered_map<std::string, const TopologyEdge*> edge_by_id;
    std::unordered_map<std::string, std::vector<Adjacent>> adjacency;
    for (const auto& edge : edges) {
        edge_by_id.emplace(edge.id, &edge);
        const auto from = node_by_id.find(edge.from_node);
        const auto to = node_by_id.find(edge.to_node);
        if (from == node_by_id.end() || to == node_by_id.end()) {
            continue;
        }
        if (!from->second->electrically_connective ||
            !to->second->electrically_connective) {
            continue;
        }
        adjacency[edge.from_node].push_back({edge.id, edge.to_node});
        adjacency[edge.to_node].push_back({edge.id, edge.from_node});
    }

    std::unordered_map<std::string, std::vector<std::string>> endpoint_ids;
    for (const auto& endpoint : endpoints) {
        if (!semantic_endpoint(endpoint)) {
            continue;
        }
        endpoint_ids[endpoint.node_id].push_back(endpoint.id);
    }

    std::unordered_set<std::string> visited_nodes;
    std::vector<std::string> component_starts;
    for (const auto& node : nodes) {
        if (!node.electrically_connective ||
            adjacency.find(node.id) == adjacency.end() ||
            visited_nodes.contains(node.id)) {
            continue;
        }
        component_starts.push_back(node.id);
    }
    std::sort(component_starts.begin(), component_starts.end());

    for (const auto& start : component_starts) {
        if (visited_nodes.contains(start)) {
            continue;
        }

        std::vector<std::string> component_nodes;
        std::vector<std::string> component_edges;
        std::deque<std::string> queue;
        queue.push_back(start);
        visited_nodes.insert(start);

        std::unordered_set<std::string> component_edge_set;

        while (!queue.empty()) {
            const auto current = queue.front();
            queue.pop_front();
            component_nodes.push_back(current);

            const auto adj_it = adjacency.find(current);
            if (adj_it == adjacency.end()) {
                continue;
            }

            for (const auto& adjacent : adj_it->second) {
                component_edge_set.insert(adjacent.edge_id);
                if (visited_nodes.insert(adjacent.other_node).second) {
                    queue.push_back(adjacent.other_node);
                }
            }
        }

        component_edges.assign(
            component_edge_set.begin(), component_edge_set.end());
        std::sort(component_nodes.begin(), component_nodes.end());
        std::sort(component_edges.begin(), component_edges.end());

        std::vector<std::string> component_endpoints;
        std::vector<std::string> splice_nodes;
        for (const auto& node_id : component_nodes) {
            const auto endpoint_it = endpoint_ids.find(node_id);
            if (endpoint_it != endpoint_ids.end()) {
                component_endpoints.insert(
                    component_endpoints.end(),
                    endpoint_it->second.begin(),
                    endpoint_it->second.end());
            }

            const auto node_it = node_by_id.find(node_id);
            if (node_it != node_by_id.end() &&
                node_it->second->type == TopologyNodeType::Splice) {
                splice_nodes.push_back(node_id);
            }
        }

        component_endpoints = unique_sorted(std::move(component_endpoints));
        splice_nodes = unique_sorted(std::move(splice_nodes));

        std::vector<const EndpointCandidate*> anchors;
        std::unordered_map<std::string, const EndpointCandidate*> endpoint_by_id;
        for (const auto& endpoint : endpoints) {
            endpoint_by_id.emplace(endpoint.id, &endpoint);
            if (std::binary_search(
                    component_endpoints.begin(),
                    component_endpoints.end(),
                    endpoint.id) &&
                anchor_endpoint(endpoint, config_)) {
                anchors.push_back(&endpoint);
            }
        }

        // Ordinary two-terminal wires do not form distribution nets merely
        // because they are connected. An anchored component is different:
        // a Ground or ExternalConnection endpoint gives the connected
        // component an explicit electrical-net identity even when there is
        // no splice. This allows a direct ground-to-terminal connection to
        // become a grounded net without inventing source selection for
        // unanchored components.
        if (component_endpoints.size() < 2 ||
            (splice_nodes.empty() && anchors.empty())) {
            continue;
        }

        ElectricalNet net;
        net.id = stable_id(
            "electrical-net",
            source_id + ":" + std::to_string(page) + ":" +
            (component_nodes.empty() ? std::string{} : component_nodes.front()));
        net.endpoint_ids = component_endpoints;
        net.splice_node_ids = splice_nodes;
        net.topology_edges = component_edges;
        net.confidence = ConfidenceClass::Unresolved;

        // A tree is required for deterministic source-to-load paths. A cycle
        // needs a separate circuit-analysis stage and is intentionally left
        // unresolved here.
        if (component_edges.size() + 1 != component_nodes.size()) {
            result.nets.push_back(std::move(net));
            continue;
        }

        if (anchors.size() != 1) {
            // No semantic basis exists for choosing which branch is the
            // source. Do not manufacture pairings simply to reduce count.
            result.nets.push_back(std::move(net));
            continue;
        }

        const auto* anchor = anchors.front();
        net.anchor_endpoint = anchor->id;
        net.role = anchor->kind == EndpointKind::Ground
            ? DistributionRole::Ground
            : DistributionRole::Unknown;
        net.confidence = ConfidenceClass::Medium;

        const auto anchor_node_it = endpoint_by_id.find(anchor->id);
        if (anchor_node_it == endpoint_by_id.end()) {
            result.nets.push_back(std::move(net));
            continue;
        }

        for (const auto& target_id : component_endpoints) {
            if (target_id == anchor->id) {
                continue;
            }

            const auto target_it = endpoint_by_id.find(target_id);
            if (target_it == endpoint_by_id.end()) {
                continue;
            }

            std::unordered_map<std::string, std::pair<std::string, std::string>>
                predecessor;
            std::deque<std::string> path_queue;
            std::unordered_set<std::string> path_seen;
            path_queue.push_back(anchor_node_it->second->node_id);
            path_seen.insert(anchor_node_it->second->node_id);

            while (!path_queue.empty()) {
                const auto current = path_queue.front();
                path_queue.pop_front();
                if (current == target_it->second->node_id) {
                    break;
                }

                auto incident = adjacency[current];
                std::sort(
                    incident.begin(), incident.end(),
                    [](const Adjacent& a, const Adjacent& b) {
                        return a.edge_id < b.edge_id;
                    });

                for (const auto& next : incident) {
                    if (path_seen.insert(next.other_node).second) {
                        predecessor[next.other_node] =
                            {current, next.edge_id};
                        path_queue.push_back(next.other_node);
                    }
                }
            }

            const auto target_node = target_it->second->node_id;
            if (!path_seen.contains(target_node)) {
                continue;
            }

            std::vector<std::string> path_edges;
            std::string current = target_node;
            while (current != anchor_node_it->second->node_id) {
                const auto pred = predecessor.find(current);
                if (pred == predecessor.end()) {
                    path_edges.clear();
                    break;
                }
                path_edges.push_back(pred->second.second);
                current = pred->second.first;
            }
            if (path_edges.empty()) {
                continue;
            }
            std::reverse(path_edges.begin(), path_edges.end());

            Wire wire;
            wire.id = stable_id(
                "wire",
                source_id + ":" + std::to_string(page) + ":" +
                anchor->id + ":" + target_id);
            wire.start_endpoint = anchor->id;
            wire.end_endpoint = target_id;
            wire.topology_edges = path_edges;
            wire.confidence = ConfidenceClass::Medium;

            std::set<std::string> segment_seen;
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

                for (const auto& conductor : conductors) {
                    if (conductor.id == segment_id) {
                        wire.heavy_cable =
                            wire.heavy_cable || conductor.heavy_cable;
                        break;
                    }
                }
            }

            result.wires.push_back(std::move(wire));
        }

        result.nets.push_back(std::move(net));
    }

    std::sort(
        result.nets.begin(), result.nets.end(),
        [](const ElectricalNet& a, const ElectricalNet& b) {
            return a.id < b.id;
        });
    std::sort(
        result.wires.begin(), result.wires.end(),
        [](const Wire& a, const Wire& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

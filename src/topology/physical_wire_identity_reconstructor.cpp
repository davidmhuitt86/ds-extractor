#include "eke_dx_wire/topology/physical_wire_identity_reconstructor.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace eke::dx::wire {
namespace {

struct AdjacentEdge {
    std::string edge_id;
    std::string other_node;
};

struct WalkOutcome {
    std::string other_endpoint_id;
    std::vector<std::string> path_edges;
    std::vector<std::string> distribution_segment_evidence_ids;
    // True when this outcome passed through at least one distribution/
    // crossing node where more than one other incident edge shared the
    // same ConductorSegment as the edge arrived on - a genuine evidence
    // contradiction (AP-WIRE-029/031: never pick a winner among equally
    // plausible physical continuations).
    bool conflicted = false;
};

// Recursively walks forward from current_node. At a Continuation node
// (exactly two incident edges) there is only one possible next edge, so
// no branching occurs. At a Splice/Junction/Crossing node (more than two
// incident edges), the walk may continue only via another incident edge
// that shares the same ConductorSegment as the edge just arrived on -
// the one form of physical-continuity evidence AP-WIRE-029/031 permit.
// If exactly one such edge exists, the walk continues unambiguously; if
// more than one exists, every one of them is walked independently (each
// producing its own outcome, all flagged conflicted) rather than picking
// one; if none exists, this branch of the walk simply ends with no
// outcome (insufficient evidence, not a conflict).
void walk_from(
    const std::string& current_node,
    const std::string& previous_edge,
    std::unordered_set<std::string> visited_nodes,
    std::vector<std::string> path_edges,
    std::vector<std::string> distribution_segment_evidence_ids,
    bool conflicted_so_far,
    const std::map<std::string, std::vector<AdjacentEdge>>& adjacency,
    const std::unordered_map<std::string, const TopologyEdge*>& edge_by_id,
    const std::unordered_map<std::string, std::string>& endpoint_by_node,
    std::set<std::pair<std::string, std::string>>& expanded_ambiguities,
    std::vector<WalkOutcome>& outcomes) {

    if (!visited_nodes.insert(current_node).second) {
        return; // cycle guard
    }

    const auto terminal_it = endpoint_by_node.find(current_node);
    if (terminal_it != endpoint_by_node.end()) {
        outcomes.push_back(WalkOutcome{
            terminal_it->second, std::move(path_edges),
            std::move(distribution_segment_evidence_ids), conflicted_so_far});
        return;
    }

    const auto adjacency_it = adjacency.find(current_node);
    if (adjacency_it == adjacency.end()) {
        return;
    }
    const auto& incident = adjacency_it->second;

    if (incident.size() == 2) {
        const AdjacentEdge* next = nullptr;
        for (const auto& candidate : incident) {
            if (candidate.edge_id != previous_edge) {
                next = &candidate;
                break;
            }
        }
        if (next == nullptr) {
            return;
        }
        auto next_path = path_edges;
        next_path.push_back(next->edge_id);
        walk_from(
            next->other_node, next->edge_id, visited_nodes,
            std::move(next_path), distribution_segment_evidence_ids,
            conflicted_so_far, adjacency, edge_by_id, endpoint_by_node,
            expanded_ambiguities, outcomes);
        return;
    }

    // Distribution node (Splice/Junction) or Crossing.
    const auto previous_edge_it = edge_by_id.find(previous_edge);
    if (previous_edge_it == edge_by_id.end() ||
        previous_edge_it->second->conductor_segment.empty()) {
        return;
    }
    const std::string target_segment = previous_edge_it->second->conductor_segment;

    std::vector<const AdjacentEdge*> matches;
    for (const auto& candidate : incident) {
        if (candidate.edge_id == previous_edge) {
            continue;
        }
        const auto candidate_edge_it = edge_by_id.find(candidate.edge_id);
        if (candidate_edge_it == edge_by_id.end()) {
            continue;
        }
        if (candidate_edge_it->second->conductor_segment == target_segment) {
            matches.push_back(&candidate);
        }
    }
    if (matches.empty()) {
        return; // no evidence this conductor continues here - stop
    }

    const bool branch_conflict = matches.size() > 1;
    if (branch_conflict) {
        // A physical Wire is endpoint-to-endpoint; this node/segment
        // ambiguity (>1 other edge sharing the arrived-on segment) is one
        // fork event, not one event per edge that happens to touch it.
        // Pass 2 seeds a separate walk from every eligible endpoint, so
        // the same fork can be entered from any of its tied edges - each
        // entry expands into a different (N-1)-sized subset of the tied
        // set, and the union across all entries over-counts the fork as
        // a full pairwise closure instead of the N-1 candidate identities
        // the evidence actually supports. Expanding it only on the first
        // arrival (deterministic: eligible endpoints are walked in sorted
        // id order) keeps the N-1 candidates and drops the rest, without
        // touching the unambiguous (matches.size() == 1) case at all -
        // shared conductor geometry elsewhere still supports as many
        // distinct Wire identities as the evidence shows.
        if (!expanded_ambiguities.emplace(current_node, target_segment).second) {
            return;
        }
    }
    for (const auto* match : matches) {
        auto next_path = path_edges;
        next_path.push_back(match->edge_id);
        auto next_evidence = distribution_segment_evidence_ids;
        next_evidence.push_back(target_segment);
        walk_from(
            match->other_node, match->edge_id, visited_nodes,
            std::move(next_path), std::move(next_evidence),
            conflicted_so_far || branch_conflict, adjacency, edge_by_id,
            endpoint_by_node, expanded_ambiguities, outcomes);
    }
}

std::vector<WalkOutcome> walk_forward(
    const EndpointCandidate& start,
    const std::map<std::string, std::vector<AdjacentEdge>>& adjacency,
    const std::unordered_map<std::string, const TopologyEdge*>& edge_by_id,
    const std::unordered_map<std::string, std::string>& endpoint_by_node,
    std::set<std::pair<std::string, std::string>>& expanded_ambiguities) {

    std::vector<WalkOutcome> outcomes;
    const auto adjacency_it = adjacency.find(start.node_id);
    if (adjacency_it == adjacency.end() || adjacency_it->second.size() != 1) {
        return outcomes;
    }
    const auto& first = adjacency_it->second.front();
    std::unordered_set<std::string> visited{start.node_id};
    walk_from(
        first.other_node, first.edge_id, visited,
        std::vector<std::string>{first.edge_id}, {}, false, adjacency,
        edge_by_id, endpoint_by_node, expanded_ambiguities, outcomes);
    return outcomes;
}

} // namespace

PhysicalWireIdentityArtifacts PhysicalWireIdentityReconstructor::reconstruct(
    const std::vector<TopologyNode>& nodes,
    const std::vector<TopologyEdge>& edges,
    const std::vector<EndpointCandidate>& endpoints,
    const std::vector<ConductorSegment>& conductors,
    const std::vector<ConductorBoundaryResolution>& boundary_resolutions,
    const std::string& source_id,
    int page) const {

    PhysicalWireIdentityArtifacts result;

    // ---- Pass 1: the existing, unchanged conservative reconstruction --
    WireReconstructor base_reconstructor;
    const WireReconstructionArtifacts base = base_reconstructor.reconstruct(
        nodes, edges, endpoints, conductors, source_id, page);
    result.unresolved_nodes = base.unresolved_nodes;

    std::map<std::string, const ConductorBoundaryResolution*> boundary_by_endpoint;
    for (const auto& resolution : boundary_resolutions) {
        boundary_by_endpoint.emplace(resolution.endpoint_id, &resolution);
    }

    std::unordered_set<std::string> claimed_by_base_wire;
    for (auto wire : base.wires) {
        wire.identity_status = WireIdentityStatus::Resolved;
        const auto start_it = boundary_by_endpoint.find(wire.start_endpoint);
        if (start_it != boundary_by_endpoint.end()) {
            wire.identity_evidence_ids.push_back(start_it->second->id);
        }
        const auto end_it = boundary_by_endpoint.find(wire.end_endpoint);
        if (end_it != boundary_by_endpoint.end()) {
            wire.identity_evidence_ids.push_back(end_it->second->id);
        }
        claimed_by_base_wire.insert(wire.start_endpoint);
        claimed_by_base_wire.insert(wire.end_endpoint);
        result.wires.push_back(std::move(wire));
    }

    // ---- Pass 2: extend through distribution/crossing nodes only where
    // explicit conductor-segment-sharing evidence justifies it ----------
    std::map<std::string, std::vector<AdjacentEdge>> adjacency;
    std::unordered_map<std::string, const TopologyEdge*> edge_by_id;
    for (const auto& edge : edges) {
        edge_by_id.emplace(edge.id, &edge);
        adjacency[edge.from_node].push_back({edge.id, edge.to_node});
        adjacency[edge.to_node].push_back({edge.id, edge.from_node});
    }
    for (auto& [node_id, incident] : adjacency) {
        std::sort(
            incident.begin(), incident.end(),
            [](const AdjacentEdge& a, const AdjacentEdge& b) {
                return a.edge_id < b.edge_id;
            });
    }

    auto has_resolved_boundary = [&](const std::string& endpoint_id) {
        const auto it = boundary_by_endpoint.find(endpoint_id);
        return it != boundary_by_endpoint.end() &&
            it->second->boundary_status == ConductorBoundaryStatus::Resolved;
    };

    // A "true endpoint" for this second pass: degree-1, not already
    // claimed by a base wire, and carrying an AP-WIRE-030 Resolved
    // boundary - a bare geometric conductor end is never a new Wire
    // boundary here (AP-WIRE-029 Sec 3).
    std::unordered_map<std::string, std::string> endpoint_by_node;
    std::vector<const EndpointCandidate*> eligible_endpoints;
    for (const auto& endpoint : endpoints) {
        if (claimed_by_base_wire.contains(endpoint.id)) {
            continue;
        }
        const auto adjacency_it = adjacency.find(endpoint.node_id);
        if (adjacency_it == adjacency.end() || adjacency_it->second.size() != 1) {
            continue;
        }
        if (!has_resolved_boundary(endpoint.id)) {
            continue;
        }
        endpoint_by_node.emplace(endpoint.node_id, endpoint.id);
        eligible_endpoints.push_back(&endpoint);
    }
    std::sort(
        eligible_endpoints.begin(), eligible_endpoints.end(),
        [](const EndpointCandidate* a, const EndpointCandidate* b) {
            return a->id < b->id;
        });

    struct Candidate {
        std::string a;
        std::string b;
        std::vector<std::string> path_edges;
        std::vector<std::string> distribution_segment_evidence_ids;
        bool conflicted = false;
    };
    std::vector<Candidate> candidates;
    std::set<std::pair<std::string, std::string>> seen_pairs;
    // Shared across every eligible endpoint's walk (in sorted, deterministic
    // order) so a distribution-node ambiguity already expanded from one
    // seed is not re-expanded from a different tied edge - see walk_from.
    std::set<std::pair<std::string, std::string>> expanded_ambiguities;

    for (const auto* endpoint : eligible_endpoints) {
        const auto outcomes = walk_forward(
            *endpoint, adjacency, edge_by_id, endpoint_by_node,
            expanded_ambiguities);
        for (const auto& outcome : outcomes) {
            if (outcome.other_endpoint_id.empty() ||
                outcome.other_endpoint_id == endpoint->id) {
                continue;
            }
            std::string a = endpoint->id;
            std::string b = outcome.other_endpoint_id;
            bool conflicted = outcome.conflicted;
            if (a > b) std::swap(a, b);
            const auto pair_key = std::make_pair(a, b);
            if (!seen_pairs.insert(pair_key).second) {
                // Discovered from the other direction already - OR the
                // conflicted flag in, never downgrade it.
                for (auto& existing : candidates) {
                    if (existing.a == a && existing.b == b) {
                        existing.conflicted = existing.conflicted || conflicted;
                    }
                }
                continue;
            }
            candidates.push_back(Candidate{
                a, b, outcome.path_edges,
                outcome.distribution_segment_evidence_ids, conflicted});
        }
    }

    std::unordered_map<std::string, const ConductorSegment*> conductor_by_id;
    for (const auto& segment : conductors) {
        conductor_by_id.emplace(segment.id, &segment);
    }

    for (const auto& candidate : candidates) {
        Wire wire;
        wire.id = stable_id(
            "wire",
            source_id + ":" + std::to_string(page) + ":" +
                candidate.a + ":" + candidate.b);
        wire.start_endpoint = candidate.a;
        wire.end_endpoint = candidate.b;
        wire.topology_edges = candidate.path_edges;
        wire.confidence = ConfidenceClass::Medium;

        std::unordered_set<std::string> segment_seen;
        for (const auto& edge_id : candidate.path_edges) {
            const auto edge_it = edge_by_id.find(edge_id);
            if (edge_it == edge_by_id.end()) continue;
            const auto& segment_id = edge_it->second->conductor_segment;
            if (segment_id.empty() || !segment_seen.insert(segment_id).second)
                continue;
            wire.conductor_segments.push_back(segment_id);
            const auto segment_it = conductor_by_id.find(segment_id);
            if (segment_it != conductor_by_id.end()) {
                wire.heavy_cable = wire.heavy_cable || segment_it->second->heavy_cable;
            }
        }

        wire.identity_status = candidate.conflicted
            ? WireIdentityStatus::Conflicted
            : WireIdentityStatus::Resolved;

        const auto a_boundary_it = boundary_by_endpoint.find(candidate.a);
        if (a_boundary_it != boundary_by_endpoint.end())
            wire.identity_evidence_ids.push_back(a_boundary_it->second->id);
        const auto b_boundary_it = boundary_by_endpoint.find(candidate.b);
        if (b_boundary_it != boundary_by_endpoint.end())
            wire.identity_evidence_ids.push_back(b_boundary_it->second->id);
        for (const auto& segment_id : candidate.distribution_segment_evidence_ids) {
            wire.identity_evidence_ids.push_back(segment_id);
        }
        std::sort(wire.identity_evidence_ids.begin(), wire.identity_evidence_ids.end());
        wire.identity_evidence_ids.erase(
            std::unique(wire.identity_evidence_ids.begin(), wire.identity_evidence_ids.end()),
            wire.identity_evidence_ids.end());

        result.wires.push_back(std::move(wire));
    }

    std::sort(
        result.wires.begin(), result.wires.end(),
        [](const Wire& a, const Wire& b) { return a.id < b.id; });

    return result;
}

} // namespace eke::dx::wire

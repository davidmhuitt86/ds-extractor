#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"
#include "eke_dx_wire/topology/wire_reconstructor.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace eke::dx::wire {
namespace {

struct AdjacentEdge {
    std::string edge_id;
    std::string other_node;
};

struct WalkTrace {
    std::string start_endpoint_id;
    std::vector<std::string> terminal_splice_ids;
    std::vector<std::string> endpoint_outcomes;
    bool other_stop = false;
    std::size_t ambiguity_events = 0U;
};

struct ReplaySummary {
    std::size_t pass1_wires = 0U;
    std::size_t pass2_candidate_pairs = 0U;
    std::size_t unique_replay_pairs = 0U;
    std::size_t model_wires = 0U;
    std::size_t residual_endpoints = 0U;
    std::size_t residual_with_splice_stop = 0U;
    std::size_t residual_with_endpoint_outcome = 0U;
    std::size_t residual_with_other_stop = 0U;
    std::size_t terminal_stop_events = 0U;
    std::size_t unique_terminal_splices = 0U;
    std::size_t degree3_repeated = 0U;
    std::size_t degree4_repeated = 0U;
    std::size_t degree3_nonrepeated = 0U;
    std::size_t replay_missing_model_pairs = 0U;
    std::size_t replay_extra_model_pairs = 0U;
};

std::unordered_set<std::string> wire_endpoint_ids(
    const std::vector<Wire>& wires) {
    std::unordered_set<std::string> result;
    result.reserve(wires.size() * 2U);
    for (const auto& wire : wires) {
        result.insert(wire.start_endpoint);
        result.insert(wire.end_endpoint);
    }
    return result;
}

std::set<std::pair<std::string, std::string>> wire_pairs(
    const std::vector<Wire>& wires) {
    std::set<std::pair<std::string, std::string>> result;
    for (const auto& wire : wires) {
        std::string a = wire.start_endpoint;
        std::string b = wire.end_endpoint;
        if (a > b)
            std::swap(a, b);
        result.emplace(std::move(a), std::move(b));
    }
    return result;
}

struct ReplayContext {
    std::map<std::string, std::vector<AdjacentEdge>> adjacency;
    std::unordered_map<std::string, const TopologyEdge*> edge_by_id;
    std::unordered_map<std::string, const TopologyNode*> node_by_id;
    std::unordered_map<std::string, std::string> endpoint_by_node;
    std::unordered_set<std::string> distribution_nodes;
    std::set<std::pair<std::string, std::string>> expanded_ambiguities;
    std::vector<std::pair<std::string, std::string>> candidate_pairs;
    std::vector<WalkTrace> traces;
};

void walk_from_exact(
    const std::string& start_endpoint_id,
    const std::string& current_node,
    const std::string& previous_edge,
    std::unordered_set<std::string> visited_nodes,
    ReplayContext& context,
    WalkTrace& trace) {

    (void)start_endpoint_id;

    if (!visited_nodes.insert(current_node).second) {
        trace.other_stop = true;
        return;
    }

    const auto terminal_it = context.endpoint_by_node.find(current_node);
    if (terminal_it != context.endpoint_by_node.end()) {
        trace.endpoint_outcomes.push_back(terminal_it->second);
        return;
    }

    const auto adjacency_it = context.adjacency.find(current_node);
    if (adjacency_it == context.adjacency.end()) {
        trace.other_stop = true;
        return;
    }

    const auto& incident = adjacency_it->second;

    if (incident.size() == 2U) {
        const AdjacentEdge* next = nullptr;
        for (const auto& candidate : incident) {
            if (candidate.edge_id != previous_edge) {
                next = &candidate;
                break;
            }
        }
        if (next == nullptr) {
            trace.other_stop = true;
            return;
        }

        if (context.distribution_nodes.contains(current_node)) {
            const auto previous_it = context.edge_by_id.find(previous_edge);
            const auto next_it = context.edge_by_id.find(next->edge_id);
            if (previous_it == context.edge_by_id.end() ||
                next_it == context.edge_by_id.end() ||
                previous_it->second->conductor_segment.empty() ||
                previous_it->second->conductor_segment !=
                    next_it->second->conductor_segment) {
                const auto node_it = context.node_by_id.find(current_node);
                if (node_it != context.node_by_id.end() &&
                    node_it->second->type == TopologyNodeType::Splice) {
                    trace.terminal_splice_ids.push_back(current_node);
                } else {
                    trace.other_stop = true;
                }
                return;
            }
        }

        walk_from_exact(
            start_endpoint_id,
            next->other_node,
            next->edge_id,
            visited_nodes,
            context,
            trace);
        return;
    }

    const auto previous_edge_it = context.edge_by_id.find(previous_edge);
    if (previous_edge_it == context.edge_by_id.end() ||
        previous_edge_it->second->conductor_segment.empty()) {
        trace.other_stop = true;
        return;
    }

    const std::string target_segment =
        previous_edge_it->second->conductor_segment;

    std::vector<const AdjacentEdge*> matches;
    for (const auto& candidate : incident) {
        if (candidate.edge_id == previous_edge)
            continue;

        const auto candidate_it = context.edge_by_id.find(candidate.edge_id);
        if (candidate_it == context.edge_by_id.end())
            continue;

        if (candidate_it->second->conductor_segment == target_segment)
            matches.push_back(&candidate);
    }

    if (matches.empty()) {
        const auto node_it = context.node_by_id.find(current_node);
        if (node_it != context.node_by_id.end() &&
            node_it->second->type == TopologyNodeType::Splice) {
            trace.terminal_splice_ids.push_back(current_node);
        } else {
            trace.other_stop = true;
        }
        return;
    }

    const bool branch_conflict = matches.size() > 1U;
    if (branch_conflict) {
        ++trace.ambiguity_events;
        if (!context.expanded_ambiguities.emplace(
                current_node, target_segment).second) {
            return;
        }
    }

    for (const auto* match : matches) {
        walk_from_exact(
            start_endpoint_id,
            match->other_node,
            match->edge_id,
            visited_nodes,
            context,
            trace);
    }
}

std::set<std::pair<std::string, std::string>>
run_exact_replay(
    const WireModel& model,
    std::vector<WalkTrace>& traces,
    std::unordered_set<std::string>& residual_ids,
    std::size_t& pass1_count,
    std::size_t& pass2_count) {

    ReplayContext context;

    for (const auto& edge : model.edges) {
        context.edge_by_id.emplace(edge.id, &edge);
        context.adjacency[edge.from_node].push_back(
            {edge.id, edge.to_node});
        context.adjacency[edge.to_node].push_back(
            {edge.id, edge.from_node});
    }
    for (auto& [node_id, incident] : context.adjacency) {
        (void)node_id;
        std::sort(
            incident.begin(),
            incident.end(),
            [](const AdjacentEdge& a, const AdjacentEdge& b) {
                return a.edge_id < b.edge_id;
            });
    }

    context.node_by_id.reserve(model.nodes.size());
    for (const auto& node : model.nodes) {
        context.node_by_id.emplace(node.id, &node);
        if (node.type == TopologyNodeType::Splice ||
            node.type == TopologyNodeType::Junction) {
            context.distribution_nodes.insert(node.id);
        }
    }

    WireReconstructor base_reconstructor;
    const WireReconstructionArtifacts base =
        base_reconstructor.reconstruct(
            model.nodes,
            model.edges,
            model.endpoint_candidates,
            model.conductor_segments,
            model.source_id,
            model.page);

    pass1_count = base.wires.size();

    const auto claimed_by_base = wire_endpoint_ids(base.wires);

    std::vector<const EndpointCandidate*> eligible;
    for (const auto& endpoint : model.endpoint_candidates) {
        if (claimed_by_base.contains(endpoint.id))
            continue;

        const auto adjacency_it = context.adjacency.find(endpoint.node_id);
        if (adjacency_it == context.adjacency.end() ||
            adjacency_it->second.size() != 1U) {
            continue;
        }

        if (endpoint.kind == EndpointKind::Splice ||
            endpoint.kind == EndpointKind::Unresolved) {
            continue;
        }

        context.endpoint_by_node.emplace(endpoint.node_id, endpoint.id);
        eligible.push_back(&endpoint);
    }

    std::sort(
        eligible.begin(),
        eligible.end(),
        [](const EndpointCandidate* a, const EndpointCandidate* b) {
            return a->id < b->id;
        });

    context.candidate_pairs.clear();

    for (const auto* endpoint : eligible) {
        WalkTrace trace;
        trace.start_endpoint_id = endpoint->id;

        const auto adjacency_it = context.adjacency.find(endpoint->node_id);
        if (adjacency_it == context.adjacency.end() ||
            adjacency_it->second.size() != 1U) {
            traces.push_back(std::move(trace));
            continue;
        }

        const auto& first = adjacency_it->second.front();
        std::unordered_set<std::string> visited{endpoint->node_id};

        walk_from_exact(
            endpoint->id,
            first.other_node,
            first.edge_id,
            visited,
            context,
            trace);

        for (const auto& other : trace.endpoint_outcomes) {
            if (other.empty() || other == endpoint->id)
                continue;

            std::string a = endpoint->id;
            std::string b = other;
            if (a > b)
                std::swap(a, b);

            context.candidate_pairs.emplace_back(std::move(a), std::move(b));
        }

        traces.push_back(std::move(trace));
    }

    std::set<std::pair<std::string, std::string>> pass2_pairs;
    for (const auto& pair : context.candidate_pairs)
        pass2_pairs.insert(pair);

    pass2_count = pass2_pairs.size();

    // The replay population is the union of the unchanged Pass-1 WireModel
    // pairs and the newly reconstructed Pass-2 endpoint pairs. This must be
    // compared as one population; comparing Pass-2 alone would incorrectly
    // report every Pass-1 wire as a missing model pair.
    std::set<std::pair<std::string, std::string>> replay_pairs =
        wire_pairs(base.wires);
    replay_pairs.insert(pass2_pairs.begin(), pass2_pairs.end());

    // Re-derive the residual set exactly as PhysicalWireIdentityReconstructor:
    // final endpoint incidence is base wires plus the unique Pass-2 pairs.
    std::unordered_set<std::string> claimed;
    claimed.reserve(model.endpoint_candidates.size());
    for (const auto& wire : base.wires) {
        claimed.insert(wire.start_endpoint);
        claimed.insert(wire.end_endpoint);
    }
    for (const auto& pair : pass2_pairs) {
        claimed.insert(pair.first);
        claimed.insert(pair.second);
    }

    for (const auto& endpoint : model.endpoint_candidates) {
        if (!claimed.contains(endpoint.id))
            residual_ids.insert(endpoint.id);
    }

    return replay_pairs;
}

std::string stop_class_for_trace(
    const WalkTrace& trace) {

    if (!trace.endpoint_outcomes.empty())
        return "endpoint_outcome";

    if (!trace.terminal_splice_ids.empty())
        return "splice_stop";

    return "other_stop";
}

ReplaySummary summarize(
    const WireModel& model,
    const std::vector<WalkTrace>& traces,
    const std::unordered_set<std::string>& residual_ids,
    std::size_t pass1,
    std::size_t pass2,
    const std::set<std::pair<std::string, std::string>>& replay_pairs) {

    ReplaySummary result;
    result.pass1_wires = pass1;
    result.pass2_candidate_pairs = pass2;
    result.unique_replay_pairs = replay_pairs.size();
    result.model_wires = model.wires.size();
    result.residual_endpoints = residual_ids.size();

    std::set<std::string> terminal_splice_ids;
    std::map<std::string, const TopologyNode*> node_by_id;
    for (const auto& node : model.nodes)
        node_by_id.emplace(node.id, &node);

    for (const auto& trace : traces) {
        if (!residual_ids.contains(trace.start_endpoint_id))
            continue;

        if (!trace.terminal_splice_ids.empty())
            ++result.residual_with_splice_stop;
        else if (!trace.endpoint_outcomes.empty())
            ++result.residual_with_endpoint_outcome;
        else
            ++result.residual_with_other_stop;

        for (const auto& splice_id : trace.terminal_splice_ids) {
            terminal_splice_ids.insert(splice_id);
            ++result.terminal_stop_events;
        }
    }

    result.unique_terminal_splices = terminal_splice_ids.size();

    for (const auto& splice_id : terminal_splice_ids) {
        const auto it = node_by_id.find(splice_id);
        if (it == node_by_id.end())
            continue;

        const auto& node = *it->second;
        std::vector<std::string> segments;
        for (const auto& edge : model.edges) {
            if (edge.from_node == node.id || edge.to_node == node.id) {
                if (!edge.conductor_segment.empty())
                    segments.push_back(edge.conductor_segment);
            }
        }
        std::sort(segments.begin(), segments.end());

        bool repeated = false;
        for (std::size_t i = 1; i < segments.size(); ++i) {
            if (segments[i] == segments[i - 1]) {
                repeated = true;
                break;
            }
        }

        if (node.type == TopologyNodeType::Splice &&
            segments.size() == 4U &&
            repeated) {
            ++result.degree4_repeated;
        } else if (node.type == TopologyNodeType::Splice &&
                   segments.size() == 3U &&
                   repeated) {
            ++result.degree3_repeated;
        } else if (node.type == TopologyNodeType::Splice &&
                   segments.size() == 3U &&
                   !repeated) {
            ++result.degree3_nonrepeated;
        }
    }

    const auto model_pairs = wire_pairs(model.wires);
    result.replay_missing_model_pairs = 0U;
    for (const auto& pair : replay_pairs) {
        if (!model_pairs.contains(pair))
            ++result.replay_extra_model_pairs;
    }
    for (const auto& pair : model_pairs) {
        if (!replay_pairs.contains(pair))
            ++result.replay_missing_model_pairs;
    }

    return result;
}

void write_report(
    const std::filesystem::path& path,
    const WireModel& model,
    const ReplaySummary& summary,
    const std::vector<WalkTrace>& traces,
    const std::unordered_set<std::string>& residual_ids) {

    std::ofstream out(path);
    if (!out)
        throw std::runtime_error(
            "Unable to create AP-DIAG-042 report: " + path.string());

    out << "{\n"
        << "  \"schema_version\": 1,\n"
        << "  \"ap\": \"AP-DIAG-042\",\n"
        << "  \"status\": \"diagnostic_only\",\n"
        << "  \"production_logic_modified\": false,\n"
        << "  \"source\": {\n"
        << "    \"source_id\": \"" << model.source_id << "\",\n"
        << "    \"page\": " << model.page << ",\n"
        << "    \"width\": " << model.image_width << ",\n"
        << "    \"height\": " << model.image_height << "\n"
        << "  },\n"
        << "  \"pipeline\": {\n"
        << "    \"model_wires\": " << summary.model_wires << ",\n"
        << "    \"model_endpoints\": " << model.endpoint_candidates.size() << ",\n"
        << "    \"model_nodes\": " << model.nodes.size() << ",\n"
        << "    \"model_edges\": " << model.edges.size() << "\n"
        << "  },\n"
        << "  \"exact_replay\": {\n"
        << "    \"pass1_wires\": " << summary.pass1_wires << ",\n"
        << "    \"pass2_candidate_pairs\": " << summary.pass2_candidate_pairs << ",\n"
        << "    \"unique_replay_pairs\": " << summary.unique_replay_pairs << ",\n"
        << "    \"replay_missing_model_pairs\": "
        << summary.replay_missing_model_pairs << ",\n"
        << "    \"replay_extra_model_pairs\": "
        << summary.replay_extra_model_pairs << "\n"
        << "  },\n"
        << "  \"residual\": {\n"
        << "    \"zero_wire_endpoints\": " << summary.residual_endpoints << ",\n"
        << "    \"residual_with_splice_stop\": "
        << summary.residual_with_splice_stop << ",\n"
        << "    \"residual_with_endpoint_outcome\": "
        << summary.residual_with_endpoint_outcome << ",\n"
        << "    \"residual_with_other_stop\": "
        << summary.residual_with_other_stop << ",\n"
        << "    \"terminal_stop_events\": "
        << summary.terminal_stop_events << ",\n"
        << "    \"unique_terminal_splices\": "
        << summary.unique_terminal_splices << "\n"
        << "  },\n"
        << "  \"terminal_splice_structure\": {\n"
        << "    \"degree3_repeated\": " << summary.degree3_repeated << ",\n"
        << "    \"degree4_repeated\": " << summary.degree4_repeated << ",\n"
        << "    \"degree3_nonrepeated\": " << summary.degree3_nonrepeated << "\n"
        << "  },\n"
        << "  \"residual_endpoint_ids\": [\n";

    std::vector<std::string> ids(residual_ids.begin(), residual_ids.end());
    std::sort(ids.begin(), ids.end());
    for (std::size_t i = 0; i < ids.size(); ++i) {
        out << "    \"" << ids[i] << "\"";
        if (i + 1 != ids.size())
            out << ",";
        out << "\n";
    }

    out << "  ],\n"
        << "  \"trace_classification\": [\n";

    std::size_t written = 0U;
    for (const auto& trace : traces) {
        if (!residual_ids.contains(trace.start_endpoint_id))
            continue;

        if (written++ != 0U)
            out << ",\n";

        out << "    {\"endpoint_id\":\""
            << trace.start_endpoint_id
            << "\",\"class\":\""
            << stop_class_for_trace(trace)
            << "\",\"terminal_splices\":[";

        for (std::size_t i = 0; i < trace.terminal_splice_ids.size(); ++i) {
            if (i != 0U)
                out << ",";
            out << "\"" << trace.terminal_splice_ids[i] << "\"";
        }

        out << "],\"endpoint_outcomes\":[";
        for (std::size_t i = 0; i < trace.endpoint_outcomes.size(); ++i) {
            if (i != 0U)
                out << ",";
            out << "\"" << trace.endpoint_outcomes[i] << "\"";
        }
        out << "],\"ambiguity_events\":"
            << trace.ambiguity_events
            << "}";
    }

    out << "\n  ]\n}\n";
}

ReplaySummary run_once(
    const std::string& image_path,
    const std::filesystem::path& report_path) {

    ExtractionPipeline pipeline;
    const WireModel model = pipeline.run(image_path, image_path);

    std::vector<WalkTrace> traces;
    std::unordered_set<std::string> residual_ids;
    std::size_t pass1 = 0U;
    std::size_t pass2 = 0U;

    const auto replay_pairs = run_exact_replay(
        model, traces, residual_ids, pass1, pass2);

    const ReplaySummary summary = summarize(
        model,
        traces,
        residual_ids,
        pass1,
        pass2,
        replay_pairs);

    write_report(
        report_path,
        model,
        summary,
        traces,
        residual_ids);

    return summary;
}

} // namespace

int run_ap_diag_042(
    const std::string& image_path,
    const std::string& output_dir) {

    const std::filesystem::path output_path(output_dir);
    std::filesystem::create_directories(output_path);

    const ReplaySummary first = run_once(
        image_path,
        output_path / "AP-DIAG-042_exact_production_replay.json");

    std::cout
        << "[AP-DIAG-042] Model wires                  : "
        << first.model_wires << "\n"
        << "[AP-DIAG-042] Pass-1 WireReconstructor wires: "
        << first.pass1_wires << "\n"
        << "[AP-DIAG-042] Pass-2 unique candidate pairs  : "
        << first.pass2_candidate_pairs << "\n"
        << "[AP-DIAG-042] Replay pair total             : "
        << first.unique_replay_pairs << "\n"
        << "[AP-DIAG-042] Replay missing model pairs    : "
        << first.replay_missing_model_pairs << "\n"
        << "[AP-DIAG-042] Replay extra model pairs      : "
        << first.replay_extra_model_pairs << "\n"
        << "[AP-DIAG-042] Residual endpoints             : "
        << first.residual_endpoints << "\n"
        << "[AP-DIAG-042] Residual with splice stop      : "
        << first.residual_with_splice_stop << "\n"
        << "[AP-DIAG-042] Residual with endpoint outcome : "
        << first.residual_with_endpoint_outcome << "\n"
        << "[AP-DIAG-042] Residual with other stop      : "
        << first.residual_with_other_stop << "\n"
        << "[AP-DIAG-042] Terminal Splice stop events   : "
        << first.terminal_stop_events << "\n"
        << "[AP-DIAG-042] Unique terminal Splices       : "
        << first.unique_terminal_splices << "\n"
        << "[AP-DIAG-042] Terminal structure 3R/4R/3N  : "
        << first.degree3_repeated << "/"
        << first.degree4_repeated << "/"
        << first.degree3_nonrepeated << "\n"
        << "[AP-DIAG-042] Report: "
        << (output_path / "AP-DIAG-042_exact_production_replay.json").string()
        << "\n"
        << "[AP-DIAG-042] No production source was modified.\n";

    return 0;
}

} // namespace eke::dx::wire

int main(int argc, char** argv) {
    try {
        const std::string image_path =
            argc >= 2 ? argv[1] : "samples/trx300ODG.png";
        const std::string output_dir =
            argc >= 3 ? argv[2] : "artifacts/audit";

        return eke::dx::wire::run_ap_diag_042(image_path, output_dir);
    } catch (const std::exception& ex) {
        std::cerr << "[AP-DIAG-042] FAILED: " << ex.what() << "\n";
        return 1;
    }
}

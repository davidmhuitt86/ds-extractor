#include "eke_dx_wire/topology/wire_model_validator.hpp"

#include <algorithm>
#include <deque>
#include <unordered_map>
#include <unordered_set>

namespace eke::dx::wire {
namespace {

void issue(
    WireValidationReport& report,
    WireValidationSeverity severity,
    const std::string& code,
    const std::string& object_id,
    const std::string& detail) {

    report.issues.push_back({
        severity,
        code,
        object_id,
        detail
    });

    if (severity == WireValidationSeverity::Error) {
        report.valid = false;
    }
}

} // namespace

WireModelValidator::WireModelValidator(
    WireModelValidationConfig config)
    : config_(config) {}

WireValidationReport WireModelValidator::validate(
    const WireModel& model) const {

    WireValidationReport report;
    report.wires_checked = model.wires.size();
    report.electrical_nets_checked = model.electrical_nets.size();

    std::unordered_map<std::string, const EndpointCandidate*> endpoint_by_id;
    endpoint_by_id.reserve(model.endpoint_candidates.size());
    for (const auto& endpoint : model.endpoint_candidates) {
        endpoint_by_id.emplace(endpoint.id, &endpoint);
    }

    std::unordered_map<std::string, const TopologyNode*> node_by_id;
    node_by_id.reserve(model.nodes.size());
    for (const auto& node : model.nodes) {
        node_by_id.emplace(node.id, &node);
    }

    std::unordered_map<std::string, const TopologyEdge*> edge_by_id;
    edge_by_id.reserve(model.edges.size());
    for (const auto& edge : model.edges) {
        edge_by_id.emplace(edge.id, &edge);
    }

    std::unordered_map<std::string, const ConductorSegment*> conductor_by_id;
    conductor_by_id.reserve(model.conductor_segments.size());
    for (const auto& conductor : model.conductor_segments) {
        conductor_by_id.emplace(conductor.id, &conductor);
    }

    for (const auto& wire : model.wires) {
        bool wire_valid = true;

        const auto start_it = endpoint_by_id.find(wire.start_endpoint);
        const auto end_it = endpoint_by_id.find(wire.end_endpoint);

        if (start_it == endpoint_by_id.end()) {
            issue(
                report, WireValidationSeverity::Error,
                "WIRE-START-ENDPOINT-MISSING", wire.id,
                "start_endpoint does not reference an endpoint candidate");
            wire_valid = false;
        }

        if (end_it == endpoint_by_id.end()) {
            issue(
                report, WireValidationSeverity::Error,
                "WIRE-END-ENDPOINT-MISSING", wire.id,
                "end_endpoint does not reference an endpoint candidate");
            wire_valid = false;
        }

        if (wire.start_endpoint == wire.end_endpoint) {
            issue(
                report, WireValidationSeverity::Error,
                "WIRE-SELF-ENDPOINT", wire.id,
                "start_endpoint and end_endpoint are identical");
            wire_valid = false;
        }

        if (wire.topology_edges.empty()) {
            issue(
                report, WireValidationSeverity::Error,
                "WIRE-NO-TOPOLOGY", wire.id,
                "wire has no topology edges");
            wire_valid = false;
        }

        std::unordered_map<std::string, int> node_degree;
        std::unordered_map<std::string, std::vector<std::string>> adjacency;
        std::unordered_set<std::string> seen_edges;
        std::unordered_set<std::string> referenced_segments;

        for (const auto& edge_id : wire.topology_edges) {
            if (!seen_edges.insert(edge_id).second) {
                issue(
                    report, WireValidationSeverity::Error,
                    "WIRE-DUPLICATE-EDGE", wire.id,
                    "wire contains the same topology edge more than once");
                wire_valid = false;
                continue;
            }

            const auto edge_it = edge_by_id.find(edge_id);
            if (edge_it == edge_by_id.end()) {
                issue(
                    report, WireValidationSeverity::Error,
                    "WIRE-EDGE-MISSING", wire.id,
                    "wire references a topology edge that does not exist");
                wire_valid = false;
                continue;
            }

            const auto& edge = *edge_it->second;
            if (!node_by_id.contains(edge.from_node) ||
                !node_by_id.contains(edge.to_node)) {
                issue(
                    report, WireValidationSeverity::Error,
                    "WIRE-EDGE-NODE-MISSING", wire.id,
                    "topology edge references a missing topology node");
                wire_valid = false;
                continue;
            }

            ++node_degree[edge.from_node];
            ++node_degree[edge.to_node];
            adjacency[edge.from_node].push_back(edge.to_node);
            adjacency[edge.to_node].push_back(edge.from_node);

            if (!edge.conductor_segment.empty()) {
                referenced_segments.insert(edge.conductor_segment);
                if (!conductor_by_id.contains(edge.conductor_segment)) {
                    issue(
                        report, WireValidationSeverity::Error,
                        "WIRE-CONDUCTOR-MISSING", wire.id,
                        "topology edge references a missing conductor segment");
                    wire_valid = false;
                }
            }
        }

        for (const auto& segment_id : wire.conductor_segments) {
            if (!conductor_by_id.contains(segment_id)) {
                issue(
                    report, WireValidationSeverity::Error,
                    "WIRE-SEGMENT-MISSING", wire.id,
                    "wire references a missing conductor segment");
                wire_valid = false;
            }
        }

        for (const auto& segment_id : referenced_segments) {
            if (std::find(
                    wire.conductor_segments.begin(),
                    wire.conductor_segments.end(),
                    segment_id) == wire.conductor_segments.end()) {
                issue(
                    report, WireValidationSeverity::Error,
                    "WIRE-SEGMENT-OMITTED", wire.id,
                    "wire topology references a conductor segment absent from conductor_segments");
                wire_valid = false;
            }
        }

        if (start_it != endpoint_by_id.end() &&
            end_it != endpoint_by_id.end()) {

            const std::string start_node = start_it->second->node_id;
            const std::string end_node = end_it->second->node_id;

            if (node_by_id.contains(start_node) &&
                node_by_id.contains(end_node)) {

                std::vector<std::string> degree_one;
                for (const auto& [node_id, degree] : node_degree) {
                    if (degree == 1) {
                        degree_one.push_back(node_id);
                    } else if (degree != 2) {
                        issue(
                            report, WireValidationSeverity::Error,
                            "WIRE-NONPATH-DEGREE", wire.id,
                            "wire topology contains an internal node whose degree is not two");
                        wire_valid = false;
                    }
                }

                std::sort(degree_one.begin(), degree_one.end());

                std::vector<std::string> expected{
                    start_node, end_node};
                std::sort(expected.begin(), expected.end());

                if (degree_one != expected) {
                    issue(
                        report, WireValidationSeverity::Error,
                        "WIRE-ENDPOINT-DEGREE-MISMATCH", wire.id,
                        "wire topology path endpoints do not match the wire endpoint candidates");
                    wire_valid = false;
                }

                // A valid wire path must be a single connected component.
                if (!node_degree.empty()) {
                    std::unordered_set<std::string> visited;
                    std::deque<std::string> queue;
                    queue.push_back(start_node);
                    visited.insert(start_node);

                    while (!queue.empty()) {
                        const auto current = queue.front();
                        queue.pop_front();

                        const auto adj_it = adjacency.find(current);
                        if (adj_it == adjacency.end()) {
                            continue;
                        }

                        for (const auto& next : adj_it->second) {
                            if (visited.insert(next).second) {
                                queue.push_back(next);
                            }
                        }
                    }

                    if (visited.size() != node_degree.size()) {
                        issue(
                            report, WireValidationSeverity::Error,
                            "WIRE-DISCONNECTED-TOPOLOGY", wire.id,
                            "wire topology edges do not form one connected path");
                        wire_valid = false;
                    }
                }
            }
        }

        bool heavy_from_segments = false;
        for (const auto& segment_id : wire.conductor_segments) {
            const auto it = conductor_by_id.find(segment_id);
            if (it != conductor_by_id.end()) {
                heavy_from_segments =
                    heavy_from_segments || it->second->heavy_cable;
            }
        }

        if (wire.heavy_cable != heavy_from_segments) {
            issue(
                report, WireValidationSeverity::Error,
                "WIRE-HEAVY-CABLE-MISMATCH", wire.id,
                "wire heavy_cable does not agree with its referenced conductor segments");
            wire_valid = false;
        }

        if (config_.warn_on_nonsemantic_endpoints &&
            start_it != endpoint_by_id.end() &&
            end_it != endpoint_by_id.end() &&
            start_it->second->kind == EndpointKind::GeometricConductorEnd &&
            end_it->second->kind == EndpointKind::GeometricConductorEnd) {
            issue(
                report, WireValidationSeverity::Warning,
                "WIRE-GEOMETRIC-ENDPOINTS",
                wire.id,
                "wire endpoints are geometric candidates; semantic terminal classification is not yet established");
        }

        if (wire_valid) {
            ++report.valid_wires;
        }
    }

    for (const auto& net : model.electrical_nets) {
        for (const auto& endpoint_id : net.endpoint_ids) {
            if (!endpoint_by_id.contains(endpoint_id)) {
                issue(
                    report, WireValidationSeverity::Error,
                    "NET-ENDPOINT-MISSING", net.id,
                    "electrical net references a missing endpoint candidate");
            }
        }

        for (const auto& node_id : net.splice_node_ids) {
            if (!node_by_id.contains(node_id)) {
                issue(
                    report, WireValidationSeverity::Error,
                    "NET-SPLICE-MISSING", net.id,
                    "electrical net references a missing splice node");
            }
        }

        for (const auto& edge_id : net.topology_edges) {
            if (!edge_by_id.contains(edge_id)) {
                issue(
                    report, WireValidationSeverity::Error,
                    "NET-EDGE-MISSING", net.id,
                    "electrical net references a missing topology edge");
            }
        }

        if (!net.anchor_endpoint.empty() &&
            std::find(
                net.endpoint_ids.begin(),
                net.endpoint_ids.end(),
                net.anchor_endpoint) == net.endpoint_ids.end()) {
            issue(
                report, WireValidationSeverity::Error,
                "NET-ANCHOR-MISSING", net.id,
                "electrical net anchor_endpoint is not a member of endpoint_ids");
        }

        if (config_.warn_on_unresolved_nets &&
            net.role == DistributionRole::Unknown) {
            issue(
                report, WireValidationSeverity::Warning,
                "NET-ROLE-UNRESOLVED", net.id,
                "electrical net has no resolved circuit role");
        }
    }

    std::sort(
        report.issues.begin(),
        report.issues.end(),
        [](const WireValidationIssue& a, const WireValidationIssue& b) {
            if (a.severity != b.severity) {
                return static_cast<int>(a.severity) <
                       static_cast<int>(b.severity);
            }
            if (a.object_id != b.object_id) {
                return a.object_id < b.object_id;
            }
            return a.code < b.code;
        });

    return report;
}

} // namespace eke::dx::wire

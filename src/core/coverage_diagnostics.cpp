#include "eke_dx_wire/core/coverage_diagnostics.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace eke::dx::wire {
namespace {

void record(
    CoverageReport& report,
    std::string category,
    std::string code,
    std::string object_id,
    std::vector<std::string> related_object_ids,
    CoverageSeverity severity,
    std::string detail) {

    report.findings.push_back({
        std::move(category),
        std::move(code),
        std::move(object_id),
        std::move(related_object_ids),
        severity,
        std::move(detail)
    });
}

void sort_related(std::vector<std::string>& ids) {
    std::sort(ids.begin(), ids.end());
}

} // namespace

ConductorOwnershipClass classify_conductor_ownership(
    std::size_t topology_edge_references,
    std::size_t wire_references) {

    if (topology_edge_references == 0 && wire_references == 0) {
        return ConductorOwnershipClass::Unreferenced;
    }
    if (wire_references == 0) {
        return ConductorOwnershipClass::TopologyOnly;
    }
    if (topology_edge_references == 0) {
        return ConductorOwnershipClass::WireOnly;
    }
    if (wire_references > 1) {
        return ConductorOwnershipClass::Shared;
    }
    return ConductorOwnershipClass::Normal;
}

EndpointWireCoverageClass classify_endpoint_wire_coverage(
    std::size_t wire_references) {

    if (wire_references == 0) {
        return EndpointWireCoverageClass::ZeroWireEndpoint;
    }
    if (wire_references == 1) {
        return EndpointWireCoverageClass::SingleWireEndpoint;
    }
    return EndpointWireCoverageClass::MultipleWireEndpoint;
}

CoverageReport build_coverage_report(const WireModel& model) {
    CoverageReport report;

    // ------------------------------------------------------------------
    // 5.1 Conductor segment ownership
    // ------------------------------------------------------------------
    std::unordered_map<std::string, std::size_t> conductor_topology_refs;
    std::unordered_map<std::string, std::size_t> conductor_wire_refs;
    std::unordered_map<std::string, std::vector<std::string>> conductor_wire_owners;
    for (const auto& segment : model.conductor_segments) {
        conductor_topology_refs.emplace(segment.id, 0);
        conductor_wire_refs.emplace(segment.id, 0);
    }
    for (const auto& edge : model.edges) {
        if (edge.conductor_segment.empty()) continue;
        auto it = conductor_topology_refs.find(edge.conductor_segment);
        if (it != conductor_topology_refs.end()) {
            ++it->second;
        }
    }
    for (const auto& wire : model.wires) {
        std::unordered_set<std::string> unique_segments(
            wire.conductor_segments.begin(), wire.conductor_segments.end());
        for (const auto& segment_id : unique_segments) {
            auto it = conductor_wire_refs.find(segment_id);
            if (it != conductor_wire_refs.end()) {
                ++it->second;
                conductor_wire_owners[segment_id].push_back(wire.id);
            }
        }
    }

    report.conductors.total = model.conductor_segments.size();
    for (const auto& segment : model.conductor_segments) {
        const std::size_t topo_refs = conductor_topology_refs[segment.id];
        const std::size_t wire_refs = conductor_wire_refs[segment.id];
        const ConductorOwnershipClass ownership_class =
            classify_conductor_ownership(topo_refs, wire_refs);

        switch (ownership_class) {
        case ConductorOwnershipClass::Unreferenced:
            ++report.conductors.unreferenced;
            record(report, "conductor", "CONDUCTOR-UNREFERENCED", segment.id,
                   {}, CoverageSeverity::Warning,
                   "conductor segment is referenced by zero topology edges and zero wires");
            break;
        case ConductorOwnershipClass::TopologyOnly:
            ++report.conductors.topology_only;
            record(report, "conductor", "CONDUCTOR-TOPOLOGY-ONLY", segment.id,
                   {}, CoverageSeverity::Notice,
                   "conductor segment is part of topology but no wire references it yet");
            break;
        case ConductorOwnershipClass::WireOnly:
            ++report.conductors.wire_only;
            record(report, "conductor", "CONDUCTOR-WIRE-ONLY", segment.id,
                   {}, CoverageSeverity::Warning,
                   "conductor segment is referenced by a wire but not by any topology edge");
            break;
        case ConductorOwnershipClass::Normal:
            ++report.conductors.normal;
            break;
        case ConductorOwnershipClass::Shared: {
            ++report.conductors.shared;
            auto owners = conductor_wire_owners[segment.id];
            sort_related(owners);
            record(report, "conductor", "CONDUCTOR-SHARED", segment.id,
                   owners, CoverageSeverity::Notice,
                   "conductor segment is legitimately traversed by more than one endpoint-to-endpoint wire");
            break;
        }
        }
    }

    // ------------------------------------------------------------------
    // 5.2 Endpoint wire coverage
    // ------------------------------------------------------------------
    std::unordered_map<std::string, std::vector<std::string>> endpoint_wire_owners;
    for (const auto& endpoint : model.endpoint_candidates) {
        endpoint_wire_owners.emplace(endpoint.id, std::vector<std::string>{});
    }
    for (const auto& wire : model.wires) {
        if (endpoint_wire_owners.count(wire.start_endpoint)) {
            endpoint_wire_owners[wire.start_endpoint].push_back(wire.id);
        }
        if (wire.end_endpoint != wire.start_endpoint &&
            endpoint_wire_owners.count(wire.end_endpoint)) {
            endpoint_wire_owners[wire.end_endpoint].push_back(wire.id);
        }
    }

    report.endpoints.total = model.endpoint_candidates.size();
    for (const auto& endpoint : model.endpoint_candidates) {
        auto owners = endpoint_wire_owners[endpoint.id];
        const auto coverage_class = classify_endpoint_wire_coverage(owners.size());
        sort_related(owners);
        switch (coverage_class) {
        case EndpointWireCoverageClass::ZeroWireEndpoint:
            ++report.endpoints.zero_wire;
            record(report, "endpoint", "ENDPOINT-ZERO-WIRE", endpoint.id,
                   {}, CoverageSeverity::Notice,
                   "endpoint candidate is not referenced by any reconstructed wire");
            break;
        case EndpointWireCoverageClass::SingleWireEndpoint:
            ++report.endpoints.single_wire;
            break;
        case EndpointWireCoverageClass::MultipleWireEndpoint:
            ++report.endpoints.multiple_wire;
            record(report, "endpoint", "ENDPOINT-MULTIPLE-WIRE", endpoint.id,
                   owners, CoverageSeverity::Notice,
                   "endpoint candidate is the terminus of more than one endpoint-to-endpoint wire");
            break;
        }
    }

    // ------------------------------------------------------------------
    // 5.3 Wire termination integrity (leverages WireModelValidator output;
    // this diagnostic does not re-derive validation, only summarizes it
    // per-wire and surfaces which wire IDs failed).
    // ------------------------------------------------------------------
    std::unordered_set<std::string> wires_with_errors;
    for (const auto& issue : model.wire_validation.issues) {
        if (issue.severity == WireValidationSeverity::Error) {
            wires_with_errors.insert(issue.object_id);
        }
    }
    report.wires.total = model.wires.size();
    for (const auto& wire : model.wires) {
        if (wires_with_errors.count(wire.id)) {
            ++report.wires.invalid;
            record(report, "wire", "WIRE-INTEGRITY-FAILED", wire.id,
                   {}, CoverageSeverity::Warning,
                   "wire has at least one WireModelValidator error; see extraction_audit for the specific issue code");
        } else {
            ++report.wires.valid;
        }
    }

    // ------------------------------------------------------------------
    // 5.4 Topology edge ownership
    // ------------------------------------------------------------------
    std::unordered_map<std::string, const ConductorSegment*> conductor_by_id;
    for (const auto& segment : model.conductor_segments) {
        conductor_by_id.emplace(segment.id, &segment);
    }
    std::unordered_map<std::string, const TopologyNode*> node_by_id;
    for (const auto& node : model.nodes) {
        node_by_id.emplace(node.id, &node);
    }
    std::unordered_map<std::string, std::vector<std::string>> edge_wire_owners;
    for (const auto& wire : model.wires) {
        for (const auto& edge_id : wire.topology_edges) {
            edge_wire_owners[edge_id].push_back(wire.id);
        }
    }

    report.topology_edges.total = model.edges.size();
    for (const auto& edge : model.edges) {
        bool anomalous = false;
        if (!edge.conductor_segment.empty() &&
            !conductor_by_id.count(edge.conductor_segment)) {
            ++report.topology_edges.missing_conductor;
            anomalous = true;
        }
        if (!node_by_id.count(edge.from_node)) {
            ++report.topology_edges.missing_from_node;
            anomalous = true;
        }
        if (!node_by_id.count(edge.to_node)) {
            ++report.topology_edges.missing_to_node;
            anomalous = true;
        }
        if (anomalous) {
            record(report, "topology_edge", "TOPOLOGY-EDGE-DANGLING-REFERENCE",
                   edge.id, {}, CoverageSeverity::Warning,
                   "topology edge references a missing conductor segment or node");
        }
        if (!edge_wire_owners.count(edge.id)) {
            ++report.topology_edges.unowned_by_any_wire;
            record(report, "topology_edge", "TOPOLOGY-EDGE-UNOWNED", edge.id,
                   {}, CoverageSeverity::Notice,
                   "topology edge is not yet part of any reconstructed wire");
        }
    }

    // ------------------------------------------------------------------
    // 5.5 Topology node coverage
    // ------------------------------------------------------------------
    std::unordered_map<std::string, std::size_t> node_degree;
    for (const auto& node : model.nodes) {
        node_degree.emplace(node.id, 0);
    }
    for (const auto& edge : model.edges) {
        if (node_degree.count(edge.from_node)) ++node_degree[edge.from_node];
        if (node_degree.count(edge.to_node)) ++node_degree[edge.to_node];
    }
    std::unordered_set<std::string> nodes_with_endpoint;
    for (const auto& endpoint : model.endpoint_candidates) {
        if (!endpoint.node_id.empty()) {
            nodes_with_endpoint.insert(endpoint.node_id);
        }
    }

    report.topology_nodes.total = model.nodes.size();
    for (const auto& node : model.nodes) {
        const std::size_t degree = node_degree[node.id];
        if (degree == 0) {
            ++report.topology_nodes.zero_degree;
            record(report, "topology_node", "TOPOLOGY-NODE-ZERO-DEGREE",
                   node.id, {}, CoverageSeverity::Warning,
                   "topology node has no incident edges");
        }
        if (node.type == TopologyNodeType::Splice && degree < 2) {
            ++report.topology_nodes.low_degree_splice;
            record(report, "topology_node", "TOPOLOGY-NODE-LOW-DEGREE-SPLICE",
                   node.id, {}, CoverageSeverity::Warning,
                   "node is classified as a splice but has fewer than two incident edges");
        }
        if (node.type == TopologyNodeType::ConductorEnd &&
            !nodes_with_endpoint.count(node.id)) {
            ++report.topology_nodes.conductor_end_without_endpoint;
            record(report, "topology_node",
                   "TOPOLOGY-NODE-CONDUCTOR-END-WITHOUT-ENDPOINT", node.id,
                   {}, CoverageSeverity::Warning,
                   "node is classified as a conductor end but no endpoint candidate references it");
        }
    }

    // ------------------------------------------------------------------
    // 5.6 Component terminal coverage
    // ------------------------------------------------------------------
    std::unordered_map<std::string, std::size_t> component_terminal_counts;
    for (const auto& terminal : model.terminal_candidates) {
        if (!terminal.component_candidate_id.empty()) {
            ++component_terminal_counts[terminal.component_candidate_id];
        }
    }
    std::unordered_map<std::string, std::size_t> component_endpoint_counts;
    for (const auto& endpoint : model.endpoint_candidates) {
        if (!endpoint.component_id.empty()) {
            ++component_endpoint_counts[endpoint.component_id];
        }
    }

    report.components.total = model.component_candidates.size();
    for (const auto& component : model.component_candidates) {
        const bool furniture =
            component.kind == ComponentCandidateKind::DiagramFurniture;
        const std::size_t terminal_evidence =
            (component_terminal_counts.count(component.id) ? component_terminal_counts[component.id] : 0) +
            (component_endpoint_counts.count(component.id) ? component_endpoint_counts[component.id] : 0);

        if (furniture) {
            ++report.components.diagram_furniture;
            if (terminal_evidence == 0) {
                ++report.components.furniture_without_terminal_evidence;
            }
            continue;
        }

        ++report.components.real_candidates;
        if (terminal_evidence == 0) {
            ++report.components.real_without_terminal_evidence;
            record(report, "component", "COMPONENT-NO-TERMINAL-EVIDENCE",
                   component.id, {}, CoverageSeverity::Warning,
                   "non-furniture component candidate has no associated terminal or endpoint evidence");
        } else {
            ++report.components.real_with_terminal_evidence;
        }
    }

    // ------------------------------------------------------------------
    // 5.7 Connector coverage
    // ------------------------------------------------------------------
    std::unordered_map<std::string, const ComponentCandidate*> component_by_id;
    for (const auto& component : model.component_candidates) {
        component_by_id.emplace(component.id, &component);
    }
    std::unordered_map<std::string, std::size_t> connector_terminal_counts;
    for (const auto& terminal : model.connector_terminals) {
        ++connector_terminal_counts[terminal.connector_id];
    }

    report.connectors.total = model.connector_candidates.size();
    report.connectors.terminals_total = model.connector_terminals.size();
    for (const auto& connector : model.connector_candidates) {
        const std::size_t terminals =
            connector_terminal_counts.count(connector.id)
                ? connector_terminal_counts[connector.id] : 0;
        const auto component_it = component_by_id.find(connector.component_candidate_id);
        const bool furniture_derived =
            component_it != component_by_id.end() &&
            component_it->second->kind == ComponentCandidateKind::DiagramFurniture;

        if (furniture_derived) {
            ++report.connectors.furniture_derived;
            record(report, "connector", "CONNECTOR-FURNITURE-DERIVED",
                   connector.id, {connector.component_candidate_id},
                   CoverageSeverity::Warning,
                   "connector candidate is anchored to a component classified as diagram furniture");
        } else if (component_it == component_by_id.end() || terminals == 0) {
            ++report.connectors.unresolved;
            record(report, "connector", "CONNECTOR-UNRESOLVED", connector.id,
                   {}, CoverageSeverity::Warning,
                   "connector candidate has no resolvable component association or no terminals");
        } else {
            ++report.connectors.genuine_looking;
        }
    }

    // ------------------------------------------------------------------
    // 5.8 Electrical net coverage (endpoint membership only; reference
    // integrity for endpoint/splice/edge/anchor IDs is already validated
    // deterministically by WireModelValidator's NET-* error codes).
    // ------------------------------------------------------------------
    std::unordered_map<std::string, std::vector<std::string>> endpoint_net_owners;
    for (const auto& endpoint : model.endpoint_candidates) {
        endpoint_net_owners.emplace(endpoint.id, std::vector<std::string>{});
    }
    for (const auto& net : model.electrical_nets) {
        for (const auto& endpoint_id : net.endpoint_ids) {
            if (endpoint_net_owners.count(endpoint_id)) {
                endpoint_net_owners[endpoint_id].push_back(net.id);
            }
        }
    }

    report.electrical_nets.total = model.electrical_nets.size();
    for (const auto& [endpoint_id, nets] : endpoint_net_owners) {
        if (nets.empty()) {
            ++report.electrical_nets.endpoints_not_in_any_net;
            record(report, "electrical_net", "NET-ENDPOINT-UNOWNED",
                   endpoint_id, {}, CoverageSeverity::Notice,
                   "endpoint candidate does not belong to any electrical net yet");
            continue;
        }
        ++report.electrical_nets.endpoints_in_nets_total;
        if (nets.size() > 1) {
            ++report.electrical_nets.endpoints_in_multiple_nets;
            auto owners = nets;
            sort_related(owners);
            record(report, "electrical_net", "NET-ENDPOINT-MULTI-OWNED",
                   endpoint_id, owners, CoverageSeverity::Warning,
                   "endpoint candidate belongs to more than one electrical net, which should be deterministically exclusive");
        }
    }

    std::sort(
        report.findings.begin(),
        report.findings.end(),
        [](const CoverageDiagnosticRecord& a, const CoverageDiagnosticRecord& b) {
            if (a.category != b.category) return a.category < b.category;
            if (a.code != b.code) return a.code < b.code;
            return a.object_id < b.object_id;
        });

    return report;
}

} // namespace eke::dx::wire

#include "eke_dx_wire/export/extraction_audit_exporter.hpp"
#include "eke_dx_wire/core/coverage_diagnostics.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;
namespace eke::dx::wire {
namespace {

std::string esc(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '\\': r += "\\\\"; break;
        case '"': r += "\\\""; break;
        case '\n': r += "\\n"; break;
        case '\r': r += "\\r"; break;
        case '\t': r += "\\t"; break;
        default: r += c; break;
        }
    }
    return r;
}
const char* conf(ConfidenceClass v) {
    switch (v) {
    case ConfidenceClass::High: return "high";
    case ConfidenceClass::Medium: return "medium";
    case ConfidenceClass::Low: return "low";
    case ConfidenceClass::Unresolved: return "unresolved";
    }
    return "unresolved";
}
const char* endpoint_kind(EndpointKind v) {
    switch (v) {
    case EndpointKind::GeometricConductorEnd: return "geometric";
    case EndpointKind::ComponentTerminal: return "component_terminal";
    case EndpointKind::ConnectorTerminal: return "connector_terminal";
    case EndpointKind::Splice: return "splice";
    case EndpointKind::Ground: return "ground";
    case EndpointKind::ExternalConnection: return "external_connection";
    case EndpointKind::Unresolved: return "unresolved";
    }
    return "unresolved";
}
const char* terminal_role(TerminalRole v) {
    switch (v) {
    case TerminalRole::Unknown: return "unknown";
    case TerminalRole::ComponentTerminal: return "component_terminal";
    case TerminalRole::ConnectorTerminal: return "connector_terminal";
    case TerminalRole::GroundTerminal: return "ground_terminal";
    case TerminalRole::PowerSource: return "power_source";
    case TerminalRole::ExternalConnection: return "external_connection";
    }
    return "unknown";
}
const char* identity(WireIdentityStatus v) {
    switch (v) {
    case WireIdentityStatus::Resolved: return "resolved";
    case WireIdentityStatus::Unresolved: return "unresolved";
    case WireIdentityStatus::Conflicted: return "conflicted";
    }
    return "unresolved";
}
const char* node_type(TopologyNodeType v) {
    switch (v) {
    case TopologyNodeType::ConductorEnd: return "conductor_end";
    case TopologyNodeType::Continuation: return "continuation";
    case TopologyNodeType::Junction: return "junction";
    case TopologyNodeType::Splice: return "splice";
    case TopologyNodeType::Crossing: return "crossing";
    case TopologyNodeType::ComponentBoundary: return "component_boundary";
    case TopologyNodeType::Unresolved: return "unresolved";
    }
    return "unresolved";
}
const char* role(DistributionRole v) {
    switch (v) {
    case DistributionRole::Ground: return "ground";
    case DistributionRole::PowerFeed: return "power_feed";
    case DistributionRole::SharedFunctionFeed: return "shared_function_feed";
    case DistributionRole::Unknown: return "unknown";
    }
    return "unknown";
}
const char* component_kind(ComponentCandidateKind v) {
    switch (v) {
    case ComponentCandidateKind::Enclosure: return "enclosure";
    case ComponentCandidateKind::CircularSymbol: return "circular_symbol";
    case ComponentCandidateKind::ChassisGround: return "chassis_ground";
    case ComponentCandidateKind::PrimitiveSymbol: return "primitive_symbol";
    case ComponentCandidateKind::DiagramFurniture: return "diagram_furniture";
    case ComponentCandidateKind::Unknown: return "unknown";
    }
    return "unknown";
}
const char* terminal_candidate_kind(TerminalCandidateKind v) {
    switch (v) {
    case TerminalCandidateKind::ComponentBoundary: return "component_boundary";
    case TerminalCandidateKind::ConnectorBoundary: return "connector_boundary";
    case TerminalCandidateKind::GroundConnection: return "ground_connection";
    case TerminalCandidateKind::Unknown: return "unknown";
    }
    return "unknown";
}

const char* connector_topology(ConnectorTopologyKind v) {
    switch (v) {
    case ConnectorTopologyKind::Inline: return "inline";
    case ConnectorTopologyKind::ComponentAttached: return "component_attached";
    case ConnectorTopologyKind::Unknown: return "unknown";
    }
    return "unknown";
}
const char* connector_status(ConnectorTerminalStatus v) {
    switch (v) {
    case ConnectorTerminalStatus::Resolved: return "resolved";
    case ConnectorTerminalStatus::Unresolved: return "unresolved";
    case ConnectorTerminalStatus::Conflicted: return "conflicted";
    }
    return "unresolved";
}

template <typename T>
void sort_id(std::vector<T>& v) {
    std::stable_sort(v.begin(), v.end(),
        [](const T& a, const T& b) { return a.id < b.id; });
}
void strings(std::ostream& out, const std::vector<std::string>& v) {
    out << "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) out << ",";
        out << "\"" << esc(v[i]) << "\"";
    }
    out << "]";
}
void findings(std::ostream& out, const CoverageReport& c) {
    out << "[";
    for (std::size_t i = 0; i < c.findings.size(); ++i) {
        const auto& f = c.findings[i];
        if (i) out << ",";
        out << "{\"category\":\"" << esc(f.category)
            << "\",\"code\":\"" << esc(f.code)
            << "\",\"object_id\":\"" << esc(f.object_id)
            << "\",\"severity\":\""
            << (f.severity == CoverageSeverity::Warning ? "warning" : "notice")
            << "\",\"detail\":\"" << esc(f.detail) << "\",\"related_object_ids\":";
        strings(out, f.related_object_ids);
        out << "}";
    }
    out << "]";
}

} // namespace

void ExtractionAuditExporter::export_json(
    const WireModel& model, const fs::path& output_path) {

    std::ofstream out(output_path);
    if (!out)
        throw std::runtime_error("Unable to create extraction audit: " + output_path.string());

    auto components = model.component_candidates;
    auto terminals = model.terminal_candidates;
    auto connectors = model.connector_candidates;
    auto connector_terminals = model.connector_terminals;
    auto segments = model.conductor_segments;
    auto endpoints = model.endpoint_candidates;
    auto wires = model.wires;
    auto nodes = model.nodes;
    auto edges = model.edges;
    auto nets = model.electrical_nets;
    sort_id(components); sort_id(terminals); sort_id(connectors);
    sort_id(connector_terminals); sort_id(segments); sort_id(endpoints);
    sort_id(wires); sort_id(nodes); sort_id(edges); sort_id(nets);

    const CoverageReport c = build_coverage_report(model);

    out << "{\n"
        << "  \"schema_version\": 2,\n"
        << "  \"source\": {\"source_id\":\"" << esc(model.source_id)
        << "\",\"page\":" << model.page
        << ",\"image_width\":" << model.image_width
        << ",\"image_height\":" << model.image_height << "},\n"
        << "  \"run_identity\": {\"kind\":\"wire_model_extraction\",\"source_id\":\""
        << esc(model.source_id) << "\",\"page\":" << model.page << "},\n"
        << "  \"counts\": {"
        << "\"components\":" << components.size()
        << ",\"terminal_candidates\":" << terminals.size()
        << ",\"connectors\":" << connectors.size()
        << ",\"connector_terminals\":" << connector_terminals.size()
        << ",\"conductor_segments\":" << segments.size()
        << ",\"endpoint_candidates\":" << endpoints.size()
        << ",\"wires\":" << wires.size()
        << ",\"topology_nodes\":" << nodes.size()
        << ",\"topology_edges\":" << edges.size()
        << ",\"electrical_nets\":" << nets.size() << "},\n";

    out << "  \"objects\": {\n";

    out << "    \"components\":[";
    for (std::size_t i=0;i<components.size();++i) {
        const auto& v=components[i]; if(i) out<<",";
        out<<"{\"id\":\""<<esc(v.id)<<"\",\"kind\":\""<<component_kind(v.kind)
           <<"\",\"confidence\":\""<<conf(v.confidence)<<"\",\"x\":"<<v.bounds.x
           <<",\"y\":"<<v.bounds.y<<",\"width\":"<<v.bounds.width
           <<",\"height\":"<<v.bounds.height<<",\"semantic_labels\":";
        strings(out,v.semantic_labels); out<<"}";
    }
    out << "],\n";

    out << "    \"terminal_candidates\":[";
    for (std::size_t i=0;i<terminals.size();++i) {
        const auto& v=terminals[i]; if(i) out<<",";
        out<<"{\"id\":\""<<esc(v.id)<<"\",\"endpoint_id\":\""<<esc(v.endpoint_id)
           <<"\",\"component_candidate_id\":\""<<esc(v.component_candidate_id)
           << "\",\"kind\":\"" << terminal_candidate_kind(v.kind) << "\""
           <<",\"x\":"<<v.position.x<<",\"y\":"<<v.position.y
           <<",\"confidence\":\""<<conf(v.confidence)<<"\"}";
    }
    out << "],\n";

    out << "    \"connectors\":[";
    for (std::size_t i=0;i<connectors.size();++i) {
        const auto& v=connectors[i]; if(i) out<<",";
        out<<"{\"id\":\""<<esc(v.id)<<"\",\"component_candidate_id\":\""
           <<esc(v.component_candidate_id)<<"\",\"attached_component_candidate_id\":\""
           <<esc(v.attached_component_candidate_id)<<"\",\"topology\":\""
           <<connector_topology(v.topology)<<"\",\"confidence\":\""<<conf(v.confidence)
           <<"\",\"x\":"<<v.bounds.x<<",\"y\":"<<v.bounds.y
           <<",\"width\":"<<v.bounds.width<<",\"height\":"<<v.bounds.height
           <<",\"semantic_labels\":";
        strings(out,v.semantic_labels); out<<"}";
    }
    out << "],\n";

    out << "    \"connector_terminals\":[";
    for (std::size_t i=0;i<connector_terminals.size();++i) {
        const auto& v=connector_terminals[i]; if(i) out<<",";
        out<<"{\"id\":\""<<esc(v.id)<<"\",\"connector_id\":\""<<esc(v.connector_id)
           <<"\",\"endpoint_id\":\""<<esc(v.endpoint_id)<<"\",\"x\":"<<v.position.x
           <<",\"y\":"<<v.position.y<<",\"terminal_name\":\""<<esc(v.terminal_name)
           <<"\",\"function_label\":\""<<esc(v.function_label)
           <<"\",\"wire_color\":\""<<esc(v.wire_color)<<"\",\"role\":\""
           <<terminal_role(v.role)<<"\",\"confidence\":\""<<conf(v.confidence)
           <<"\",\"status\":\""<<connector_status(v.status)<<"\"}";
    }
    out << "],\n";

    out << "    \"conductor_segments\":[";
    for (std::size_t i=0;i<segments.size();++i) {
        const auto& v=segments[i]; if(i) out<<",";
        out<<"{\"id\":\""<<esc(v.id)<<"\",\"x1\":"<<v.geometry.a.x
           <<",\"y1\":"<<v.geometry.a.y<<",\"x2\":"<<v.geometry.b.x
           <<",\"y2\":"<<v.geometry.b.y<<",\"thickness_px\":"<<v.thickness_px
           <<",\"confidence\":\""<<conf(v.confidence)<<"\",\"heavy_cable\":"
           <<(v.heavy_cable?"true":"false")<<",\"provenance_stage\":\""
           <<esc(v.provenance.stage)<<"\"}";
    }
    out << "],\n";

    out << "    \"endpoint_candidates\":[";
    for (std::size_t i=0;i<endpoints.size();++i) {
        const auto& v=endpoints[i]; if(i) out<<",";
        out<<"{\"id\":\""<<esc(v.id)<<"\",\"node_id\":\""<<esc(v.node_id)
           <<"\",\"x\":"<<v.position.x<<",\"y\":"<<v.position.y
           <<",\"kind\":\""<<endpoint_kind(v.kind)<<"\",\"terminal_role\":\""
           <<terminal_role(v.terminal_role)<<"\",\"confidence\":\""<<conf(v.confidence)
           <<"\",\"component_id\":\""<<esc(v.component_id)
           <<"\",\"terminal_name\":\""<<esc(v.terminal_name)
           <<"\",\"function_label\":\""<<esc(v.function_label)
           <<"\",\"wire_color\":\""<<esc(v.wire_color)<<"\",\"incident_edges\":";
        strings(out,v.incident_edges); out<<"}";
    }
    out << "],\n";

    out << "    \"wires\":[";
    for (std::size_t i=0;i<wires.size();++i) {
        const auto& v=wires[i]; if(i) out<<",";
        out<<"{\"id\":\""<<esc(v.id)<<"\",\"start_endpoint\":\""<<esc(v.start_endpoint)
           <<"\",\"end_endpoint\":\""<<esc(v.end_endpoint)
           <<"\",\"identity_status\":\""<<identity(v.identity_status)
           <<"\",\"confidence\":\""<<conf(v.confidence)<<"\",\"heavy_cable\":"
           <<(v.heavy_cable?"true":"false")<<",\"topology_edges\":";
        strings(out,v.topology_edges); out<<",\"conductor_segments\":";
        strings(out,v.conductor_segments); out<<",\"identity_evidence_ids\":";
        strings(out,v.identity_evidence_ids); out<<"}";
    }
    out << "],\n";

    out << "    \"topology_nodes\":[";
    for (std::size_t i=0;i<nodes.size();++i) {
        const auto& v=nodes[i]; if(i) out<<",";
        out<<"{\"id\":\""<<esc(v.id)<<"\",\"x\":"<<v.position.x
           <<",\"y\":"<<v.position.y<<",\"type\":\""<<node_type(v.type)
           <<"\",\"electrically_connective\":"<<(v.electrically_connective?"true":"false")<<"}";
    }
    out << "],\n";

    out << "    \"topology_edges\":[";
    for (std::size_t i=0;i<edges.size();++i) {
        const auto& v=edges[i]; if(i) out<<",";
        out<<"{\"id\":\""<<esc(v.id)<<"\",\"from_node\":\""<<esc(v.from_node)
           <<"\",\"to_node\":\""<<esc(v.to_node)
           <<"\",\"conductor_segment\":\""<<esc(v.conductor_segment)<<"\"}";
    }
    out << "],\n";

    out << "    \"electrical_nets\":[";
    for (std::size_t i=0;i<nets.size();++i) {
        const auto& v=nets[i]; if(i) out<<",";
        out<<"{\"id\":\""<<esc(v.id)<<"\",\"role\":\""<<role(v.role)
           <<"\",\"confidence\":\""<<conf(v.confidence)
           <<"\",\"anchor_endpoint\":\""<<esc(v.anchor_endpoint)
           <<"\",\"endpoint_ids\":";
        strings(out,v.endpoint_ids); out<<",\"splice_node_ids\":";
        strings(out,v.splice_node_ids); out<<",\"topology_edges\":";
        strings(out,v.topology_edges); out<<"}";
    }
    out << "]\n  },\n";

    out << "  \"validation\":{\"valid\":"<<(model.wire_validation.valid?"true":"false")
        <<",\"errors\":"<<model.audit.validation_errors
        <<",\"warnings\":"<<model.audit.validation_warnings<<",\"issues\":[";
    for(std::size_t i=0;i<model.wire_validation.issues.size();++i){
        const auto& v=model.wire_validation.issues[i]; if(i) out<<",";
        out<<"{\"severity\":\""<<(v.severity==WireValidationSeverity::Error?"error":"warning")
           <<"\",\"code\":\""<<esc(v.code)<<"\",\"object_id\":\""<<esc(v.object_id)
           <<"\",\"detail\":\""<<esc(v.detail)<<"\"}";
    }
    out << "]},\n";

    out << "  \"coverage\":{"
        << "\"conductor_segments\":{\"total\":"<<c.conductors.total
        <<",\"unreferenced\":"<<c.conductors.unreferenced
        <<",\"topology_only\":"<<c.conductors.topology_only
        <<",\"wire_only\":"<<c.conductors.wire_only
        <<",\"normal\":"<<c.conductors.normal
        <<",\"shared\":"<<c.conductors.shared<<"},"
        << "\"endpoints\":{\"total\":"<<c.endpoints.total
        <<",\"zero_wire\":"<<c.endpoints.zero_wire
        <<",\"single_wire\":"<<c.endpoints.single_wire
        <<",\"multiple_wire\":"<<c.endpoints.multiple_wire<<"},"
        << "\"wires\":{\"total\":"<<c.wires.total
        <<",\"valid\":"<<c.wires.valid
        <<",\"invalid\":"<<c.wires.invalid<<"},"
        << "\"topology_edges\":{\"total\":"<<c.topology_edges.total
        <<",\"missing_conductor\":"<<c.topology_edges.missing_conductor
        <<",\"missing_from_node\":"<<c.topology_edges.missing_from_node
        <<",\"missing_to_node\":"<<c.topology_edges.missing_to_node
        <<",\"unowned_by_any_wire\":"<<c.topology_edges.unowned_by_any_wire<<"},"
        << "\"topology_nodes\":{\"total\":"<<c.topology_nodes.total
        <<",\"zero_degree\":"<<c.topology_nodes.zero_degree
        <<",\"low_degree_splice\":"<<c.topology_nodes.low_degree_splice
        <<",\"conductor_end_without_endpoint\":"<<c.topology_nodes.conductor_end_without_endpoint<<"},"
        << "\"components\":{\"total\":"<<c.components.total
        <<",\"diagram_furniture\":"<<c.components.diagram_furniture
        <<",\"real_candidates\":"<<c.components.real_candidates
        <<",\"real_with_terminal_evidence\":"<<c.components.real_with_terminal_evidence
        <<",\"real_without_terminal_evidence\":"<<c.components.real_without_terminal_evidence
        <<",\"furniture_without_terminal_evidence\":"<<c.components.furniture_without_terminal_evidence<<"},"
        << "\"connectors\":{\"total\":"<<c.connectors.total
        <<",\"terminals_total\":"<<c.connectors.terminals_total
        <<",\"genuine_looking\":"<<c.connectors.genuine_looking
        <<",\"furniture_derived\":"<<c.connectors.furniture_derived
        <<",\"unresolved\":"<<c.connectors.unresolved<<"},"
        << "\"electrical_nets\":{\"total\":"<<c.electrical_nets.total
        <<",\"endpoints_in_nets_total\":"<<c.electrical_nets.endpoints_in_nets_total
        <<",\"endpoints_not_in_any_net\":"<<c.electrical_nets.endpoints_not_in_any_net
        <<",\"endpoints_in_multiple_nets\":"<<c.electrical_nets.endpoints_in_multiple_nets<<"},"
        << "\"findings\":";
    findings(out,c);
    out << "}\n}\n";
}

} // namespace eke::dx::wire

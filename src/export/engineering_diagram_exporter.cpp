#include "eke_dx_wire/export/engineering_diagram_exporter.hpp"

#include <fstream>
#include <stdexcept>

namespace eke::dx::wire {
namespace {

std::string json_escape(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += ch; break;
        }
    }
    return result;
}

const char* status_name(DiagramObjectStatus status) {
    switch (status) {
    case DiagramObjectStatus::Resolved: return "resolved";
    case DiagramObjectStatus::Unresolved: return "unresolved";
    case DiagramObjectStatus::Conflicted: return "conflicted";
    }
    return "unresolved";
}

const char* confidence_name(ConfidenceClass value) {
    switch (value) {
    case ConfidenceClass::High: return "high";
    case ConfidenceClass::Medium: return "medium";
    case ConfidenceClass::Low: return "low";
    case ConfidenceClass::Unresolved: return "unresolved";
    }
    return "unresolved";
}

const char* component_kind_name(ComponentCandidateKind kind) {
    switch (kind) {
    case ComponentCandidateKind::Enclosure: return "enclosure";
    case ComponentCandidateKind::CircularSymbol: return "circular_symbol";
    case ComponentCandidateKind::ChassisGround: return "chassis_ground";
    case ComponentCandidateKind::PrimitiveSymbol: return "primitive_symbol";
    case ComponentCandidateKind::DiagramFurniture: return "diagram_furniture";
    case ComponentCandidateKind::Unknown: return "unknown";
    }
    return "unknown";
}

const char* node_type_name(TopologyNodeType type) {
    switch (type) {
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

const char* text_semantic_kind_name(TextSemanticKind kind) {
    switch (kind) {
    case TextSemanticKind::GroundLabel: return "ground_label";
    case TextSemanticKind::PowerFeedLabel: return "power_feed_label";
    case TextSemanticKind::SharedFunctionFeedLabel: return "shared_function_feed_label";
    case TextSemanticKind::ComponentLabel: return "component_label";
    case TextSemanticKind::ConnectorLabel: return "connector_label";
    case TextSemanticKind::TerminalLabel: return "terminal_label";
    case TextSemanticKind::WireColorLabel: return "wire_color_label";
    case TextSemanticKind::FunctionLabel: return "function_label";
    case TextSemanticKind::Unknown: return "unknown";
    }
    return "unknown";
}

const char* association_target_kind_name(SemanticAssociationTargetKind kind) {
    switch (kind) {
    case SemanticAssociationTargetKind::Component: return "component";
    case SemanticAssociationTargetKind::Endpoint: return "endpoint";
    }
    return "endpoint";
}

const char* distribution_role_name(DistributionRole role) {
    switch (role) {
    case DistributionRole::Ground: return "ground";
    case DistributionRole::PowerFeed: return "power_feed";
    case DistributionRole::SharedFunctionFeed: return "shared_function_feed";
    case DistributionRole::Unknown: return "unresolved";
    }
    return "unresolved";
}

template <typename T, typename F>
void write_string_array(std::ostream& out, const std::vector<T>& items, F to_string) {
    out << "[";
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i) out << ", ";
        out << "\"" << json_escape(to_string(items[i])) << "\"";
    }
    out << "]";
}

} // namespace

void EngineeringDiagramExporter::export_json(
    const EngineeringDiagram& diagram,
    const std::string& output_path) {

    std::ofstream out(output_path);
    if (!out) {
        throw std::runtime_error("Unable to create engineering diagram JSON: " + output_path);
    }

    out << "{\n"
        << "  \"format\": \"eke-dx-wire-engineering-diagram\",\n"
        << "  \"version\": \"1.0\",\n"
        << "  \"coordinate_system\": \"" << kEngineeringDiagramCoordinateSystem << "\",\n"
        << "  \"source_id\": \"" << json_escape(diagram.source_id) << "\",\n"
        << "  \"page\": " << diagram.page << ",\n"
        << "  \"image_width\": " << diagram.image_width << ",\n"
        << "  \"image_height\": " << diagram.image_height << ",\n";

    out << "  \"components\": [\n";
    for (std::size_t i = 0; i < diagram.components.size(); ++i) {
        const auto& c = diagram.components[i];
        out << "    {\n"
            << "      \"component_id\": \"" << json_escape(c.component_id) << "\",\n"
            << "      \"kind\": \"" << component_kind_name(c.kind) << "\",\n"
            << "      \"x\": " << c.bounds.x << ", \"y\": " << c.bounds.y
            << ", \"width\": " << c.bounds.width << ", \"height\": " << c.bounds.height << ",\n"
            << "      \"geometry_confidence\": \"" << confidence_name(c.geometry_confidence) << "\",\n"
            << "      \"semantic_labels\": ";
        write_string_array(out, c.semantic_labels, [](const std::string& s) { return s; });
        out << ",\n"
            << "      \"canonical_name\": \"" << json_escape(c.canonical_name) << "\",\n"
            << "      \"identity_status\": \"" << status_name(c.identity_status) << "\",\n"
            << "      \"identity_evidence_ids\": ";
        write_string_array(out, c.identity_evidence_ids, [](const std::string& s) { return s; });
        out << ",\n"
            << "      \"symbol_geometry_id\": \"" << json_escape(c.symbol_geometry_id) << "\",\n"
            << "      \"terminal_candidate_ids\": ";
        write_string_array(out, c.terminal_candidate_ids, [](const std::string& s) { return s; });
        out << ",\n      \"endpoint_ids\": ";
        write_string_array(out, c.endpoint_ids, [](const std::string& s) { return s; });
        out << "\n    }";
        if (i + 1 != diagram.components.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";

    out << "  \"connectors\": [\n";
    for (std::size_t i = 0; i < diagram.connectors.size(); ++i) {
        const auto& c = diagram.connectors[i];
        out << "    {\n"
            << "      \"connector_id\": \"" << json_escape(c.connector_id) << "\",\n"
            << "      \"component_id\": \"" << json_escape(c.component_id) << "\",\n"
            << "      \"x\": " << c.bounds.x << ", \"y\": " << c.bounds.y
            << ", \"width\": " << c.bounds.width << ", \"height\": " << c.bounds.height << ",\n"
            << "      \"confidence\": \"" << confidence_name(c.confidence) << "\",\n"
            << "      \"connector_terminal_ids\": ";
        write_string_array(out, c.connector_terminal_ids, [](const std::string& s) { return s; });
        out << "\n    }";
        if (i + 1 != diagram.connectors.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";

    out << "  \"wires\": [\n";
    for (std::size_t i = 0; i < diagram.wires.size(); ++i) {
        const auto& w = diagram.wires[i];
        out << "    {\n"
            << "      \"wire_id\": \"" << json_escape(w.wire_id) << "\",\n"
            << "      \"start_endpoint_id\": \"" << json_escape(w.start_endpoint_id) << "\",\n"
            << "      \"end_endpoint_id\": \"" << json_escape(w.end_endpoint_id) << "\",\n"
            << "      \"topology_edge_ids\": ";
        write_string_array(out, w.topology_edge_ids, [](const std::string& s) { return s; });
        out << ",\n      \"conductor_segment_ids\": ";
        write_string_array(out, w.conductor_segment_ids, [](const std::string& s) { return s; });
        out << ",\n"
            << "      \"geometry_confidence\": \"" << confidence_name(w.geometry_confidence) << "\",\n"
            << "      \"heavy_cable\": " << (w.heavy_cable ? "true" : "false") << ",\n"
            << "      \"wire_semantic_resolution_id\": \""
            << json_escape(w.wire_semantic_resolution_id) << "\"\n"
            << "    }";
        if (i + 1 != diagram.wires.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";

    out << "  \"splices\": [\n";
    for (std::size_t i = 0; i < diagram.splices.size(); ++i) {
        const auto& s = diagram.splices[i];
        out << "    {\n"
            << "      \"node_id\": \"" << json_escape(s.node_id) << "\",\n"
            << "      \"x\": " << s.position.x << ", \"y\": " << s.position.y << ",\n"
            << "      \"type\": \"" << node_type_name(s.type) << "\",\n"
            << "      \"incident_wire_ids\": ";
        write_string_array(out, s.incident_wire_ids, [](const std::string& v) { return v; });
        out << "\n    }";
        if (i + 1 != diagram.splices.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";

    out << "  \"crossing_node_ids\": ";
    write_string_array(out, diagram.crossing_node_ids, [](const std::string& v) { return v; });
    out << ",\n";

    out << "  \"labels\": [\n";
    for (std::size_t i = 0; i < diagram.labels.size(); ++i) {
        const auto& l = diagram.labels[i];
        out << "    {\n"
            << "      \"text_region_id\": \"" << json_escape(l.text_region_id) << "\",\n"
            << "      \"raw_text\": \"" << json_escape(l.raw_text) << "\",\n"
            << "      \"normalized_text\": \"" << json_escape(l.normalized_text) << "\",\n"
            << "      \"kind\": \"" << text_semantic_kind_name(l.kind) << "\",\n"
            << "      \"confidence\": \"" << confidence_name(l.confidence) << "\",\n"
            << "      \"status\": \"" << status_name(l.status) << "\",\n"
            << "      \"provider\": \"" << json_escape(l.provider) << "\",\n"
            << "      \"associated_object_id\": \"" << json_escape(l.associated_object_id) << "\",\n"
            << "      \"associated_object_kind\": \""
            << association_target_kind_name(l.associated_object_kind) << "\"\n"
            << "    }";
        if (i + 1 != diagram.labels.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";

    out << "  \"electrical_nets\": [\n";
    for (std::size_t i = 0; i < diagram.electrical_nets.size(); ++i) {
        const auto& n = diagram.electrical_nets[i];
        out << "    {\n"
            << "      \"net_id\": \"" << json_escape(n.net_id) << "\",\n"
            << "      \"endpoint_ids\": ";
        write_string_array(out, n.endpoint_ids, [](const std::string& v) { return v; });
        out << ",\n      \"splice_node_ids\": ";
        write_string_array(out, n.splice_node_ids, [](const std::string& v) { return v; });
        out << ",\n      \"topology_edge_ids\": ";
        write_string_array(out, n.topology_edge_ids, [](const std::string& v) { return v; });
        out << ",\n      \"wire_ids\": ";
        write_string_array(out, n.wire_ids, [](const std::string& v) { return v; });
        out << ",\n"
            << "      \"role\": \"" << distribution_role_name(n.role) << "\",\n"
            << "      \"confidence\": \"" << confidence_name(n.confidence) << "\"\n"
            << "    }";
        if (i + 1 != diagram.electrical_nets.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";

    const auto& v = diagram.validation;
    out << "  \"validation\": {\n"
        << "    \"valid_references\": " << v.valid_references << ",\n"
        << "    \"invalid_references\": " << v.invalid_references << ",\n"
        << "    \"duplicate_relationships\": " << v.duplicate_relationships << ",\n"
        << "    \"orphan_objects\": " << v.orphan_objects << ",\n"
        << "    \"issues\": [\n";
    for (std::size_t i = 0; i < v.issues.size(); ++i) {
        const auto& issue = v.issues[i];
        out << "      {\n"
            << "        \"category\": \"" << json_escape(issue.category) << "\",\n"
            << "        \"code\": \"" << json_escape(issue.code) << "\",\n"
            << "        \"object_id\": \"" << json_escape(issue.object_id) << "\",\n"
            << "        \"detail\": \"" << json_escape(issue.detail) << "\"\n"
            << "      }";
        if (i + 1 != v.issues.size()) out << ",";
        out << "\n";
    }
    out << "    ]\n"
        << "  }\n"
        << "}\n";
}

} // namespace eke::dx::wire

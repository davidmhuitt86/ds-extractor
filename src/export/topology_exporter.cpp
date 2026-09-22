#include "eke_dx_wire/export/topology_exporter.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

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

} // namespace

void TopologyExporter::export_json(
    const WireModel& model,
    const std::string& output_path) {

    std::ofstream out(output_path);
    if (!out) {
        throw std::runtime_error(
            "Unable to create topology JSON: " + output_path);
    }

    out << "{\n";
    out << "  \"source_id\": \"" << json_escape(model.source_id) << "\",\n";
    out << "  \"page\": " << model.page << ",\n";
    out << "  \"nodes\": [\n";

    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        const auto& node = model.nodes[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(node.id) << "\",\n"
            << "      \"x\": " << node.position.x << ",\n"
            << "      \"y\": " << node.position.y << ",\n"
            << "      \"type\": \"" << node_type_name(node.type) << "\",\n"
            << "      \"electrically_connective\": "
            << (node.electrically_connective ? "true" : "false") << "\n"
            << "    }";
        if (i + 1 != model.nodes.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n";
    out << "  \"edges\": [\n";

    for (std::size_t i = 0; i < model.edges.size(); ++i) {
        const auto& edge = model.edges[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(edge.id) << "\",\n"
            << "      \"from_node\": \"" << json_escape(edge.from_node) << "\",\n"
            << "      \"to_node\": \"" << json_escape(edge.to_node) << "\",\n"
            << "      \"conductor_segment\": \""
            << json_escape(edge.conductor_segment) << "\"\n"
            << "    }";
        if (i + 1 != model.edges.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n";
    out << "  \"wires\": []\n";
    out << "}\n";
}

} // namespace eke::dx::wire

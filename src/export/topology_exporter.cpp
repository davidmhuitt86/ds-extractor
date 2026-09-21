#include "eke_dx_wire/export/topology_exporter.hpp"

#include <fstream>
#include <stdexcept>

namespace eke::dx::wire {
namespace {

const char* node_type_name(TopologyNodeType type) {
    switch (type) {
    case TopologyNodeType::ConductorEnd: return "conductor_end";
    case TopologyNodeType::Continuation: return "continuation";
    case TopologyNodeType::Junction: return "junction";
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
    out << "  \"source_id\": \"" << model.source_id << "\",\n";
    out << "  \"page\": " << model.page << ",\n";
    out << "  \"nodes\": [\n";

    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        const auto& node = model.nodes[i];
        out << "    {\n"
            << "      \"id\": \"" << node.id << "\",\n"
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
            << "      \"id\": \"" << edge.id << "\",\n"
            << "      \"from_node\": \"" << edge.from_node << "\",\n"
            << "      \"to_node\": \"" << edge.to_node << "\",\n"
            << "      \"conductor_segment\": \""
            << edge.conductor_segment << "\"\n"
            << "    }";
        if (i + 1 != model.edges.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n";
    out << "  \"wires\": []\n";
    out << "}\n";
}

} // namespace eke::dx::wire

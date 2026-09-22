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
        throw std::runtime_error("Unable to create topology JSON: " + output_path);
    }

    auto json_escape = [](const std::string& value) {
        std::string result;
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
    };

    auto confidence_name = [](ConfidenceClass confidence) {
        switch (confidence) {
        case ConfidenceClass::High: return "high";
        case ConfidenceClass::Medium: return "medium";
        case ConfidenceClass::Low: return "low";
        case ConfidenceClass::Unresolved: return "unresolved";
        }
        return "unresolved";
    };

    auto distribution_role_name = [](DistributionRole role) {
        switch (role) {
        case DistributionRole::Ground: return "ground";
        case DistributionRole::PowerFeed: return "power_feed";
        case DistributionRole::SharedFunctionFeed: return "shared_function_feed";
        case DistributionRole::Unknown: return "unknown";
        }
        return "unknown";
    };

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

    out << "  ],\n  \"edges\": [\n";
    for (std::size_t i = 0; i < model.edges.size(); ++i) {
        const auto& edge = model.edges[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(edge.id) << "\",\n"
            << "      \"from_node\": \"" << json_escape(edge.from_node) << "\",\n"
            << "      \"to_node\": \"" << json_escape(edge.to_node) << "\",\n"
            << "      \"conductor_segment\": \"" << json_escape(edge.conductor_segment) << "\"\n"
            << "    }";
        if (i + 1 != model.edges.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n  \"wires\": [\n";
    for (std::size_t i = 0; i < model.wires.size(); ++i) {
        const auto& wire = model.wires[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(wire.id) << "\",\n"
            << "      \"start_endpoint\": \"" << json_escape(wire.start_endpoint) << "\",\n"
            << "      \"end_endpoint\": \"" << json_escape(wire.end_endpoint) << "\",\n"
            << "      \"confidence\": \"" << confidence_name(wire.confidence) << "\",\n"
            << "      \"heavy_cable\": " << (wire.heavy_cable ? "true" : "false") << ",\n"
            << "      \"topology_edges\": [";
        for (std::size_t j = 0; j < wire.topology_edges.size(); ++j) {
            if (j) out << ", ";
            out << "\"" << json_escape(wire.topology_edges[j]) << "\"";
        }
        out << "],\n      \"conductor_segments\": [";
        for (std::size_t j = 0; j < wire.conductor_segments.size(); ++j) {
            if (j) out << ", ";
            out << "\"" << json_escape(wire.conductor_segments[j]) << "\"";
        }
        out << "]\n    }";
        if (i + 1 != model.wires.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n  \"electrical_nets\": [\n";
    for (std::size_t i = 0; i < model.electrical_nets.size(); ++i) {
        const auto& net = model.electrical_nets[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(net.id) << "\",\n"
            << "      \"role\": \"" << distribution_role_name(net.role) << "\",\n"
            << "      \"confidence\": \"" << confidence_name(net.confidence) << "\",\n"
            << "      \"anchor_endpoint\": \"" << json_escape(net.anchor_endpoint) << "\",\n"
            << "      \"endpoint_ids\": [";
        for (std::size_t j = 0; j < net.endpoint_ids.size(); ++j) {
            if (j) out << ", ";
            out << "\"" << json_escape(net.endpoint_ids[j]) << "\"";
        }
        out << "],\n      \"splice_node_ids\": [";
        for (std::size_t j = 0; j < net.splice_node_ids.size(); ++j) {
            if (j) out << ", ";
            out << "\"" << json_escape(net.splice_node_ids[j]) << "\"";
        }
        out << "],\n      \"topology_edges\": [";
        for (std::size_t j = 0; j < net.topology_edges.size(); ++j) {
            if (j) out << ", ";
            out << "\"" << json_escape(net.topology_edges[j]) << "\"";
        }
        out << "]\n    }";
        if (i + 1 != model.electrical_nets.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
}
} // namespace eke::dx::wire

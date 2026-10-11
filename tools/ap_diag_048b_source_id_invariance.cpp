#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

namespace {

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20) {
                out << "\\u" << std::hex << std::setw(4)
                    << std::setfill('0') << static_cast<int>(ch)
                    << std::dec;
            } else {
                out << static_cast<char>(ch);
            }
        }
    }
    return out.str();
}

std::string number(double value) {
    std::ostringstream out;
    out << std::setprecision(15) << value;
    return out.str();
}

std::string point_key(Point2D point) {
    return number(point.x) + "," + number(point.y);
}

bool point_less(Point2D a, Point2D b) {
    if (a.x != b.x) return a.x < b.x;
    return a.y < b.y;
}

std::string segment_geometry_key(const Segment2D& segment) {
    Point2D a = segment.a;
    Point2D b = segment.b;
    if (point_less(b, a)) std::swap(a, b);
    return point_key(a) + "--" + point_key(b);
}

std::string node_type_name(TopologyNodeType type) {
    switch (type) {
    case TopologyNodeType::ConductorEnd: return "conductor_end";
    case TopologyNodeType::Continuation: return "continuation";
    case TopologyNodeType::Junction: return "junction";
    case TopologyNodeType::Splice: return "splice";
    case TopologyNodeType::Crossing: return "crossing";
    case TopologyNodeType::ComponentBoundary: return "component_boundary";
    default: return "unresolved";
    }
}

std::string endpoint_kind_name(EndpointKind kind) {
    switch (kind) {
    case EndpointKind::GeometricConductorEnd: return "geometric_conductor_end";
    case EndpointKind::ComponentTerminal: return "component_terminal";
    case EndpointKind::ConnectorTerminal: return "connector_terminal";
    case EndpointKind::Splice: return "splice";
    case EndpointKind::Ground: return "ground";
    case EndpointKind::ExternalConnection: return "external_connection";
    default: return "unresolved";
    }
}

std::string fnv1a_file_hash(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to open source file for hashing: " + path.string());

    std::uint64_t hash = 14695981039346656037ull;
    char buffer[8192];
    while (input) {
        input.read(buffer, sizeof(buffer));
        const auto count = input.gcount();
        for (std::streamsize i = 0; i < count; ++i) {
            hash ^= static_cast<unsigned char>(buffer[i]);
            hash *= 1099511628211ull;
        }
    }
    if (!input.eof()) throw std::runtime_error("Error reading source file for hashing: " + path.string());

    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

std::vector<std::string> sorted_values(std::vector<std::string> values) {
    std::sort(values.begin(), values.end());
    return values;
}

bool same_multiset(
    const std::vector<std::string>& a,
    const std::vector<std::string>& b) {
    return sorted_values(a) == sorted_values(b);
}

std::vector<std::string> values_only_in(
    const std::vector<std::string>& left,
    const std::vector<std::string>& right) {
    const auto a = sorted_values(left);
    const auto b = sorted_values(right);
    std::vector<std::string> result;
    std::set_difference(a.begin(), a.end(), b.begin(), b.end(),
                        std::back_inserter(result));
    return result;
}

void write_string_array(std::ostream& out, const std::vector<std::string>& values) {
    out << "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) out << ",";
        out << "\"" << json_escape(values[i]) << "\"";
    }
    out << "]";
}

struct NodePositionType {
    std::string type;
    bool electrically_connective = true;
};

struct RunSnapshot {
    std::string label;
    std::string source_id;
    const WireModel* model = nullptr;
    std::vector<std::string> segment_geometry_sequence;
    std::vector<std::string> node_positions;
    std::vector<std::string> endpoint_positions;
    std::vector<std::string> edge_geometry_keys;
    std::vector<std::string> wire_endpoint_pairs;
    std::map<std::string, NodePositionType> node_type_at_position;
    std::map<std::string, Point2D> endpoint_position_by_id;
    std::map<std::string, std::string> segment_geometry_by_id;
    std::map<std::string, std::string> node_position_by_id;
    std::map<std::string, std::string> endpoint_node_type_by_id;
};

RunSnapshot snapshot_model(
    const std::string& label,
    const std::string& source_id,
    const WireModel& model) {
    RunSnapshot snapshot;
    snapshot.label = label;
    snapshot.source_id = source_id;
    snapshot.model = &model;

    for (const auto& segment : model.conductor_segments) {
        const std::string key = segment_geometry_key(segment.geometry);
        snapshot.segment_geometry_sequence.push_back(key);
        snapshot.segment_geometry_by_id.emplace(segment.id, key);
    }

    for (const auto& node : model.nodes) {
        const std::string key = point_key(node.position);
        snapshot.node_positions.push_back(key);
        snapshot.node_type_at_position.emplace(
            key, NodePositionType{node_type_name(node.type), node.electrically_connective});
        snapshot.node_position_by_id.emplace(node.id, key);
    }

    for (const auto& endpoint : model.endpoint_candidates) {
        const std::string key = point_key(endpoint.position);
        snapshot.endpoint_positions.push_back(key);
        snapshot.endpoint_position_by_id.emplace(endpoint.id, endpoint.position);
        const auto node_it = std::find_if(
            model.nodes.begin(), model.nodes.end(),
            [&endpoint](const TopologyNode& node) { return node.id == endpoint.node_id; });
        snapshot.endpoint_node_type_by_id.emplace(
            endpoint.id,
            node_it == model.nodes.end() ? "missing_node" : node_type_name(node_it->type));
    }

    for (const auto& edge : model.edges) {
        const auto from_it = snapshot.node_position_by_id.find(edge.from_node);
        const auto to_it = snapshot.node_position_by_id.find(edge.to_node);
        const auto segment_it = snapshot.segment_geometry_by_id.find(edge.conductor_segment);
        if (from_it == snapshot.node_position_by_id.end() ||
            to_it == snapshot.node_position_by_id.end()) {
            snapshot.edge_geometry_keys.push_back("UNRESOLVED_NODE_REFERENCE:" + edge.id);
            continue;
        }
        const std::string segment_key =
            edge.conductor_segment.empty()
                ? std::string("<no-conductor-segment>")
                : segment_it != snapshot.segment_geometry_by_id.end()
                    ? segment_it->second
                    : std::string("<missing-conductor-segment-geometry>");
        std::string a = from_it->second;
        std::string b = to_it->second;
        if (b < a) std::swap(a, b);
        snapshot.edge_geometry_keys.push_back(
            segment_key + "|" + a + "<->" + b);
    }

    for (const auto& wire : model.wires) {
        const auto start_it = snapshot.endpoint_position_by_id.find(wire.start_endpoint);
        const auto end_it = snapshot.endpoint_position_by_id.find(wire.end_endpoint);
        if (start_it == snapshot.endpoint_position_by_id.end() ||
            end_it == snapshot.endpoint_position_by_id.end()) {
            snapshot.wire_endpoint_pairs.push_back("UNRESOLVED:" + wire.id);
            continue;
        }
        std::string a = point_key(start_it->second);
        std::string b = point_key(end_it->second);
        if (b < a) std::swap(a, b);
        snapshot.wire_endpoint_pairs.push_back(a + "<->" + b);
    }

    return snapshot;
}

void write_snapshot(std::ostream& out, const RunSnapshot& snapshot) {
    const WireModel& model = *snapshot.model;
    out << "{\n"
        << "      \"label\":\"" << json_escape(snapshot.label) << "\",\n"
        << "      \"source_id\":\"" << json_escape(snapshot.source_id) << "\",\n"
        << "      \"counts\":{\"normalized_conductor_segments\":" << model.conductor_segments.size()
        << ",\"wires\":" << model.wires.size()
        << ",\"endpoints\":" << model.endpoint_candidates.size()
        << ",\"nodes\":" << model.nodes.size()
        << ",\"edges\":" << model.edges.size()
        << ",\"components\":" << model.component_candidates.size()
        << ",\"connectors\":" << model.connector_candidates.size() << "},\n";

    out << "      \"normalized_conductor_segments\":[";
    for (std::size_t i = 0; i < model.conductor_segments.size(); ++i) {
        const auto& segment = model.conductor_segments[i];
        if (i) out << ",";
        out << "\n        {\"sequence_index\":" << i
            << ",\"id\":\"" << json_escape(segment.id)
            << "\",\"geometry_key\":\"" << json_escape(snapshot.segment_geometry_sequence[i])
            << "\",\"a\":{\"x\":" << number(segment.geometry.a.x)
            << ",\"y\":" << number(segment.geometry.a.y)
            << "},\"b\":{\"x\":" << number(segment.geometry.b.x)
            << ",\"y\":" << number(segment.geometry.b.y)
            << "},\"thickness_px\":" << number(segment.thickness_px)
            << ",\"heavy_cable\":" << (segment.heavy_cable ? "true" : "false")
            << ",\"provenance_source_id\":\"" << json_escape(segment.provenance.source_id)
            << "\"}";
    }
    out << "\n      ],\n";

    out << "      \"nodes\":[";
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        const auto& node = model.nodes[i];
        if (i) out << ",";
        out << "\n        {\"id\":\"" << json_escape(node.id)
            << "\",\"position_key\":\"" << json_escape(point_key(node.position))
            << "\",\"x\":" << number(node.position.x)
            << ",\"y\":" << number(node.position.y)
            << ",\"type\":\"" << node_type_name(node.type)
            << "\",\"electrically_connective\":" << (node.electrically_connective ? "true" : "false")
            << "}";
    }
    out << "\n      ],\n";

    out << "      \"edges\":[";
    for (std::size_t i = 0; i < model.edges.size(); ++i) {
        const auto& edge = model.edges[i];
        if (i) out << ",";
        const auto from_it = snapshot.node_position_by_id.find(edge.from_node);
        const auto to_it = snapshot.node_position_by_id.find(edge.to_node);
        const auto seg_it = snapshot.segment_geometry_by_id.find(edge.conductor_segment);
        out << "\n        {\"id\":\"" << json_escape(edge.id) << "\"";
        if (from_it != snapshot.node_position_by_id.end())
            out << ",\"from_position_key\":\"" << json_escape(from_it->second) << "\"";
        else
            out << ",\"from_position_key\":null";
        if (to_it != snapshot.node_position_by_id.end())
            out << ",\"to_position_key\":\"" << json_escape(to_it->second) << "\"";
        else
            out << ",\"to_position_key\":null";
        if (seg_it != snapshot.segment_geometry_by_id.end())
            out << ",\"segment_geometry_key\":\"" << json_escape(seg_it->second) << "\"";
        else
            out << ",\"segment_geometry_key\":null";
        out << "}";
    }
    out << "\n      ],\n";

    out << "      \"endpoints\":[";
    for (std::size_t i = 0; i < model.endpoint_candidates.size(); ++i) {
        const auto& endpoint = model.endpoint_candidates[i];
        if (i) out << ",";
        out << "\n        {\"id\":\"" << json_escape(endpoint.id)
            << "\",\"node_id\":\"" << json_escape(endpoint.node_id)
            << "\",\"position_key\":\"" << json_escape(point_key(endpoint.position))
            << "\",\"x\":" << number(endpoint.position.x)
            << ",\"y\":" << number(endpoint.position.y)
            << ",\"kind\":\"" << endpoint_kind_name(endpoint.kind)
            << "\",\"node_type\":\"" << json_escape(snapshot.endpoint_node_type_by_id.at(endpoint.id))
            << "\",\"incident_edge_count\":" << endpoint.incident_edges.size()
            << "}";
    }
    out << "\n      ],\n";

    out << "      \"wires\":[";
    for (std::size_t i = 0; i < model.wires.size(); ++i) {
        const auto& wire = model.wires[i];
        if (i) out << ",";
        const auto start_it = snapshot.endpoint_position_by_id.find(wire.start_endpoint);
        const auto end_it = snapshot.endpoint_position_by_id.find(wire.end_endpoint);
        out << "\n        {\"id\":\"" << json_escape(wire.id) << "\"";
        if (start_it != snapshot.endpoint_position_by_id.end())
            out << ",\"start_position_key\":\"" << json_escape(point_key(start_it->second)) << "\"";
        else out << ",\"start_position_key\":null";
        if (end_it != snapshot.endpoint_position_by_id.end())
            out << ",\"end_position_key\":\"" << json_escape(point_key(end_it->second)) << "\"";
        else out << ",\"end_position_key\":null";
        out << ",\"segment_geometry_keys\":[";
        bool first_segment = true;
        for (const auto& segment_id : wire.conductor_segments) {
            if (!first_segment) out << ",";
            first_segment = false;
            const auto segment_it = snapshot.segment_geometry_by_id.find(segment_id);
            if (segment_it == snapshot.segment_geometry_by_id.end())
                out << "null";
            else
                out << "\"" << json_escape(segment_it->second) << "\"";
        }
        out << "]}";
    }
    out << "\n      ]\n"
        << "    }";
}

void write_key_list(
    std::ostream& out,
    const std::vector<std::string>& values) {
    write_string_array(out, values);
}

std::vector<std::tuple<std::string, std::string, std::string>> node_type_differences(
    const RunSnapshot& a,
    const RunSnapshot& b) {
    std::vector<std::tuple<std::string, std::string, std::string>> result;
    for (const auto& [position, type_a] : a.node_type_at_position) {
        const auto it = b.node_type_at_position.find(position);
        if (it == b.node_type_at_position.end()) continue;
        if (type_a.type != it->second.type ||
            type_a.electrically_connective != it->second.electrically_connective) {
            result.emplace_back(position, type_a.type, it->second.type);
        }
    }
    return result;
}

void write_node_type_differences(
    std::ostream& out,
    const std::vector<std::tuple<std::string, std::string, std::string>>& diffs) {
    out << "[";
    for (std::size_t i = 0; i < diffs.size(); ++i) {
        if (i) out << ",";
        out << "{\"position_key\":\"" << json_escape(std::get<0>(diffs[i]))
            << "\",\"basename_source_id_type\":\"" << json_escape(std::get<1>(diffs[i]))
            << "\",\"path_source_id_type\":\"" << json_escape(std::get<2>(diffs[i]))
            << "\"}";
    }
    out << "]";
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            std::cerr << "Usage: dx-audit-source-id-invariance <source-image> <output-json>\n";
            return 2;
        }

        const fs::path source_path = argv[1];
        const fs::path output_path = argv[2];
        const cv::Mat image = cv::imread(source_path.string(), cv::IMREAD_GRAYSCALE);
        if (image.empty()) throw std::runtime_error("Unable to load source image: " + source_path.string());

        const auto source_bytes = fs::file_size(source_path);
        const std::string source_hash = fnv1a_file_hash(source_path);
        const std::string basename_source_id = source_path.filename().string();
        const std::string path_source_id = source_path.string();
        if (basename_source_id == path_source_id) {
            throw std::runtime_error(
                "Pass a source path with a directory component (for example samples\\\\trx300ODG.png) "
                "so basename and full-path source_id variants are distinct.");
        }

        // The only intended input difference between these sequential runs is source_id.
        ExtractionPipeline pipeline;
        const WireModel basename_model = pipeline.run(source_path.string(), basename_source_id);
        const WireModel path_model = pipeline.run(source_path.string(), path_source_id);

        const RunSnapshot basename_snapshot =
            snapshot_model("basename_source_id", basename_source_id, basename_model);
        const RunSnapshot path_snapshot =
            snapshot_model("path_source_id", path_source_id, path_model);

        const auto segment_count_a = basename_snapshot.segment_geometry_sequence.size();
        const auto segment_count_b = path_snapshot.segment_geometry_sequence.size();
        std::size_t first_segment_order_divergence = 0;
        const std::size_t segment_common_count = std::min(segment_count_a, segment_count_b);
        while (first_segment_order_divergence < segment_common_count &&
               basename_snapshot.segment_geometry_sequence[first_segment_order_divergence] ==
                   path_snapshot.segment_geometry_sequence[first_segment_order_divergence]) {
            ++first_segment_order_divergence;
        }
        const bool segment_sequence_equal =
            basename_snapshot.segment_geometry_sequence == path_snapshot.segment_geometry_sequence;
        const bool has_segment_sequence_divergence = !segment_sequence_equal;
        if (has_segment_sequence_divergence &&
            first_segment_order_divergence == segment_common_count &&
            segment_count_a != segment_count_b) {
            first_segment_order_divergence = segment_common_count;
        }
        const bool segment_geometry_multiset_equal =
            same_multiset(basename_snapshot.segment_geometry_sequence,
                          path_snapshot.segment_geometry_sequence);
        const bool node_positions_equal =
            same_multiset(basename_snapshot.node_positions, path_snapshot.node_positions);
        const bool endpoint_positions_equal =
            same_multiset(basename_snapshot.endpoint_positions, path_snapshot.endpoint_positions);
        const bool edge_geometry_equal =
            same_multiset(basename_snapshot.edge_geometry_keys, path_snapshot.edge_geometry_keys);
        const bool wire_endpoint_pairs_equal =
            same_multiset(basename_snapshot.wire_endpoint_pairs, path_snapshot.wire_endpoint_pairs);
        const auto node_types_diff = node_type_differences(basename_snapshot, path_snapshot);

        fs::create_directories(output_path.parent_path().empty() ? fs::path(".") : output_path.parent_path());
        std::ofstream out(output_path);
        if (!out) throw std::runtime_error("Unable to open output JSON: " + output_path.string());

        out << "{\n"
            << "  \"ap\":\"AP-DIAG-048B\",\n"
            << "  \"status\":\"diagnostic_only\",\n"
            << "  \"production_logic_modified\":false,\n"
            << "  \"source_image\":{\"path\":\"" << json_escape(source_path.string())
            << "\",\"width\":" << image.cols << ",\"height\":" << image.rows
            << ",\"size_bytes\":" << source_bytes
            << ",\"fnv1a64\":\"" << source_hash << "\"},\n"
            << "  \"run_order\":\"basename_source_id_then_path_source_id\",\n"
            << "  \"runs\":[\n    ";
        write_snapshot(out, basename_snapshot);
        out << ",\n    ";
        write_snapshot(out, path_snapshot);
        out << "\n  ],\n"
            << "  \"comparison\":{\n"
            << "    \"source_id_is_the_only_intended_input_difference\":true,\n"
            << "    \"normalized_segment_count_basename_source_id\":" << segment_count_a << ",\n"
            << "    \"normalized_segment_count_path_source_id\":" << segment_count_b << ",\n"
            << "    \"normalized_segment_geometry_multiset_equal\":"
            << (segment_geometry_multiset_equal ? "true" : "false") << ",\n"
            << "    \"normalized_segment_geometry_sequence_equal\":"
            << (segment_sequence_equal ? "true" : "false") << ",\n"
            << "    \"first_segment_sequence_divergence_index\":";
        if (has_segment_sequence_divergence)
            out << first_segment_order_divergence;
        else
            out << "null";
        out << ",\n"
            << "    \"node_position_multiset_equal\":" << (node_positions_equal ? "true" : "false") << ",\n"
            << "    \"node_positions_only_basename_source_id\":";
        write_key_list(out, values_only_in(basename_snapshot.node_positions, path_snapshot.node_positions));
        out << ",\n    \"node_positions_only_path_source_id\":";
        write_key_list(out, values_only_in(path_snapshot.node_positions, basename_snapshot.node_positions));
        out << ",\n    \"common_position_node_type_or_connectivity_differences\":";
        write_node_type_differences(out, node_types_diff);
        out << ",\n"
            << "    \"endpoint_position_multiset_equal\":" << (endpoint_positions_equal ? "true" : "false") << ",\n"
            << "    \"endpoint_positions_only_basename_source_id\":";
        write_key_list(out, values_only_in(basename_snapshot.endpoint_positions, path_snapshot.endpoint_positions));
        out << ",\n    \"endpoint_positions_only_path_source_id\":";
        write_key_list(out, values_only_in(path_snapshot.endpoint_positions, basename_snapshot.endpoint_positions));
        out << ",\n"
            << "    \"edge_geometry_multiset_equal\":" << (edge_geometry_equal ? "true" : "false") << ",\n"
            << "    \"edge_geometry_keys_only_basename_source_id\":";
        write_key_list(out, values_only_in(basename_snapshot.edge_geometry_keys, path_snapshot.edge_geometry_keys));
        out << ",\n    \"edge_geometry_keys_only_path_source_id\":";
        write_key_list(out, values_only_in(path_snapshot.edge_geometry_keys, basename_snapshot.edge_geometry_keys));
        out << ",\n"
            << "    \"wire_endpoint_pair_multiset_equal\":" << (wire_endpoint_pairs_equal ? "true" : "false") << ",\n"
            << "    \"wire_endpoint_pairs_only_basename_source_id\":";
        write_key_list(out, values_only_in(basename_snapshot.wire_endpoint_pairs, path_snapshot.wire_endpoint_pairs));
        out << ",\n    \"wire_endpoint_pairs_only_path_source_id\":";
        write_key_list(out, values_only_in(path_snapshot.wire_endpoint_pairs, basename_snapshot.wire_endpoint_pairs));
        out << "\n  }\n}\n";

        if (!out) throw std::runtime_error("Failed while writing report: " + output_path.string());

        std::cout << "[AP-DIAG-048B] Source: " << source_path.string() << "\n"
                  << "[AP-DIAG-048B] Dimensions: " << image.cols << "x" << image.rows << "\n"
                  << "[AP-DIAG-048B] Source FNV-1a-64: " << source_hash << "\n"
                  << "[AP-DIAG-048B] Basename source_id: " << basename_source_id << "\n"
                  << "[AP-DIAG-048B] Path source_id: " << path_source_id << "\n"
                  << "[AP-DIAG-048B] Normalized segments: " << segment_count_a
                  << " / " << segment_count_b << "\n"
                  << "[AP-DIAG-048B] Segment geometry multiset equal: "
                  << (segment_geometry_multiset_equal ? "yes" : "no") << "\n"
                  << "[AP-DIAG-048B] Segment geometry sequence equal: "
                  << (segment_sequence_equal ? "yes" : "no") << "\n"
                  << "[AP-DIAG-048B] Node positions equal: " << (node_positions_equal ? "yes" : "no") << "\n"
                  << "[AP-DIAG-048B] Endpoint positions equal: " << (endpoint_positions_equal ? "yes" : "no") << "\n"
                  << "[AP-DIAG-048B] Edge geometry equal: " << (edge_geometry_equal ? "yes" : "no") << "\n"
                  << "[AP-DIAG-048B] Wire endpoint pairs equal: " << (wire_endpoint_pairs_equal ? "yes" : "no") << "\n"
                  << "[AP-DIAG-048B] Report: " << output_path.string() << "\n"
                  << "[AP-DIAG-048B] Diagnostic only; production extraction unchanged.\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "[AP-DIAG-048B] ERROR: " << e.what() << "\n";
        return 1;
    }
}

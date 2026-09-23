#include "eke_dx_wire/export/artifact_writer.hpp"

#include "eke_dx_wire/export/recognition_input_exporter.hpp"
#include "eke_dx_wire/export/svg_exporter.hpp"
#include "eke_dx_wire/export/topology_exporter.hpp"

#include <fstream>
#include <stdexcept>

namespace fs = std::filesystem;

namespace eke::dx::wire {
namespace {

const char* distribution_role_name(DistributionRole value) {
    switch (value) {
    case DistributionRole::Ground: return "ground";
    case DistributionRole::PowerFeed: return "power_feed";
    case DistributionRole::SharedFunctionFeed: return "shared_function_feed";
    case DistributionRole::Unknown: return "unresolved";
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

void write_audit(const WireModel& model, const fs::path& path) {
    const ExtractionAudit& audit = model.audit;
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Unable to create extraction audit: " + path.string());
    }

    out << "{\n"
        << "  \"conductor_segments\": " << audit.conductor_segments << ",\n"
        << "  \"topology_nodes\": " << audit.topology_nodes << ",\n"
        << "  \"topology_edges\": " << audit.topology_edges << ",\n"
        << "  \"topology_node_types\": {\n"
        << "    \"conductor_end\": " << audit.conductor_end_nodes << ",\n"
        << "    \"continuation\": " << audit.continuation_nodes << ",\n"
        << "    \"junction\": " << audit.junction_nodes << ",\n"
        << "    \"splice\": " << audit.splice_nodes << ",\n"
        << "    \"crossing\": " << audit.crossing_nodes << ",\n"
        << "    \"component_boundary\": " << audit.component_boundary_nodes << ",\n"
        << "    \"unresolved\": " << audit.unresolved_nodes << "\n"
        << "  },\n"
        << "  \"endpoint_candidates\": " << audit.endpoint_candidates << ",\n"
        << "  \"endpoint_kinds\": {\n"
        << "    \"geometric\": " << audit.geometric_endpoints << ",\n"
        << "    \"component_terminal\": " << audit.component_terminals << ",\n"
        << "    \"connector_terminal\": " << audit.connector_terminals << ",\n"
        << "    \"ground\": " << audit.ground_endpoints << ",\n"
        << "    \"external_connection\": " << audit.external_connections << ",\n"
        << "    \"splice\": " << audit.splice_endpoints << ",\n"
        << "    \"unresolved\": " << audit.unresolved_endpoints << "\n"
        << "  },\n"
        << "  \"shapes\": " << audit.shapes << ",\n"
        << "  \"component_symbol_recognitions\": " << audit.component_symbol_recognitions << ",\n"
        << "  \"shape_kinds\": {\n"
        << "    \"enclosure\": " << audit.enclosure_shapes << ",\n"
        << "    \"circular\": " << audit.circular_shapes << ",\n"
        << "    \"chassis_ground\": " << audit.chassis_ground_shapes << ",\n"
        << "    \"primitive\": " << audit.primitive_shapes << ",\n"
        << "    \"unknown\": " << audit.unknown_shapes << "\n"
        << "  },\n"
        << "  \"wires\": " << audit.wires << ",\n"
        << "  \"heavy_cable_wires\": " << audit.heavy_cable_wires << ",\n"
        << "  \"unresolved_wires\": " << audit.unresolved_wires << ",\n"
        << "  \"electrical_nets\": " << audit.electrical_nets << ",\n"
        << "  \"net_roles\": {\n"
        << "    \"ground\": " << audit.ground_nets << ",\n"
        << "    \"power_feed\": " << audit.power_feed_nets << ",\n"
        << "    \"shared_function_feed\": " << audit.shared_function_feed_nets << ",\n"
        << "    \"unresolved\": " << audit.unresolved_nets << "\n"
        << "  },\n"
        << "  \"gaps_bridged\": " << audit.gaps_bridged << ",\n"
        << "  \"validation\": {\n"
        << "    \"valid_wires\": " << audit.valid_wires << ",\n"
        << "    \"errors\": " << audit.validation_errors << ",\n"
        << "    \"warnings\": " << audit.validation_warnings << "\n"
        << "  }\n"
        << "}\n";
}

void write_project(const WireModel& model, const std::string& image_path, const fs::path& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Unable to create project manifest: " + path.string());
    }

    out << "{\n"
        << "  \"format\": \"eke-dx-wire-project\",\n"
        << "  \"version\": \"0.1.2\",\n"
        << "  \"source\": \"" << image_path << "\",\n"
        << "  \"page\": " << model.page << ",\n"
        << "  \"image_width\": " << model.image_width << ",\n"
        << "  \"image_height\": " << model.image_height << ",\n"
        << "  \"conductor_segment_count\": " << model.conductor_segments.size() << ",\n"
        << "  \"topology_node_count\": " << model.nodes.size() << ",\n"
        << "  \"topology_edge_count\": " << model.edges.size() << ",\n"
        << "  \"wire_count\": " << model.wires.size() << ",\n"
        << "  \"component_symbol_recognition_count\": " << model.component_symbol_recognitions.size() << "\n"
        << "}\n";
}

} // namespace

void ExtractionArtifactWriter::write(
    const WireModel& model,
    const cv::Mat& normalized,
    const std::string& image_path,
    const fs::path& output_root) {

    fs::create_directories(output_root / "artifacts" / "normalized");
    fs::create_directories(output_root / "artifacts" / "masks");
    fs::create_directories(output_root / "artifacts" / "segments");
    fs::create_directories(output_root / "artifacts" / "topology");
    fs::create_directories(output_root / "artifacts" / "validation");
    fs::create_directories(output_root / "artifacts" / "audit");
    fs::create_directories(output_root / "artifacts" / "recognition");
    fs::create_directories(output_root / "output");

    RecognitionInputExporter::export_package(
        model,
        normalized,
        image_path,
        (output_root / "artifacts" / "recognition").string());

    SvgExporter::export_segments(
        model,
        (output_root / "output" / "wires.svg").string());

    TopologyExporter::export_json(
        model,
        (output_root / "artifacts" / "topology" / "topology.json").string());

    write_audit(
        model,
        output_root / "artifacts" / "audit" / "extraction_audit.json");

    write_project(
        model,
        image_path,
        output_root / "project.json");
}

} // namespace eke::dx::wire

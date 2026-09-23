#include "eke_dx_wire/export/svg_exporter.hpp"
#include "eke_dx_wire/export/topology_exporter.hpp"
#include "eke_dx_wire/export/recognition_input_exporter.hpp"
#include "eke_dx_wire/image/normalizer.hpp"
#include "eke_dx_wire/image/image_loader.hpp"
#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

static std::string json_escape(const std::string& value) {
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

static void usage() {
    std::cout
        << "dx-extract 0.1.2\n\n"
        << "Usage:\n"
        << "  dx-extract inspect <image>\n"
        << "  dx-extract extract <image> --output <directory>\n";
}

static int inspect(const std::string& path) {
    const cv::Mat image = ImageLoader::load(path);

    std::cout << "source: " << path << "\n"
              << "width: " << image.cols << "\n"
              << "height: " << image.rows << "\n"
              << "channels: " << image.channels() << "\n"
              << "depth: " << image.depth() << "\n";

    return 0;
}

static int extract(const std::string& image_path, const std::string& output) {
    fs::create_directories(fs::path(output) / "artifacts" / "normalized");
    fs::create_directories(fs::path(output) / "artifacts" / "masks");
    fs::create_directories(fs::path(output) / "artifacts" / "segments");
    fs::create_directories(fs::path(output) / "artifacts" / "topology");
    fs::create_directories(fs::path(output) / "artifacts" / "validation");
    fs::create_directories(fs::path(output) / "artifacts" / "audit");
    fs::create_directories(fs::path(output) / "artifacts" / "recognition");
    fs::create_directories(fs::path(output) / "output");

    ExtractionPipeline pipeline;
    WireModel model = pipeline.run(image_path, image_path);

    RecognitionInputExporter::export_package(
        model,
        ImageNormalizer::normalize(ImageLoader::load(image_path)),
        image_path,
        (fs::path(output) / "artifacts" / "recognition").string());

    SvgExporter::export_segments(
        model,
        (fs::path(output) / "output" / "wires.svg").string());

    TopologyExporter::export_json(
        model,
        (fs::path(output) / "artifacts" / "topology" / "topology.json").string());

    const ExtractionAudit& audit = model.audit;
    std::ofstream audit_file(
        fs::path(output) / "artifacts" / "audit" / "extraction_audit.json");
    audit_file << "{\n"
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

    std::ofstream manifest(fs::path(output) / "project.json");
    manifest << "{\n"
             << "  \"format\": \"eke-dx-wire-project\",\n"
             << "  \"version\": \"0.1.2\",\n"
             << "  \"source\": \"" << image_path << "\",\n"
             << "  \"page\": " << model.page << ",\n"
             << "  \"image_width\": " << model.image_width << ",\n"
             << "  \"image_height\": " << model.image_height << ",\n"
             << "  \"conductor_segment_count\": "
             << model.conductor_segments.size() << ",\n"
             << "  \"topology_node_count\": "
             << model.nodes.size() << ",\n"
             << "  \"topology_edge_count\": "
             << model.edges.size() << ",\n"
             << "  \"wire_count\": " << model.wires.size() << "\n"
             << "}\n";

    std::cout << "extracted conductor segments: "
              << model.conductor_segments.size() << "\n"
              << "topology nodes: " << model.nodes.size() << "\n"
              << "topology edges: " << model.edges.size() << "\n"
              << "reconstructed wires: " << model.wires.size() << "\n"
              << "electrical nets: " << model.audit.electrical_nets << "\n"
              << "endpoint candidates: " << model.audit.endpoint_candidates << "\n"
              << "validation errors: " << model.audit.validation_errors << "\n"
              << "validation warnings: " << model.audit.validation_warnings << "\n"
              << "audit: "
              << (fs::path(output) / "artifacts" / "audit" / "extraction_audit.json").string() << "\n"
              << "recognition package: "
              << (fs::path(output) / "artifacts" / "recognition").string() << "\\n"
              << "output: "
              << (fs::path(output) / "output" / "wires.svg").string()
              << "\n";

    return 0;
}

int main(int argc, char** argv) {
    try {
        if (argc < 3) {
            usage();
            return 2;
        }

        const std::string command = argv[1];

        if (command == "inspect") {
            return inspect(argv[2]);
        }

        if (command == "extract") {
            if (argc < 5 || std::string(argv[3]) != "--output") {
                usage();
                return 2;
            }
            return extract(argv[2], argv[4]);
        }

        usage();
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

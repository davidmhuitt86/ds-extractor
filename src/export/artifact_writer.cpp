#include "eke_dx_wire/export/artifact_writer.hpp"

#include "eke_dx_wire/core/coverage_diagnostics.hpp"
#include "eke_dx_wire/export/recognition_input_exporter.hpp"
#include "eke_dx_wire/export/svg_exporter.hpp"
#include "eke_dx_wire/export/topology_exporter.hpp"
#include "eke_dx_wire/export/review_artifact_writer.hpp"

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

const char* coverage_severity_name(CoverageSeverity severity) {
    switch (severity) {
    case CoverageSeverity::Notice: return "notice";
    case CoverageSeverity::Warning: return "warning";
    }
    return "notice";
}

void write_coverage(const CoverageReport& coverage, std::ostream& out) {
    out << "  \"coverage\": {\n"
        << "    \"conductor_segments\": {\n"
        << "      \"total\": " << coverage.conductors.total << ",\n"
        << "      \"unreferenced\": " << coverage.conductors.unreferenced << ",\n"
        << "      \"topology_only\": " << coverage.conductors.topology_only << ",\n"
        << "      \"wire_only\": " << coverage.conductors.wire_only << ",\n"
        << "      \"normal\": " << coverage.conductors.normal << ",\n"
        << "      \"shared\": " << coverage.conductors.shared << "\n"
        << "    },\n"
        << "    \"endpoints\": {\n"
        << "      \"total\": " << coverage.endpoints.total << ",\n"
        << "      \"zero_wire\": " << coverage.endpoints.zero_wire << ",\n"
        << "      \"single_wire\": " << coverage.endpoints.single_wire << ",\n"
        << "      \"multiple_wire\": " << coverage.endpoints.multiple_wire << "\n"
        << "    },\n"
        << "    \"wires\": {\n"
        << "      \"total\": " << coverage.wires.total << ",\n"
        << "      \"valid\": " << coverage.wires.valid << ",\n"
        << "      \"invalid\": " << coverage.wires.invalid << "\n"
        << "    },\n"
        << "    \"topology_edges\": {\n"
        << "      \"total\": " << coverage.topology_edges.total << ",\n"
        << "      \"missing_conductor\": " << coverage.topology_edges.missing_conductor << ",\n"
        << "      \"missing_from_node\": " << coverage.topology_edges.missing_from_node << ",\n"
        << "      \"missing_to_node\": " << coverage.topology_edges.missing_to_node << ",\n"
        << "      \"unowned_by_any_wire\": " << coverage.topology_edges.unowned_by_any_wire << "\n"
        << "    },\n"
        << "    \"topology_nodes\": {\n"
        << "      \"total\": " << coverage.topology_nodes.total << ",\n"
        << "      \"zero_degree\": " << coverage.topology_nodes.zero_degree << ",\n"
        << "      \"low_degree_splice\": " << coverage.topology_nodes.low_degree_splice << ",\n"
        << "      \"conductor_end_without_endpoint\": " << coverage.topology_nodes.conductor_end_without_endpoint << "\n"
        << "    },\n"
        << "    \"components\": {\n"
        << "      \"total\": " << coverage.components.total << ",\n"
        << "      \"diagram_furniture\": " << coverage.components.diagram_furniture << ",\n"
        << "      \"real_candidates\": " << coverage.components.real_candidates << ",\n"
        << "      \"real_with_terminal_evidence\": " << coverage.components.real_with_terminal_evidence << ",\n"
        << "      \"real_without_terminal_evidence\": " << coverage.components.real_without_terminal_evidence << ",\n"
        << "      \"furniture_without_terminal_evidence\": " << coverage.components.furniture_without_terminal_evidence << "\n"
        << "    },\n"
        << "    \"connectors\": {\n"
        << "      \"total\": " << coverage.connectors.total << ",\n"
        << "      \"terminals_total\": " << coverage.connectors.terminals_total << ",\n"
        << "      \"genuine_looking\": " << coverage.connectors.genuine_looking << ",\n"
        << "      \"furniture_derived\": " << coverage.connectors.furniture_derived << ",\n"
        << "      \"unresolved\": " << coverage.connectors.unresolved << "\n"
        << "    },\n"
        << "    \"electrical_nets\": {\n"
        << "      \"total\": " << coverage.electrical_nets.total << ",\n"
        << "      \"endpoints_in_nets_total\": " << coverage.electrical_nets.endpoints_in_nets_total << ",\n"
        << "      \"endpoints_not_in_any_net\": " << coverage.electrical_nets.endpoints_not_in_any_net << ",\n"
        << "      \"endpoints_in_multiple_nets\": " << coverage.electrical_nets.endpoints_in_multiple_nets << "\n"
        << "    },\n"
        << "    \"findings\": [\n";

    for (std::size_t i = 0; i < coverage.findings.size(); ++i) {
        const auto& finding = coverage.findings[i];
        out << "      {\n"
            << "        \"category\": \"" << json_escape(finding.category) << "\",\n"
            << "        \"code\": \"" << json_escape(finding.code) << "\",\n"
            << "        \"object_id\": \"" << json_escape(finding.object_id) << "\",\n"
            << "        \"severity\": \"" << coverage_severity_name(finding.severity) << "\",\n"
            << "        \"detail\": \"" << json_escape(finding.detail) << "\",\n"
            << "        \"related_object_ids\": [";
        for (std::size_t j = 0; j < finding.related_object_ids.size(); ++j) {
            if (j) out << ", ";
            out << "\"" << json_escape(finding.related_object_ids[j]) << "\"";
        }
        out << "]\n      }";
        if (i + 1 != coverage.findings.size()) out << ",";
        out << "\n";
    }

    out << "    ]\n"
        << "  }\n";
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
        << "    \"diagram_furniture\": " << audit.diagram_furniture_shapes << ",\n"
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
        << "  \"symbol_geometry\": {\n"
        << "    \"components_with_symbol_geometry\": " << audit.components_with_symbol_geometry << ",\n"
        << "    \"components_without_symbol_geometry\": " << audit.components_without_symbol_geometry << ",\n"
        << "    \"primitives_total\": " << audit.symbol_primitives << ",\n"
        << "    \"primitive_kinds\": {\n"
        << "      \"line\": " << audit.symbol_primitive_lines << ",\n"
        << "      \"circle\": " << audit.symbol_primitive_circles << ",\n"
        << "      \"rectangle\": " << audit.symbol_primitive_rectangles << ",\n"
        << "      \"terminal_lead\": " << audit.symbol_primitive_terminal_leads << ",\n"
        << "      \"unknown\": " << audit.symbol_primitive_unknown << "\n"
        << "    }\n"
        << "  },\n"
        << "  \"validation\": {\n"
        << "    \"valid_wires\": " << audit.valid_wires << ",\n"
        << "    \"errors\": " << audit.validation_errors << ",\n"
        << "    \"warnings\": " << audit.validation_warnings << ",\n"
        << "    \"warning_codes\": {\n";

    for (std::size_t i = 0; i < audit.validation_warning_summaries.size(); ++i) {
        const auto& summary = audit.validation_warning_summaries[i];
        out << "      \"" << summary.code << "\": " << summary.count
            << (i + 1 == audit.validation_warning_summaries.size() ? "\n" : ",\n");
    }

    out << "    }\n"
        << "  },\n";

    write_coverage(build_coverage_report(model), out);

    out << "}\n";
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
    fs::create_directories(output_root / "artifacts" / "extraction_review");
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

    ReviewArtifactWriter::write(
        model,
        normalized,
        output_root);
}

} // namespace eke::dx::wire

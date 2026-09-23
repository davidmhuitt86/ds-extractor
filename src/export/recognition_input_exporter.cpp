#include "eke_dx_wire/export/recognition_input_exporter.hpp"
#include "eke_dx_wire/core/geometry.hpp"
#include <opencv2/imgcodecs.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace eke::dx::wire {
namespace fs = std::filesystem;
namespace {

std::string json_escape(const std::string& value) {
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

const char* endpoint_kind_name(EndpointKind value) {
    switch (value) {
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

const char* terminal_role_name(TerminalRole value) {
    switch (value) {
    case TerminalRole::Unknown: return "unknown";
    case TerminalRole::ComponentTerminal: return "component_terminal";
    case TerminalRole::ConnectorTerminal: return "connector_terminal";
    case TerminalRole::GroundTerminal: return "ground_terminal";
    case TerminalRole::PowerSource: return "power_source";
    case TerminalRole::ExternalConnection: return "external_connection";
    }
    return "unknown";
}

const char* target_kind_name(SemanticAssociationTargetKind value) {
    return value == SemanticAssociationTargetKind::Component ? "component" : "endpoint";
}

const char* relation_name(SemanticAssociationRelation value) {
    return value == SemanticAssociationRelation::LabelToComponent
        ? "label_to_component" : "label_to_endpoint";
}

double point_to_box(Point2D point, const BoundingBox& box) {
    const double left = box.x;
    const double right = box.x + box.width;
    const double top = box.y;
    const double bottom = box.y + box.height;
    const double dx = point.x < left ? left - point.x : point.x > right ? point.x - right : 0.0;
    const double dy = point.y < top ? top - point.y : point.y > bottom ? point.y - bottom : 0.0;
    return std::hypot(dx, dy);
}

double segment_to_box(const Segment2D& segment, const BoundingBox& box) {
    return (std::min)({
        point_to_box(segment.a, box),
        point_to_box(segment.b, box),
        point_to_box(midpoint(segment.a, segment.b), box)
    });
}

void write_region_metadata(
    const WireModel& model,
    const TextRegion& region,
    const BoundingBox& crop,
    double context_radius,
    const fs::path& path) {

    std::ofstream out(path);
    if (!out) throw std::runtime_error("Unable to create region metadata: " + path.string());

    out << "{\n"
        << "  \"schema\": \"eke-dx-wire-recognition-region-v1\",\n"
        << "  \"region\": {\"id\": \"" << json_escape(region.id)
        << "\", \"x\": " << region.bounds.x
        << ", \"y\": " << region.bounds.y
        << ", \"width\": " << region.bounds.width
        << ", \"height\": " << region.bounds.height
        << ", \"confidence\": " << region.confidence << "},\n"
        << "  \"crop\": {\"x\": " << crop.x
        << ", \"y\": " << crop.y
        << ", \"width\": " << crop.width
        << ", \"height\": " << crop.height << "},\n"
        << "  \"context_radius_px\": " << context_radius << ",\n"
        << "  \"nearby_components\": [\n";

    bool first = true;
    for (const auto& component : model.component_candidates) {
        const Point2D center{
            component.bounds.x + component.bounds.width / 2.0,
            component.bounds.y + component.bounds.height / 2.0};
        const double d = point_to_box(center, region.bounds);
        if (d > context_radius) continue;
        if (!first) out << ",\n";
        first = false;
        out << "    {\"id\": \"" << json_escape(component.id)
            << "\", \"x\": " << component.bounds.x
            << ", \"y\": " << component.bounds.y
            << ", \"width\": " << component.bounds.width
            << ", \"height\": " << component.bounds.height
            << ", \"confidence\": \"" << confidence_name(component.confidence)
            << "\", \"distance_px\": " << d << "}";
    }

    out << "\n  ],\n  \"nearby_endpoints\": [\n";
    first = true;
    for (const auto& endpoint : model.endpoint_candidates) {
        const double d = point_to_box(endpoint.position, region.bounds);
        if (d > context_radius) continue;
        if (!first) out << ",\n";
        first = false;
        out << "    {\"id\": \"" << json_escape(endpoint.id)
            << "\", \"x\": " << endpoint.position.x
            << ", \"y\": " << endpoint.position.y
            << ", \"kind\": \"" << endpoint_kind_name(endpoint.kind)
            << "\", \"terminal_role\": \"" << terminal_role_name(endpoint.terminal_role)
            << "\", \"component_id\": \"" << json_escape(endpoint.component_id)
            << "\", \"terminal_name\": \"" << json_escape(endpoint.terminal_name)
            << "\", \"function_label\": \"" << json_escape(endpoint.function_label)
            << "\", \"wire_color\": \"" << json_escape(endpoint.wire_color)
            << "\", \"distance_px\": " << d << "}";
    }

    out << "\n  ],\n  \"nearby_conductor_segments\": [\n";
    first = true;
    for (const auto& segment : model.conductor_segments) {
        const double d = segment_to_box(segment.geometry, region.bounds);
        if (d > context_radius) continue;
        if (!first) out << ",\n";
        first = false;
        out << "    {\"id\": \"" << json_escape(segment.id)
            << "\", \"x1\": " << segment.geometry.a.x
            << ", \"y1\": " << segment.geometry.a.y
            << ", \"x2\": " << segment.geometry.b.x
            << ", \"y2\": " << segment.geometry.b.y
            << ", \"thickness_px\": " << segment.thickness_px
            << ", \"confidence\": \"" << confidence_name(segment.confidence)
            << "\", \"heavy_cable\": " << (segment.heavy_cable ? "true" : "false")
            << ", \"distance_px\": " << d << "}";
    }

    out << "\n  ],\n  \"existing_semantic_associations\": [\n";
    first = true;
    for (const auto& association : model.semantic_associations) {
        if (association.text_region_id != region.id) continue;
        if (!first) out << ",\n";
        first = false;
        out << "    {\"id\": \"" << json_escape(association.id)
            << "\", \"target_id\": \"" << json_escape(association.target_id)
            << "\", \"target_kind\": \"" << target_kind_name(association.target_kind)
            << "\", \"relation\": \"" << relation_name(association.relation)
            << "\", \"distance_px\": " << association.distance
            << ", \"confidence\": \"" << confidence_name(association.confidence) << "\"}";
    }
    out << "\n  ]\n}\n";
}

} // namespace

void RecognitionInputExporter::export_package(
    const WireModel& model,
    const cv::Mat& normalized_image,
    const std::string& original_image_path,
    const std::string& output_directory) {

    if (normalized_image.empty())
        throw std::invalid_argument("Recognition input export requires a normalized image");

    const fs::path root(output_directory);
    const fs::path regions = root / "regions";
    const fs::path crops = regions / "crops";
    fs::create_directories(crops);
    fs::create_directories(root / "source_original");

    const fs::path original_target =
        root / "source_original" / fs::path(original_image_path).filename();

    const cv::Mat original_image = cv::imread(
        original_image_path,
        cv::IMREAD_UNCHANGED);
    if (original_image.empty()) {
        throw std::runtime_error(
            "Unable to read original image: " + original_image_path);
    }

    if (!cv::imwrite(original_target.string(), original_image)) {
        throw std::runtime_error(
            "Unable to write original image copy: " + original_target.string());
    }

    if (!cv::imwrite((root / "source_normalized.png").string(), normalized_image))
        throw std::runtime_error("Unable to write normalized recognition image");

    constexpr int margin = 24;
    constexpr double context_radius = 180.0;
    const cv::Rect image_bounds(0, 0, normalized_image.cols, normalized_image.rows);

    std::ofstream manifest(root / "manifest.json");
    std::ofstream input(root / "recognition_input.json");
    if (!manifest || !input)
        throw std::runtime_error("Unable to create recognition package metadata");

    manifest << "{\n"
             << "  \"format\": \"eke-dx-wire-recognition-input\",\n"
             << "  \"version\": \"1.0\",\n"
             << "  \"source_id\": \"" << json_escape(model.source_id) << "\",\n"
             << "  \"page\": " << model.page << ",\n"
             << "  \"image_width\": " << model.image_width << ",\n"
             << "  \"image_height\": " << model.image_height << ",\n"
             << "  \"text_region_count\": " << model.text_regions.size() << ",\n"
             << "  \"files\": {\"original\": \"source_original/"
             << json_escape(original_target.filename().string())
             << "\", \"normalized\": \"source_normalized.png\", "
             << "\"recognition_input\": \"recognition_input.json\", "
             << "\"instructions\": \"instructions.md\"}\n}\n";

    input << "{\n"
          << "  \"schema\": \"eke-dx-wire-recognition-input-v1\",\n"
          << "  \"source_id\": \"" << json_escape(model.source_id) << "\",\n"
          << "  \"page\": " << model.page << ",\n"
          << "  \"image\": {\"width\": " << model.image_width
          << ", \"height\": " << model.image_height
          << ", \"normalized\": \"source_normalized.png\"},\n"
          << "  \"recognition_contract\": {"
          << "\"task\": \"read each supplied text region exactly as drawn\", "
          << "\"do_not_invent_text\": true, "
          << "\"preserve_uncertainty\": true, "
          << "\"return_region_id_with_observation\": true},\n"
          << "  \"regions\": [\n";

    bool first_region = true;
    for (const auto& region : model.text_regions) {
        const cv::Rect requested(
            region.bounds.x - margin,
            region.bounds.y - margin,
            region.bounds.width + 2 * margin,
            region.bounds.height + 2 * margin);
        const cv::Rect crop = requested & image_bounds;
        if (crop.empty()) continue;

        if (!cv::imwrite(
                (crops / (region.id + ".png")).string(),
                normalized_image(crop))) {
            throw std::runtime_error("Unable to write text-region crop");
        }

        write_region_metadata(
            model, region,
            {crop.x, crop.y, crop.width, crop.height},
            context_radius,
            regions / (region.id + ".json"));

        if (!first_region) input << ",\n";
        first_region = false;
        input << "    {\"id\": \"" << json_escape(region.id)
              << "\", \"x\": " << region.bounds.x
              << ", \"y\": " << region.bounds.y
              << ", \"width\": " << region.bounds.width
              << ", \"height\": " << region.bounds.height
              << ", \"crop\": \"regions/crops/" << json_escape(region.id)
              << ".png\", \"metadata\": \"regions/"
              << json_escape(region.id) << ".json\"}";
    }
    input << "\n  ]\n}\n";

    std::ofstream instructions(root / "instructions.md");
    instructions
        << "# EKE-DX-WIRE Recognition Input\n\n"
        << "Read the text in each supplied region using the source diagram "
           "and region crop as visual evidence. Return one observation per "
           "region ID. Do not invent characters that are not visually supported.\n\n"
        << "Required fields: text_region_id, raw_text, confidence, notes.\n\n"
        << "Confidence values: high, medium, low, unresolved. Preserve uncertainty.\n\n"
        << "Neighboring components, endpoints, and conductors are context only. "
           "They must not be used to invent missing text. A splice or junction "
           "is not a wire endpoint; wire identity remains endpoint-to-endpoint.\n";

    std::ofstream schema(root / "schema.json");
    schema
        << "{\n"
        << "  \"format\": \"eke-dx-wire-recognition-observation\",\n"
        << "  \"version\": \"1.0\",\n"
        << "  \"required\": [\"text_region_id\", \"raw_text\", \"confidence\"],\n"
        << "  \"confidence_values\": [\"high\", \"medium\", \"low\", \"unresolved\"],\n"
        << "  \"rules\": {\"one_observation_per_region\": true, "
        << "\"do_not_invent_text\": true, \"preserve_uncertainty\": true}\n"
        << "}\n";
}

} // namespace eke::dx::wire

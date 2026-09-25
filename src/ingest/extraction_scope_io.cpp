#include "eke_dx_wire/ingest/extraction_scope_io.hpp"

#include <opencv2/core.hpp>

#include <fstream>
#include <sstream>
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

void read_string(const cv::FileNode& node, std::string& target) {
    if (node.empty() || node.isNone()) {
        return;
    }
    node >> target;
}

void read_int(const cv::FileNode& node, int& target) {
    if (node.empty() || node.isNone()) {
        return;
    }
    node >> target;
}

std::vector<BoundingBox> read_regions(const cv::FileNode& node) {
    std::vector<BoundingBox> result;
    if (node.empty() || !node.isSeq()) {
        return result;
    }

    for (const auto& item : node) {
        if (!item.isMap()) {
            continue;
        }
        BoundingBox box;
        read_int(item["x"], box.x);
        read_int(item["y"], box.y);
        read_int(item["width"], box.width);
        read_int(item["height"], box.height);
        result.push_back(box);
    }

    return result;
}

std::vector<AnnotationRegion> read_annotation_regions(const cv::FileNode& node) {
    std::vector<AnnotationRegion> result;
    if (node.empty() || !node.isSeq()) {
        return result;
    }

    for (const auto& item : node) {
        if (!item.isMap()) {
            continue;
        }
        AnnotationRegion region;
        read_string(item["id"], region.id);
        read_int(item["x"], region.bounds.x);
        read_int(item["y"], region.bounds.y);
        read_int(item["width"], region.bounds.width);
        read_int(item["height"], region.bounds.height);
        read_string(item["annotation_type"], region.annotation_type);
        read_string(item["description"], region.description);
        result.push_back(std::move(region));
    }

    return result;
}

DiagramMetadata read_metadata(const cv::FileNode& node) {
    DiagramMetadata metadata;
    if (node.empty() || !node.isMap()) {
        return metadata;
    }

    read_string(node["diagram_type"], metadata.diagram_type);
    read_string(node["manufacturer"], metadata.manufacturer);
    read_string(node["make"], metadata.make);
    read_string(node["model"], metadata.model);
    read_string(node["year"], metadata.year);
    read_string(node["vehicle_type"], metadata.vehicle_type);
    read_string(node["system"], metadata.system);
    read_string(node["harness_branch"], metadata.harness_branch);
    read_string(node["diagram_section"], metadata.diagram_section);

    return metadata;
}

std::string serialize_region(const BoundingBox& box, const std::string& indent) {
    std::ostringstream out;
    out << indent << "{\"x\": " << box.x << ", \"y\": " << box.y
        << ", \"width\": " << box.width << ", \"height\": " << box.height
        << "}";
    return out.str();
}

std::string serialize_regions(
    const std::vector<BoundingBox>& regions, const std::string& indent) {

    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < regions.size(); ++i) {
        out << "\n" << indent << "  " << serialize_region(regions[i], "");
        if (i + 1 < regions.size()) {
            out << ",";
        }
    }
    if (!regions.empty()) {
        out << "\n" << indent;
    }
    out << "]";
    return out.str();
}

std::string serialize_annotation_regions(
    const std::vector<AnnotationRegion>& regions, const std::string& indent) {

    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < regions.size(); ++i) {
        const auto& region = regions[i];
        out << "\n" << indent << "  {\"id\": \"" << json_escape(region.id)
            << "\", \"x\": " << region.bounds.x
            << ", \"y\": " << region.bounds.y
            << ", \"width\": " << region.bounds.width
            << ", \"height\": " << region.bounds.height
            << ", \"annotation_type\": \""
            << json_escape(region.annotation_type)
            << "\", \"description\": \"" << json_escape(region.description)
            << "\"}";
        if (i + 1 < regions.size()) {
            out << ",";
        }
    }
    if (!regions.empty()) {
        out << "\n" << indent;
    }
    out << "]";
    return out.str();
}

std::string serialize_metadata(const DiagramMetadata& metadata) {
    std::ostringstream out;
    out << "{\n"
        << "    \"diagram_type\": \"" << json_escape(metadata.diagram_type) << "\",\n"
        << "    \"manufacturer\": \"" << json_escape(metadata.manufacturer) << "\",\n"
        << "    \"make\": \"" << json_escape(metadata.make) << "\",\n"
        << "    \"model\": \"" << json_escape(metadata.model) << "\",\n"
        << "    \"year\": \"" << json_escape(metadata.year) << "\",\n"
        << "    \"vehicle_type\": \"" << json_escape(metadata.vehicle_type) << "\",\n"
        << "    \"system\": \"" << json_escape(metadata.system) << "\",\n"
        << "    \"harness_branch\": \"" << json_escape(metadata.harness_branch) << "\",\n"
        << "    \"diagram_section\": \"" << json_escape(metadata.diagram_section) << "\"\n"
        << "  }";
    return out.str();
}

} // namespace

ExtractionScope ExtractionScopeIO::load(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Unable to open extraction scope JSON: " + path);
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    return parse(buffer.str());
}

ExtractionScope ExtractionScopeIO::parse(const std::string& json_text) {
    cv::FileStorage storage(
        json_text,
        cv::FileStorage::READ | cv::FileStorage::MEMORY |
            cv::FileStorage::FORMAT_JSON);

    if (!storage.isOpened()) {
        throw std::runtime_error("Unable to parse extraction scope JSON");
    }

    ExtractionScope scope;
    read_int(storage["schema_version"], scope.schema_version);

    const cv::FileNode source = storage["source"];
    if (!source.empty() && source.isMap()) {
        read_string(source["path"], scope.source_path);
        read_int(source["page"], scope.source_page);
    }

    scope.include_regions = read_regions(storage["include_regions"]);
    scope.exclusion_regions = read_regions(storage["exclusion_regions"]);
    scope.annotation_regions =
        read_annotation_regions(storage["annotation_regions"]);
    scope.metadata = read_metadata(storage["metadata"]);

    return scope;
}

void ExtractionScopeIO::save(
    const ExtractionScope& scope, const std::string& path) {

    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Unable to create extraction scope JSON: " + path);
    }
    out << serialize(scope);
}

std::string ExtractionScopeIO::serialize(const ExtractionScope& scope) {
    std::ostringstream out;
    out << "{\n"
        << "  \"schema_version\": " << scope.schema_version << ",\n"
        << "  \"source\": {\n"
        << "    \"path\": \"" << json_escape(scope.source_path) << "\",\n"
        << "    \"page\": " << scope.source_page << "\n"
        << "  },\n"
        << "  \"include_regions\": "
        << serialize_regions(scope.include_regions, "  ") << ",\n"
        << "  \"exclusion_regions\": "
        << serialize_regions(scope.exclusion_regions, "  ") << ",\n"
        << "  \"annotation_regions\": "
        << serialize_annotation_regions(scope.annotation_regions, "  ") << ",\n"
        << "  \"metadata\": " << serialize_metadata(scope.metadata) << "\n"
        << "}\n";
    return out.str();
}

} // namespace eke::dx::wire

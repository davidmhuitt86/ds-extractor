#include "eke_dx_wire/export/recognition_input_exporter.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <iostream>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

int main() {
    const fs::path root =
        fs::temp_directory_path() / "eke-dx-wire-recognition-export-test";
    fs::remove_all(root);
    fs::create_directories(root);

    const fs::path original =
        fs::path(DX_WIRE_SOURCE_DIR) / "samples" / "trx300ODG.png";
    const fs::path output = root / "package";

    std::cerr << "checkpoint: source path\n";
    if (!fs::exists(original) || !fs::is_regular_file(original)) {
        std::cerr << "source missing: " << original.string() << "\n";
        return 1;
    }

    std::cerr << "checkpoint: before imread\n";
    const cv::Mat image = cv::imread(original.string(), cv::IMREAD_GRAYSCALE);
    std::cerr << "checkpoint: after imread " << image.cols << "x" << image.rows << "\n";
    if (image.empty()) {
        return 1;
    }

    WireModel model;
    model.source_id = "fixture";
    model.page = 0;
    model.image_width = image.cols;
    model.image_height = image.rows;

    TextRegion region;
    region.id = "text-region-test";
    region.bounds = {20, 20, 20, 10};
    region.confidence = 0.9;
    model.text_regions.push_back(region);

    ComponentCandidate component;
    component.id = "component-test";
    component.bounds = {45, 20, 10, 10};
    component.confidence = ConfidenceClass::High;
    model.component_candidates.push_back(component);

    EndpointCandidate endpoint;
    endpoint.id = "endpoint-test";
    endpoint.position = {30.0, 25.0};
    endpoint.kind = EndpointKind::ComponentTerminal;
    endpoint.confidence = ConfidenceClass::Medium;
    model.endpoint_candidates.push_back(endpoint);

    std::cerr << "checkpoint: before export\n";
    RecognitionInputExporter::export_package(
        model, image, original.string(), output.string());
    std::cerr << "checkpoint: after export\n";

    std::cerr << "checkpoint: verify outputs\n";
    std::cerr << "checkpoint: validating manifest\n";
    if (!fs::exists(output / "manifest.json")) return 1;
    if (!fs::exists(output / "recognition_input.json")) return 1;
    if (!fs::exists(output / "instructions.md")) return 1;
    if (!fs::exists(output / "schema.json")) return 1;
    if (!fs::exists(output / "source_normalized.png")) return 1;
    if (!fs::exists(output / "source_original" / "trx300ODG.png")) return 1;
    if (!fs::exists(output / "regions" / "text-region-test.json")) return 1;
    if (!fs::exists(output / "regions" / "crops" / "text-region-test.png")) return 1;

    std::cerr << "checkpoint: outputs verified\n";
    std::cerr << "checkpoint: files validated\n";
    std::ifstream manifest(output / "manifest.json");
    std::cerr << "checkpoint: manifest opened\n";
    std::cerr << "checkpoint: reading manifest\n";
    const std::string contents(
        (std::istreambuf_iterator<char>(manifest)),
        std::istreambuf_iterator<char>());

    if (contents.find("eke-dx-wire-recognition-input") == std::string::npos) return 1;
    if (contents.find("text_region_count") == std::string::npos) return 1;
    std::cerr << "checkpoint: manifest read\n";
    std::cerr << "checkpoint: manifest verified\n";

    manifest.close();
    std::cerr << "checkpoint: manifest closed\n";
    fs::remove_all(root);
    std::cerr << "checkpoint: temp removed\n";
    return 0;
}

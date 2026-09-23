#include "eke_dx_wire/export/recognition_input_exporter.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

namespace {

bool require_path(const fs::path& path) {
    if (!fs::exists(path)) {
        std::cerr << "missing expected path: " << path << "\n";
        return false;
    }
    return true;
}

} // namespace

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
        std::cerr << "missing source image: " << original << "\n";
        return 1;
    }

    std::cerr << "checkpoint: before imread\n";
    const cv::Mat image = cv::imread(original.string(), cv::IMREAD_GRAYSCALE);
    std::cerr << "checkpoint: after imread "
              << image.cols << "x" << image.rows << "\n";
    if (image.empty()) {
        std::cerr << "unable to load source image\n";
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

    const fs::path expected_paths[] = {
        output / "manifest.json",
        output / "recognition_input.json",
        output / "instructions.md",
        output / "schema.json",
        output / "source_normalized.png",
        output / "source_original" / "trx300ODG.png",
        output / "regions" / "text-region-test.json",
        output / "regions" / "crops" / "text-region-test.png"
    };

    for (const auto& path : expected_paths) {
        if (!require_path(path)) {
            fs::remove_all(root);
            return 1;
        }
    }

    std::string contents;
    {
        std::ifstream manifest(output / "manifest.json");
        if (!manifest) {
            std::cerr << "unable to open manifest: "
                      << output / "manifest.json" << "\n";
            fs::remove_all(root);
            return 1;
        }

        contents.assign(
            (std::istreambuf_iterator<char>(manifest)),
            std::istreambuf_iterator<char>());
    }

    if (contents.find("eke-dx-wire-recognition-input") == std::string::npos ||
        contents.find("text_region_count") == std::string::npos) {
        std::cerr << "manifest content validation failed\n";
        fs::remove_all(root);
        return 1;
    }

    // Ensure all file handles are closed before cleanup. Windows refuses to
    // remove open files, making cleanup behavior platform-dependent.
    fs::remove_all(root);
    return 0;
}

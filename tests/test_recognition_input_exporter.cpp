#include "eke_dx_wire/export/recognition_input_exporter.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

int main() {
    const fs::path root =
        fs::temp_directory_path() / "eke-dx-wire-recognition-export-test";
    fs::remove_all(root);
    fs::create_directories(root);

    const fs::path original = root / "source.png";
    const fs::path output = root / "package";

    cv::Mat image = cv::Mat::zeros(80, 100, CV_8UC1);
    cv::rectangle(image, cv::Rect(20, 20, 20, 10), cv::Scalar(255), 1);
    assert(cv::imwrite(original.string(), image));

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

    RecognitionInputExporter::export_package(
        model, image, original.string(), output.string());

    assert(fs::exists(output / "manifest.json"));
    assert(fs::exists(output / "recognition_input.json"));
    assert(fs::exists(output / "instructions.md"));
    assert(fs::exists(output / "schema.json"));
    assert(fs::exists(output / "source_normalized.png"));
    assert(fs::exists(output / "source_original" / "source.png"));
    assert(fs::exists(output / "regions" / "text-region-test.json"));
    assert(fs::exists(output / "regions" / "crops" / "text-region-test.png"));

    std::ifstream manifest(output / "manifest.json");
    const std::string contents(
        (std::istreambuf_iterator<char>(manifest)),
        std::istreambuf_iterator<char>());

    assert(contents.find("eke-dx-wire-recognition-input") != std::string::npos);
    assert(contents.find("text_region_count") != std::string::npos);

    fs::remove_all(root);
    return 0;
}

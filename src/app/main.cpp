#include "eke_dx_wire/export/svg_exporter.hpp"
#include "eke_dx_wire/image/image_loader.hpp"
#include "eke_dx_wire/image/normalizer.hpp"
#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

static void usage() {
    std::cout
        << "dx-extract 0.1.0\n\n"
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
    fs::create_directories(fs::path(output) / "artifacts" / "validation");
    fs::create_directories(fs::path(output) / "output");

    ExtractionPipeline pipeline;
    WireModel model = pipeline.run(image_path, image_path);

    SvgExporter::export_segments(
        model,
        (fs::path(output) / "output" / "wires.svg").string());

    std::ofstream manifest(fs::path(output) / "project.json");
    manifest << "{\n"
             << "  \"format\": \"eke-dx-wire-project\",\n"
             << "  \"version\": \"0.1.0\",\n"
             << "  \"source\": \"" << image_path << "\",\n"
             << "  \"page\": " << model.page << ",\n"
             << "  \"image_width\": " << model.image_width << ",\n"
             << "  \"image_height\": " << model.image_height << ",\n"
             << "  \"segment_count\": " << model.segments.size() << "\n"
             << "}\n";

    std::cout << "extracted segments: " << model.segments.size() << "\n"
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

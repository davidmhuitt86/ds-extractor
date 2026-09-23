#include "eke_dx_wire/export/artifact_writer.hpp"
#include "eke_dx_wire/export/svg_exporter.hpp"
#include "eke_dx_wire/export/topology_exporter.hpp"
#include "eke_dx_wire/export/recognition_input_exporter.hpp"
#include "eke_dx_wire/topology/json_text_recognition_provider.hpp"
#include "eke_dx_wire/image/normalizer.hpp"
#include "eke_dx_wire/image/image_loader.hpp"
#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
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

static const char* distribution_role_name(DistributionRole value) {
    switch (value) {
    case DistributionRole::Ground: return "ground";
    case DistributionRole::PowerFeed: return "power_feed";
    case DistributionRole::SharedFunctionFeed: return "shared_function_feed";
    case DistributionRole::Unknown: return "unresolved";
    }
    return "unresolved";
}

static const char* confidence_name(ConfidenceClass value) {
    switch (value) {
    case ConfidenceClass::High: return "high";
    case ConfidenceClass::Medium: return "medium";
    case ConfidenceClass::Low: return "low";
    case ConfidenceClass::Unresolved: return "unresolved";
    }
    return "unresolved";
}

static void usage() {
    std::cout
        << "dx-extract 0.1.2\n\n"
        << "Usage:\n"
        << "  dx-extract inspect <image>\n"
        << "  dx-extract extract <image> --output <directory> [--recognition <observations.json>]\n";
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

static int extract(const std::string& image_path, const std::string& output, const std::string& recognition_path = {}) {
    // AP-WIRE-013: when recognition observations are supplied, execute
    // both the deterministic baseline and the recognition-assisted pipeline.
    // This makes semantic improvement measurable without allowing recognition
    // to alter the underlying geometry/topology extraction.
    std::optional<WireModel> baseline_model;
    if (!recognition_path.empty()) {
        ExtractionPipeline baseline_pipeline;
        baseline_model = baseline_pipeline.run(image_path, image_path);
    }

    ExtractionConfig config;
    if (!recognition_path.empty()) {
        config.text_recognition_provider =
            std::make_shared<JsonTextRecognitionProvider>(recognition_path);
    }

    ExtractionPipeline pipeline(config);
    WireModel model = pipeline.run(image_path, image_path);

    if (!recognition_path.empty()) {
        std::ofstream report(
            fs::path(output) / "artifacts" / "recognition" /
            "semantic_resolution_report.json");

        if (!report) {
            throw std::runtime_error(
                "Unable to create semantic resolution report");
        }

        report << "{\n"
               << "  \"format\": \"eke-dx-wire-semantic-resolution-report\",\n"
               << "  \"version\": \"1.0\",\n"
               << "  \"source\": \"" << json_escape(image_path) << "\",\n"
               << "  \"recognition_observations\": "
               << model.text_recognition_evidence.size() << ",\n"
               << "  \"semantic_observations\": "
               << model.text_semantic_evidence.size() << ",\n"
               << "  \"semantic_associations\": "
               << model.semantic_associations.size() << ",\n"
               << "  \"baseline_unresolved_nets\": "
               << baseline_model.value().audit.unresolved_nets << ",\n"
               << "  \"recognized_unresolved_nets\": "
               << model.audit.unresolved_nets << ",\n"
               << "  \"nets\": [\n";

        bool first_net = true;
        for (const auto& recognized_net : model.electrical_nets) {
            const auto baseline_it = std::find_if(
                baseline_model.value().electrical_nets.begin(),
                baseline_model.value().electrical_nets.end(),
                [&](const ElectricalNet& net) {
                    return net.id == recognized_net.id;
                });

            if (!first_net) {
                report << ",\n";
            }
            first_net = false;

            const DistributionRole baseline_role =
                baseline_it == baseline_model.value().electrical_nets.end()
                    ? DistributionRole::Unknown
                    : baseline_it->role;
            const ConfidenceClass baseline_confidence =
                baseline_it == baseline_model.value().electrical_nets.end()
                    ? ConfidenceClass::Unresolved
                    : baseline_it->confidence;

            report << "    {\"id\": \"" << json_escape(recognized_net.id)
                   << "\", \"baseline_role\": \""
                   << distribution_role_name(baseline_role)
                   << "\", \"baseline_confidence\": \""
                   << confidence_name(baseline_confidence)
                   << "\", \"recognized_role\": \""
                   << distribution_role_name(recognized_net.role)
                   << "\", \"recognized_confidence\": \""
                   << confidence_name(recognized_net.confidence)
                   << "\", \"changed\": "
                   << ((baseline_role != recognized_net.role ||
                        baseline_confidence != recognized_net.confidence)
                           ? "true" : "false")
                   << "}";
        }

        report << "\n  ]\n}\n";
    }

    ExtractionArtifactWriter::write(
        model,
        ImageNormalizer::normalize(ImageLoader::load(image_path)),
        image_path,
        fs::path(output));

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
            std::string recognition_path;
            if (argc == 7 && std::string(argv[5]) == "--recognition") {
                recognition_path = argv[6];
            } else if (argc != 5) {
                usage();
                return 2;
            }
            return extract(argv[2], argv[4], recognition_path);
        }

        usage();
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}

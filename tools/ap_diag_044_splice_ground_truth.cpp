#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

namespace {
struct GroundTruthPoint {
    const char* id;
    double x;
    double y;
};

const GroundTruthPoint kGroundTruth[] = {
    {"SPLICE-01", 422.8, 102.6},
    {"SPLICE-02", 624.6, 113.3},
    {"SPLICE-03", 458.1, 140.3},
    {"SPLICE-04", 470.6, 157.2},
    {"SPLICE-05", 423.1, 164.5},
    {"SPLICE-06", 510.9, 183.9},
    {"SPLICE-07", 87.2, 256.7},
    {"SPLICE-08", 424.9, 253.7},
    {"SPLICE-09", 541.9, 246.6},
    {"SPLICE-10", 506.8, 265.0},
    {"SPLICE-11", 640.6, 263.0},
    {"SPLICE-12", 530.4, 299.7},
    {"SPLICE-13", 178.8, 315.9},
    {"SPLICE-14", 167.9, 327.7},
    {"SPLICE-15", 154.8, 337.2},
    {"SPLICE-16", 471.3, 336.4},
    {"SPLICE-17", 489.6, 331.3},
    {"SPLICE-18", 648.5, 407.6},
    {"SPLICE-19", 243.1, 464.2}
};

constexpr std::size_t kGroundTruthCount =
    sizeof(kGroundTruth) / sizeof(kGroundTruth[0]);
constexpr double kAssociationThresholdPx = 6.0;

double distance(Point2D p, double x, double y) {
    const double dx = p.x - x;
    const double dy = p.y - y;
    return std::sqrt(dx * dx + dy * dy);
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

struct Residual {
    std::string endpoint_id;
    std::string splice_id;
};

std::vector<Residual> load_residuals(const fs::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error(
            "Unable to open AP-DIAG-042 report: " + path.string());
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();

    const std::regex pattern(
        R"rx("endpoint_id":"([^"]+)","class":"splice_stop","terminal_splices":\["([^"]+)")rx");

    std::vector<Residual> result;
    for (std::sregex_iterator it(text.begin(), text.end(), pattern), end;
         it != end;
         ++it) {
        result.push_back({(*it)[1].str(), (*it)[2].str()});
    }
    return result;
}

struct BestMatch {
    std::string id;
    double distance_px = std::numeric_limits<double>::infinity();
};

BestMatch nearest_endpoint(
    const WireModel& model,
    double x,
    double y) {
    BestMatch result;
    for (const auto& endpoint : model.endpoint_candidates) {
        const double d = distance(endpoint.position, x, y);
        if (d < result.distance_px) {
            result.id = endpoint.id;
            result.distance_px = d;
        }
    }
    return result;
}

BestMatch nearest_node(
    const WireModel& model,
    double x,
    double y) {
    BestMatch result;
    for (const auto& node : model.nodes) {
        const double d = distance(node.position, x, y);
        if (d < result.distance_px) {
            result.id = node.id;
            result.distance_px = d;
        }
    }
    return result;
}

BestMatch nearest_residual_node(
    const WireModel& model,
    const std::vector<Residual>& residuals,
    double x,
    double y) {
    BestMatch result;

    for (const auto& residual : residuals) {
        const auto node = std::find_if(
            model.nodes.begin(),
            model.nodes.end(),
            [&](const TopologyNode& candidate) {
                return candidate.id == residual.splice_id;
            });

        if (node == model.nodes.end())
            continue;

        const double d = distance(node->position, x, y);
        if (d < result.distance_px) {
            result.id = residual.splice_id;
            result.distance_px = d;
        }
    }

    return result;
}

void write_report(
    const fs::path& path,
    const WireModel& model,
    const std::vector<Residual>& residuals) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error(
            "Unable to write AP-DIAG-044 report: " + path.string());
    }

    output
        << "{\n"
        << "  \"schema_version\": 1,\n"
        << "  \"ap\": \"AP-DIAG-044\",\n"
        << "  \"status\": \"diagnostic_only\",\n"
        << "  \"production_logic_modified\": false,\n"
        << "  \"ground_truth\": {\n"
        << "    \"source_id\": \"" << json_escape(model.source_id) << "\",\n"
        << "    \"count\": " << kGroundTruthCount << ",\n"
        << "    \"association_threshold_px\": "
        << kAssociationThresholdPx << "\n"
        << "  },\n"
        << "  \"population\": {\n"
        << "    \"current_model_wires\": " << model.wires.size() << ",\n"
        << "    \"current_model_endpoints\": "
        << model.endpoint_candidates.size() << ",\n"
        << "    \"current_model_nodes\": " << model.nodes.size() << ",\n"
        << "    \"current_model_edges\": " << model.edges.size() << ",\n"
        << "    \"residual_splice_records\": "
        << residuals.size() << "\n"
        << "  },\n"
        << "  \"source_splices\": [\n";

    std::size_t covered = 0U;

    for (std::size_t i = 0; i < kGroundTruthCount; ++i) {
        const auto& ground_truth = kGroundTruth[i];
        const BestMatch endpoint =
            nearest_endpoint(model, ground_truth.x, ground_truth.y);
        const BestMatch node =
            nearest_node(model, ground_truth.x, ground_truth.y);
        const BestMatch residual =
            nearest_residual_node(
                model,
                residuals,
                ground_truth.x,
                ground_truth.y);

        const bool endpoint_match =
            endpoint.distance_px <= kAssociationThresholdPx;
        const bool residual_match =
            residual.distance_px <= kAssociationThresholdPx;

        if (residual_match)
            ++covered;

        output
            << "    {\n"
            << "      \"id\": \"" << ground_truth.id << "\",\n"
            << "      \"source_position\": {\"x\": "
            << ground_truth.x << ", \"y\": "
            << ground_truth.y << "},\n"
            << "      \"nearest_endpoint\": {\"id\": \""
            << json_escape(endpoint.id)
            << "\", \"distance_px\": "
            << endpoint.distance_px << "},\n"
            << "      \"nearest_topology_node\": {\"id\": \""
            << json_escape(node.id)
            << "\", \"distance_px\": "
            << node.distance_px << "},\n"
            << "      \"nearest_residual_node\": {\"id\": \""
            << json_escape(residual.id)
            << "\", \"distance_px\": "
            << residual.distance_px << "},\n"
            << "      \"endpoint_within_threshold\": "
            << (endpoint_match ? "true" : "false") << ",\n"
            << "      \"residual_within_threshold\": "
            << (residual_match ? "true" : "false") << "\n"
            << "    }";

        if (i + 1U != kGroundTruthCount)
            output << ",";
        output << "\n";
    }

    output
        << "  ],\n"
        << "  \"coverage\": {\n"
        << "    \"ground_truth_splices_with_residual\": "
        << covered << ",\n"
        << "    \"ground_truth_splices_without_residual\": "
        << (kGroundTruthCount - covered) << "\n"
        << "  },\n"
        << "  \"residuals\": [\n";

    std::size_t matched_residuals = 0U;

    for (std::size_t i = 0; i < residuals.size(); ++i) {
        const auto node = std::find_if(
            model.nodes.begin(),
            model.nodes.end(),
            [&](const TopologyNode& candidate) {
                return candidate.id == residuals[i].splice_id;
            });

        double best_distance = std::numeric_limits<double>::infinity();
        std::string nearest_ground_truth;

        if (node != model.nodes.end()) {
            for (const auto& ground_truth : kGroundTruth) {
                const double d =
                    distance(node->position, ground_truth.x, ground_truth.y);

                if (d < best_distance) {
                    best_distance = d;
                    nearest_ground_truth = ground_truth.id;
                }
            }
        }

        const bool matched =
            best_distance <= kAssociationThresholdPx;

        if (matched)
            ++matched_residuals;

        output
            << "    {\n"
            << "      \"endpoint_id\": \""
            << json_escape(residuals[i].endpoint_id) << "\",\n"
            << "      \"residual_node_id\": \""
            << json_escape(residuals[i].splice_id) << "\",\n"
            << "      \"nearest_ground_truth_splice\": \""
            << json_escape(nearest_ground_truth) << "\",\n"
            << "      \"distance_px\": "
            << best_distance << ",\n"
            << "      \"matches_ground_truth\": "
            << (matched ? "true" : "false") << "\n"
            << "    }";

        if (i + 1U != residuals.size())
            output << ",";
        output << "\n";
    }

    output
        << "  ],\n"
        << "  \"residual_summary\": {\n"
        << "    \"matched_to_ground_truth\": "
        << matched_residuals << ",\n"
        << "    \"not_matched_to_ground_truth\": "
        << (residuals.size() - matched_residuals) << "\n"
        << "  }\n"
        << "}\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 3) {
            std::cerr
                << "Usage: dx-audit-splice-ground-truth "
                << "<image> <output_dir>\n";
            return 2;
        }

        const fs::path image_path = argv[1];
        const fs::path output_dir = argv[2];

        fs::create_directories(output_dir);

        const fs::path ap042_path =
            output_dir / "AP-DIAG-042_exact_production_replay.json";

        const std::vector<Residual> residuals =
            load_residuals(ap042_path);

        if (residuals.size() != 35U) {
            throw std::runtime_error(
                "Expected 35 AP-DIAG-042 residual mappings; found " +
                std::to_string(residuals.size()) +
                ". Run AP-DIAG-042 against the current extraction first.");
        }

        const cv::Mat source =
            cv::imread(image_path.string(), cv::IMREAD_COLOR);

        if (source.empty()) {
            throw std::runtime_error(
                "Unable to load source image: " + image_path.string());
        }

        ExtractionPipeline pipeline;
        const WireModel model =
            pipeline.run(image_path.string(), image_path.string());

        if (model.image_width != 898 || model.image_height != 549) {
            throw std::runtime_error(
                "Ground-truth source dimensions must be 898x549; found " +
                std::to_string(model.image_width) + "x" +
                std::to_string(model.image_height));
        }

        if (model.wires.size() != 77U) {
            throw std::runtime_error(
                "Expected 77 production model wires; found " +
                std::to_string(model.wires.size()));
        }

        const fs::path report =
            output_dir / "AP-DIAG-044_splice_ground_truth.json";

        write_report(report, model, residuals);

        std::cout
            << "[AP-DIAG-044] Ground-truth splices     : "
            << kGroundTruthCount << "\n"
            << "[AP-DIAG-044] Current model wires      : "
            << model.wires.size() << "\n"
            << "[AP-DIAG-044] Current model endpoints  : "
            << model.endpoint_candidates.size() << "\n"
            << "[AP-DIAG-044] Current topology nodes   : "
            << model.nodes.size() << "\n"
            << "[AP-DIAG-044] Residual splice mappings : "
            << residuals.size() << "\n"
            << "[AP-DIAG-044] Report: "
            << report.string() << "\n"
            << "[AP-DIAG-044] Diagnostic only; no production source modified.\n";

        return 0;
    } catch (const std::exception& error) {
        std::cerr
            << "[AP-DIAG-044] ERROR: "
            << error.what() << "\n";
        return 1;
    }
}

#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

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
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

namespace {

struct Point {
    double x = 0.0;
    double y = 0.0;
};

struct DetectedMarker {
    Point annotation_position;
    double score = 0.0;
};

struct RegisteredMarker {
    std::string id;
    Point annotation_position;
    Point source_position;
    double detection_score = 0.0;
};

struct Residual {
    std::string endpoint_id;
    std::string splice_id;
};

struct BestMatch {
    std::string id;
    double distance_px = std::numeric_limits<double>::infinity();
};

constexpr std::size_t kGroundTruthCount = 19U;
constexpr std::size_t kExpectedResidualCount = 35U;
constexpr std::size_t kExpectedModelWires = 77U;
constexpr int kMarkerWindowPx = 19;
constexpr int kNmsRadiusPx = 13;
constexpr double kMinimumMarkerScore = 150.0;
constexpr double kAssociationThresholdPx = 6.0;
constexpr double kRegistrationSearchHalfRangePx = 32.0;
constexpr double kRegistrationScaleMin = 0.715;
constexpr double kRegistrationScaleMax = 0.735;
constexpr double kRegistrationScaleStep = 0.001;
constexpr double kMinimumRegistrationScore = 0.80;

double distance(Point a, Point b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
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

cv::Mat make_yellow_mask(const cv::Mat& image) {
    cv::Mat hsv;
    cv::cvtColor(image, hsv, cv::COLOR_BGR2HSV);

    cv::Mat mask;
    cv::inRange(
        hsv,
        cv::Scalar(20, 120, 120),
        cv::Scalar(40, 255, 255),
        mask);

    const cv::Mat kernel =
        cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel);
    return mask;
}

std::vector<DetectedMarker> detect_markers(const cv::Mat& annotation) {
    const cv::Mat mask = make_yellow_mask(annotation);

    cv::Mat scores;
    cv::boxFilter(
        mask,
        scores,
        CV_32F,
        cv::Size(kMarkerWindowPx, kMarkerWindowPx),
        cv::Point(-1, -1),
        false,
        cv::BORDER_CONSTANT);

    std::vector<cv::Point> candidates;
    candidates.reserve(static_cast<std::size_t>(scores.rows * scores.cols / 100));

    for (int y = kMarkerWindowPx / 2;
         y < scores.rows - kMarkerWindowPx / 2;
         ++y) {
        for (int x = kMarkerWindowPx / 2;
             x < scores.cols - kMarkerWindowPx / 2;
             ++x) {
            const float score = scores.at<float>(y, x);
            if (score >= static_cast<float>(kMinimumMarkerScore))
                candidates.emplace_back(x, y);
        }
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [&](const cv::Point& a, const cv::Point& b) {
            return scores.at<float>(a.y, a.x) >
                   scores.at<float>(b.y, b.x);
        });

    std::vector<DetectedMarker> selected;
    selected.reserve(kGroundTruthCount);

    for (const cv::Point candidate : candidates) {
        bool suppressed = false;
        for (const auto& marker : selected) {
            const double dx =
                static_cast<double>(candidate.x) -
                marker.annotation_position.x;
            const double dy =
                static_cast<double>(candidate.y) -
                marker.annotation_position.y;
            if (std::sqrt(dx * dx + dy * dy) <=
                static_cast<double>(kNmsRadiusPx)) {
                suppressed = true;
                break;
            }
        }

        if (suppressed)
            continue;

        selected.push_back({
            {static_cast<double>(candidate.x),
             static_cast<double>(candidate.y)},
            static_cast<double>(scores.at<float>(candidate.y, candidate.x))
        });

        if (selected.size() == kGroundTruthCount)
            break;
    }

    if (selected.size() != kGroundTruthCount) {
        throw std::runtime_error(
            "Yellow-marker detector found " +
            std::to_string(selected.size()) +
            " markers; expected exactly 19.");
    }

    return selected;
}

struct Registration {
    double scale = 0.0;
    double tx = 0.0;
    double ty = 0.0;
    double score = 0.0;
};

Registration register_annotation(
    const cv::Mat& source,
    const cv::Mat& annotation) {

    cv::Mat source_gray;
    cv::cvtColor(source, source_gray, cv::COLOR_BGR2GRAY);

    cv::Mat annotation_clean = annotation.clone();
    const cv::Mat yellow_mask = make_yellow_mask(annotation_clean);
    annotation_clean.setTo(cv::Scalar(255, 255, 255), yellow_mask);

    cv::Mat annotation_gray;
    cv::cvtColor(annotation_clean, annotation_gray, cv::COLOR_BGR2GRAY);

    const int pad = static_cast<int>(
        std::ceil(kRegistrationSearchHalfRangePx)) + 8;

    cv::Mat padded_source;
    cv::copyMakeBorder(
        source_gray,
        padded_source,
        pad,
        pad,
        pad,
        pad,
        cv::BORDER_CONSTANT,
        cv::Scalar(255));

    Registration best;

    for (double scale = kRegistrationScaleMin;
         scale <= kRegistrationScaleMax + 0.0001;
         scale += kRegistrationScaleStep) {

        cv::Mat resized;
        cv::resize(
            annotation_gray,
            resized,
            cv::Size(),
            scale,
            scale,
            cv::INTER_AREA);

        if (resized.cols > padded_source.cols ||
            resized.rows > padded_source.rows) {
            continue;
        }

        cv::Mat result;
        cv::matchTemplate(
            padded_source,
            resized,
            result,
            cv::TM_CCOEFF_NORMED);

        double min_value = 0.0;
        double max_value = 0.0;
        cv::Point min_location;
        cv::Point max_location;
        cv::minMaxLoc(
            result,
            &min_value,
            &max_value,
            &min_location,
            &max_location);

        if (max_value > best.score) {
            best.scale = scale;
            best.tx =
                static_cast<double>(max_location.x - pad);
            best.ty =
                static_cast<double>(max_location.y - pad);
            best.score = max_value;
        }
    }

    if (best.score < kMinimumRegistrationScore) {
        throw std::runtime_error(
            "Annotation-to-source registration score " +
            std::to_string(best.score) +
            " is below the required minimum " +
            std::to_string(kMinimumRegistrationScore) + ".");
    }

    return best;
}

Point map_to_source(const Registration& registration, Point annotation) {
    return {
        registration.scale * annotation.x + registration.tx,
        registration.scale * annotation.y + registration.ty
    };
}

BestMatch nearest_endpoint(
    const WireModel& model,
    Point point) {

    BestMatch result;
    for (const auto& endpoint : model.endpoint_candidates) {
        const Point candidate{endpoint.position.x, endpoint.position.y};
        const double d = distance(candidate, point);
        if (d < result.distance_px) {
            result.id = endpoint.id;
            result.distance_px = d;
        }
    }
    return result;
}

BestMatch nearest_node(
    const WireModel& model,
    Point point) {

    BestMatch result;
    for (const auto& node : model.nodes) {
        const Point candidate{node.position.x, node.position.y};
        const double d = distance(candidate, point);
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
    Point point) {

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

        const Point candidate{node->position.x, node->position.y};
        const double d = distance(candidate, point);
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
    const std::vector<Residual>& residuals,
    const std::vector<RegisteredMarker>& markers,
    const Registration& registration) {

    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error(
            "Unable to write AP-DIAG-044 report: " + path.string());
    }

    output
        << "{\n"
        << "  \"schema_version\": 2,\n"
        << "  \"ap\": \"AP-DIAG-044\",\n"
        << "  \"status\": \"diagnostic_only\",\n"
        << "  \"production_logic_modified\": false,\n"
        << "  \"annotation_provenance\": {\n"
        << "    \"marker_source\": \"external_annotated_diagram\",\n"
        << "    \"marker_count\": " << markers.size() << ",\n"
        << "    \"registration_scale\": " << registration.scale << ",\n"
        << "    \"registration_translation\": {\"x\": "
        << registration.tx << ", \"y\": " << registration.ty << "},\n"
        << "    \"registration_score\": " << registration.score << "\n"
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

    for (std::size_t i = 0; i < markers.size(); ++i) {
        const auto& marker = markers[i];
        const BestMatch endpoint =
            nearest_endpoint(model, marker.source_position);
        const BestMatch node =
            nearest_node(model, marker.source_position);
        const BestMatch residual =
            nearest_residual_node(
                model,
                residuals,
                marker.source_position);

        const bool endpoint_match =
            endpoint.distance_px <= kAssociationThresholdPx;
        const bool residual_match =
            residual.distance_px <= kAssociationThresholdPx;

        if (residual_match)
            ++covered;

        output
            << "    {\n"
            << "      \"id\": \""
            << marker.id << "\",\n"
            << "      \"annotation_position\": {\"x\": "
            << marker.annotation_position.x << ", \"y\": "
            << marker.annotation_position.y << "},\n"
            << "      \"source_position\": {\"x\": "
            << marker.source_position.x << ", \"y\": "
            << marker.source_position.y << "},\n"
            << "      \"detection_score\": "
            << marker.detection_score << ",\n"
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

        if (i + 1U != markers.size())
            output << ",";
        output << "\n";
    }

    output
        << "  ],\n"
        << "  \"coverage\": {\n"
        << "    \"ground_truth_splices_with_residual\": "
        << covered << ",\n"
        << "    \"ground_truth_splices_without_residual\": "
        << (markers.size() - covered) << "\n"
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
            const Point residual_point{
                node->position.x,
                node->position.y
            };

            for (const auto& marker : markers) {
                const double d =
                    distance(residual_point, marker.source_position);

                if (d < best_distance) {
                    best_distance = d;
                    nearest_ground_truth = marker.id;
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
        if (argc < 4) {
            std::cerr
                << "Usage: dx-audit-splice-ground-truth "
                << "<source_image> <annotated_image> <output_dir>\n";
            return 2;
        }

        const fs::path source_path = argv[1];
        const fs::path annotation_path = argv[2];
        const fs::path output_dir = argv[3];

        fs::create_directories(output_dir);

        const fs::path ap042_path =
            output_dir / "AP-DIAG-042_exact_production_replay.json";

        const std::vector<Residual> residuals =
            load_residuals(ap042_path);

        if (residuals.size() != kExpectedResidualCount) {
            throw std::runtime_error(
                "Expected 35 AP-DIAG-042 residual mappings; found " +
                std::to_string(residuals.size()) +
                ". Run AP-DIAG-042 against the current extraction first.");
        }

        const cv::Mat source =
            cv::imread(source_path.string(), cv::IMREAD_COLOR);
        const cv::Mat annotation =
            cv::imread(annotation_path.string(), cv::IMREAD_COLOR);

        if (source.empty()) {
            throw std::runtime_error(
                "Unable to load source image: " + source_path.string());
        }
        if (annotation.empty()) {
            throw std::runtime_error(
                "Unable to load annotated image: " +
                annotation_path.string());
        }

        ExtractionPipeline pipeline;
        const WireModel model =
            pipeline.run(source_path.string(), source_path.string());

        if (model.image_width != 898 || model.image_height != 549) {
            throw std::runtime_error(
                "Ground-truth source dimensions must be 898x549; found " +
                std::to_string(model.image_width) + "x" +
                std::to_string(model.image_height));
        }

        if (model.wires.size() != kExpectedModelWires) {
            throw std::runtime_error(
                "Expected 77 production model wires; found " +
                std::to_string(model.wires.size()));
        }

        const std::vector<DetectedMarker> detected =
            detect_markers(annotation);

        const Registration registration =
            register_annotation(source, annotation);

        std::vector<RegisteredMarker> markers;
        markers.reserve(detected.size());

        for (std::size_t i = 0; i < detected.size(); ++i) {
            markers.push_back({
                "SPLICE-" +
                    (i + 1U < 10U ? "0" : "") +
                    std::to_string(i + 1U),
                detected[i].annotation_position,
                map_to_source(
                    registration,
                    detected[i].annotation_position),
                detected[i].score
            });
        }

        std::sort(
            markers.begin(),
            markers.end(),
            [](const RegisteredMarker& a, const RegisteredMarker& b) {
                if (a.source_position.y != b.source_position.y)
                    return a.source_position.y < b.source_position.y;
                return a.source_position.x < b.source_position.x;
            });

        for (std::size_t i = 0; i < markers.size(); ++i) {
            markers[i].id =
                "SPLICE-" +
                (i + 1U < 10U ? "0" : "") +
                std::to_string(i + 1U);
        }

        const fs::path report =
            output_dir / "AP-DIAG-044_splice_ground_truth.json";

        write_report(
            report,
            model,
            residuals,
            markers,
            registration);

        std::cout
            << "[AP-DIAG-044] Annotated markers detected : "
            << markers.size() << "\n"
            << "[AP-DIAG-044] Registration scale         : "
            << registration.scale << "\n"
            << "[AP-DIAG-044] Registration translation    : ("
            << registration.tx << ", " << registration.ty << ")\n"
            << "[AP-DIAG-044] Registration score          : "
            << registration.score << "\n"
            << "[AP-DIAG-044] Current model wires         : "
            << model.wires.size() << "\n"
            << "[AP-DIAG-044] Current model endpoints     : "
            << model.endpoint_candidates.size() << "\n"
            << "[AP-DIAG-044] Current topology nodes      : "
            << model.nodes.size() << "\n"
            << "[AP-DIAG-044] Residual splice mappings    : "
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

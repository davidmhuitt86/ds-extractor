#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

namespace {

struct GroundTruth {
    std::string id;
    Point2D position {};
};

struct BestNode {
    const TopologyNode* node = nullptr;
    double distance = std::numeric_limits<double>::infinity();
};

struct BestEndpoint {
    const EndpointCandidate* endpoint = nullptr;
    double distance = std::numeric_limits<double>::infinity();
};

struct BestObject {
    bool matched = false;
    std::string id;
    double distance = std::numeric_limits<double>::infinity();
};

struct InkEvidence {
    double center_5x5 = 0.0;
    double local_11x11 = 0.0;
    double horizontal_support = 0.0;
    double vertical_support = 0.0;
};

constexpr std::size_t kExpectedMissed = 9U;
constexpr double kAssociationPx = 6.0;
constexpr double kObjectDistancePx = 12.0;

double point_distance(Point2D a, Point2D b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

std::string json_escape(const std::string& value) {
    std::string out;
    for (const char ch : value) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += ch; break;
        }
    }
    return out;
}

std::vector<GroundTruth> load_missed_ground_truth(const fs::path& path) {
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error(
            "Unable to open AP-DIAG-044 report: " + path.string());

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();

    const std::regex record(
        R"rx("id"\s*:\s*"(SPLICE-[0-9]+)"[\s\S]*?"source_position"\s*:\s*\{\s*"x"\s*:\s*([0-9eE+.-]+)\s*,\s*"y"\s*:\s*([0-9eE+.-]+)\s*\}[\s\S]*?"residual_within_threshold"\s*:\s*(true|false))rx");

    std::vector<GroundTruth> result;
    for (std::sregex_iterator it(text.begin(), text.end(), record), end;
         it != end;
         ++it) {
        if ((*it)[4].str() != "false")
            continue;

        result.push_back({
            (*it)[1].str(),
            {std::stod((*it)[2].str()), std::stod((*it)[3].str())}
        });
    }

    if (result.size() != kExpectedMissed) {
        throw std::runtime_error(
            "Expected exactly 9 missed AP-DIAG-044 ground-truth splices; found " +
            std::to_string(result.size()));
    }

    return result;
}

BestNode nearest_node(const WireModel& model, Point2D p) {
    BestNode result;
    for (const auto& node : model.nodes) {
        const double d = point_distance(node.position, p);
        if (d < result.distance) {
            result.node = &node;
            result.distance = d;
        }
    }
    return result;
}

BestEndpoint nearest_endpoint(const WireModel& model, Point2D p) {
    BestEndpoint result;
    for (const auto& endpoint : model.endpoint_candidates) {
        const double d = point_distance(endpoint.position, p);
        if (d < result.distance) {
            result.endpoint = &endpoint;
            result.distance = d;
        }
    }
    return result;
}

double box_distance(const BoundingBox& box, Point2D p) {
    const double left = static_cast<double>(box.x);
    const double top = static_cast<double>(box.y);
    const double right = left + box.width;
    const double bottom = top + box.height;

    const double dx = std::max({left - p.x, 0.0, p.x - right});
    const double dy = std::max({top - p.y, 0.0, p.y - bottom});
    return std::hypot(dx, dy);
}

BestObject nearest_component(const WireModel& model, Point2D p) {
    BestObject result;
    for (const auto& component : model.component_candidates) {
        const double d = box_distance(component.bounds, p);
        if (d < result.distance) {
            result.matched = d <= kObjectDistancePx;
            result.id = component.id;
            result.distance = d;
        }
    }
    return result;
}

BestObject nearest_connector(const WireModel& model, Point2D p) {
    BestObject result;
    for (const auto& connector : model.connector_candidates) {
        const double d = box_distance(connector.bounds, p);
        if (d < result.distance) {
            result.matched = d <= kObjectDistancePx;
            result.id = connector.id;
            result.distance = d;
        }
    }
    return result;
}

std::vector<const TopologyEdge*> incident_edges(
    const WireModel& model,
    const TopologyNode& node) {

    std::vector<const TopologyEdge*> result;
    for (const auto& edge : model.edges) {
        if (edge.from_node == node.id || edge.to_node == node.id)
            result.push_back(&edge);
    }
    return result;
}

std::map<std::string, std::size_t> segment_counts(
    const std::vector<const TopologyEdge*>& edges) {

    std::map<std::string, std::size_t> counts;
    for (const auto* edge : edges) {
        if (!edge->conductor_segment.empty())
            ++counts[edge->conductor_segment];
    }
    return counts;
}

InkEvidence measure_ink(const cv::Mat& gray, Point2D p) {
    const int cx = static_cast<int>(std::lround(p.x));
    const int cy = static_cast<int>(std::lround(p.y));

    auto density = [&](int radius) {
        const int left = std::max(0, cx - radius);
        const int top = std::max(0, cy - radius);
        const int right = std::min(gray.cols, cx + radius + 1);
        const int bottom = std::min(gray.rows, cy + radius + 1);

        if (right <= left || bottom <= top)
            return 0.0;

        const cv::Mat roi =
            gray(cv::Rect(left, top, right - left, bottom - top));

        return static_cast<double>(cv::countNonZero(roi < 180)) /
               static_cast<double>(roi.total());
    };

    const int left = std::max(0, cx - 8);
    const int top = std::max(0, cy - 8);
    const int right = std::min(gray.cols, cx + 9);
    const int bottom = std::min(gray.rows, cy + 9);

    double horizontal_pixels = 0.0;
    double vertical_pixels = 0.0;

    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            if (gray.at<unsigned char>(y, x) >= 180)
                continue;

            if (std::abs(y - cy) <= 1)
                horizontal_pixels += 1.0;

            if (std::abs(x - cx) <= 1)
                vertical_pixels += 1.0;
        }
    }

    return {
        density(2),
        density(5),
        horizontal_pixels / 51.0,
        vertical_pixels / 51.0
    };
}

std::string node_type_name(TopologyNodeType type) {
    switch (type) {
    case TopologyNodeType::ConductorEnd: return "conductor_end";
    case TopologyNodeType::Continuation: return "continuation";
    case TopologyNodeType::Junction: return "junction";
    case TopologyNodeType::Splice: return "splice";
    case TopologyNodeType::Crossing: return "crossing";
    case TopologyNodeType::ComponentBoundary: return "component_boundary";
    default: return "unresolved";
    }
}

std::string endpoint_kind_name(EndpointKind kind) {
    switch (kind) {
    case EndpointKind::GeometricConductorEnd: return "geometric_conductor_end";
    case EndpointKind::ComponentTerminal: return "component_terminal";
    case EndpointKind::ConnectorTerminal: return "connector_terminal";
    case EndpointKind::Splice: return "splice";
    case EndpointKind::Ground: return "ground";
    case EndpointKind::ExternalConnection: return "external_connection";
    default: return "unresolved";
    }
}

std::string terminal_role_name(TerminalRole role) {
    switch (role) {
    case TerminalRole::ComponentTerminal: return "component_terminal";
    case TerminalRole::ConnectorTerminal: return "connector_terminal";
    case TerminalRole::GroundTerminal: return "ground_terminal";
    case TerminalRole::PowerSource: return "power_source";
    case TerminalRole::ExternalConnection: return "external_connection";
    default: return "unknown";
    }
}

std::string failure_signature(
    const BestNode& node,
    const BestEndpoint& endpoint,
    std::size_t degree,
    const InkEvidence& ink) {

    if (node.distance > kAssociationPx)
        return "NO_TOPOLOGY_NODE_WITHIN_ASSOCIATION_THRESHOLD";

    if (node.node->type != TopologyNodeType::Splice)
        return "TOPOLOGY_NODE_PRESENT_BUT_NOT_SPLICE";

    if (degree < 3U)
        return "SPLICE_NODE_WITH_LOW_TOPOLOGY_DEGREE";

    if (endpoint.distance > kAssociationPx)
        return "SPLICE_NODE_WITHOUT_NEARBY_ENDPOINT";

    if (ink.center_5x5 < 0.10)
        return "SPLICE_NODE_WITH_WEAK_LOCAL_INK";

    return "SPLICE_NODE_PRESENT_BUT_NOT_REPRESENTED_AS_RESIDUAL";
}

void write_crop(
    const cv::Mat& source,
    Point2D p,
    const fs::path& path) {

    const int cx = static_cast<int>(std::lround(p.x));
    const int cy = static_cast<int>(std::lround(p.y));
    constexpr int pad = 36;

    const int left = std::max(0, cx - pad);
    const int top = std::max(0, cy - pad);
    const int right = std::min(source.cols, cx + pad + 1);
    const int bottom = std::min(source.rows, cy + pad + 1);

    cv::Mat crop =
        source(cv::Rect(left, top, right - left, bottom - top)).clone();

    cv::resize(crop, crop, cv::Size(), 5.0, 5.0, cv::INTER_NEAREST);

    cv::circle(
        crop,
        {(cx - left) * 5, (cy - top) * 5},
        12,
        cv::Scalar(0, 0, 255),
        2,
        cv::LINE_AA);

    if (!cv::imwrite(path.string(), crop))
        throw std::runtime_error("Unable to write crop: " + path.string());
}

void write_contact_sheet(
    const std::vector<fs::path>& crops,
    const fs::path& path) {

    constexpr int tile_width = 260;
    constexpr int tile_height = 260;
    constexpr int columns = 3;

    const int rows =
        static_cast<int>((crops.size() + columns - 1U) / columns);

    cv::Mat sheet(
        rows * tile_height,
        columns * tile_width,
        CV_8UC3,
        cv::Scalar(255, 255, 255));

    for (std::size_t i = 0; i < crops.size(); ++i) {
        cv::Mat image =
            cv::imread(crops[i].string(), cv::IMREAD_COLOR);

        if (image.empty())
            throw std::runtime_error(
                "Unable to read crop: " + crops[i].string());

        cv::resize(
            image,
            image,
            cv::Size(tile_width, tile_height));

        image.copyTo(
            sheet(cv::Rect(
                static_cast<int>(i % columns) * tile_width,
                static_cast<int>(i / columns) * tile_height,
                tile_width,
                tile_height)));
    }

    if (!cv::imwrite(path.string(), sheet))
        throw std::runtime_error(
            "Unable to write contact sheet: " + path.string());
}

void write_report(
    const fs::path& path,
    const WireModel& model,
    const std::vector<GroundTruth>& records,
    const cv::Mat& gray) {

    std::ofstream out(path);
    if (!out)
        throw std::runtime_error(
            "Unable to write AP-DIAG-045 report: " + path.string());

    out
        << "{\n"
        << "  \"schema_version\": 1,\n"
        << "  \"ap\": \"AP-DIAG-045\",\n"
        << "  \"status\": \"diagnostic_only\",\n"
        << "  \"production_logic_modified\": false,\n"
        << "  \"ground_truth_population\": {\n"
        << "    \"expected_missed_splices\": " << records.size() << ",\n"
        << "    \"source_model_wires\": " << model.wires.size() << ",\n"
        << "    \"source_model_endpoints\": "
        << model.endpoint_candidates.size() << ",\n"
        << "    \"source_model_nodes\": " << model.nodes.size() << ",\n"
        << "    \"source_model_edges\": " << model.edges.size() << "\n"
        << "  },\n"
        << "  \"records\": [\n";

    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto& gt = records[i];

        const BestNode node = nearest_node(model, gt.position);
        const BestEndpoint endpoint =
            nearest_endpoint(model, gt.position);
        const BestObject component =
            nearest_component(model, gt.position);
        const BestObject connector =
            nearest_connector(model, gt.position);
        const InkEvidence ink =
            measure_ink(gray, gt.position);

        std::vector<const TopologyEdge*> edges;
        std::map<std::string, std::size_t> counts;

        if (node.node != nullptr) {
            edges = incident_edges(model, *node.node);
            counts = segment_counts(edges);
        }

        std::size_t repeated = 0U;
        for (const auto& [segment, count] : counts) {
            (void)segment;
            if (count > 1U)
                repeated += count;
        }

        const std::string signature =
            failure_signature(
                node,
                endpoint,
                edges.size(),
                ink);

        out
            << "    {\n"
            << "      \"id\": \"" << json_escape(gt.id) << "\",\n"
            << "      \"source_position\": {\"x\": "
            << gt.position.x << ", \"y\": "
            << gt.position.y << "},\n"
            << "      \"nearest_topology_node\": {\n"
            << "        \"id\": \""
            << (node.node ? json_escape(node.node->id) : "")
            << "\",\n"
            << "        \"distance_px\": " << node.distance << ",\n"
            << "        \"type\": \""
            << (node.node ? node_type_name(node.node->type) : "none")
            << "\",\n"
            << "        \"degree\": " << edges.size() << ",\n"
            << "        \"unique_conductor_segments\": "
            << counts.size() << ",\n"
            << "        \"repeated_segment_incidents\": "
            << repeated << "\n"
            << "      },\n"
            << "      \"nearest_endpoint\": {\n"
            << "        \"id\": \""
            << (endpoint.endpoint
                    ? json_escape(endpoint.endpoint->id)
                    : "")
            << "\",\n"
            << "        \"distance_px\": "
            << endpoint.distance << ",\n"
            << "        \"kind\": \""
            << (endpoint.endpoint
                    ? endpoint_kind_name(endpoint.endpoint->kind)
                    : "none")
            << "\",\n"
            << "        \"terminal_role\": \""
            << (endpoint.endpoint
                    ? terminal_role_name(endpoint.endpoint->terminal_role)
                    : "none")
            << "\",\n"
            << "        \"incident_edges\": "
            << (endpoint.endpoint
                    ? endpoint.endpoint->incident_edges.size()
                    : 0U)
            << "\n"
            << "      },\n"
            << "      \"nearest_component\": {\"matched\": "
            << (component.matched ? "true" : "false")
            << ", \"id\": \"" << json_escape(component.id)
            << "\", \"distance_px\": " << component.distance
            << "},\n"
            << "      \"nearest_connector\": {\"matched\": "
            << (connector.matched ? "true" : "false")
            << ", \"id\": \"" << json_escape(connector.id)
            << "\", \"distance_px\": " << connector.distance
            << "},\n"
            << "      \"ink\": {\"center_5x5\": "
            << ink.center_5x5
            << ", \"local_11x11\": " << ink.local_11x11
            << ", \"horizontal_support\": "
            << ink.horizontal_support
            << ", \"vertical_support\": "
            << ink.vertical_support << "},\n"
            << "      \"failure_signature\": \""
            << signature << "\"\n"
            << "    }";

        if (i + 1U != records.size())
            out << ",";

        out << "\n";
    }

    out << "  ]\n}\n";
}

int run(int argc, char** argv) {
    if (argc < 3) {
        std::cerr
            << "Usage: dx-audit-missed-splice-analysis "
            << "<image> <output_dir>\n";
        return 2;
    }

    const fs::path image_path = argv[1];
    const fs::path output_dir = argv[2];
    const fs::path ap044_path =
        output_dir / "AP-DIAG-044_splice_ground_truth.json";
    const fs::path report_path =
        output_dir / "AP-DIAG-045_missed_splice_analysis.json";
    const fs::path crop_dir =
        fs::path("artifacts") / "missed_splice_analysis";

    fs::create_directories(output_dir);
    fs::create_directories(crop_dir);

    const std::vector<GroundTruth> records =
        load_missed_ground_truth(ap044_path);

    const cv::Mat source =
        cv::imread(image_path.string(), cv::IMREAD_COLOR);

    if (source.empty())
        throw std::runtime_error(
            "Unable to load source image: " + image_path.string());

    cv::Mat gray;
    cv::cvtColor(source, gray, cv::COLOR_BGR2GRAY);

    ExtractionPipeline pipeline;
    const WireModel model =
        pipeline.run(image_path.string(), image_path.string());

    if (model.image_width != 898 || model.image_height != 549)
        throw std::runtime_error(
            "Expected canonical 898x549 source dimensions.");

    if (model.wires.size() != 77U)
        throw std::runtime_error(
            "Expected 77 production model wires; found " +
            std::to_string(model.wires.size()));

    std::vector<fs::path> crops;
    crops.reserve(records.size());

    for (std::size_t i = 0; i < records.size(); ++i) {
        const fs::path crop =
            crop_dir /
            (std::string("missed-") +
             (i < 9U ? "00" : "0") +
             std::to_string(i + 1U) +
             ".png");

        write_crop(source, records[i].position, crop);
        crops.push_back(crop);
    }

    write_report(
        report_path,
        model,
        records,
        gray);

    write_contact_sheet(
        crops,
        crop_dir / "AP-DIAG-045_contact_sheet.png");

    std::cout
        << "[AP-DIAG-045] Missed ground-truth splices : "
        << records.size() << "\n"
        << "[AP-DIAG-045] Current model wires         : "
        << model.wires.size() << "\n"
        << "[AP-DIAG-045] Current model endpoints     : "
        << model.endpoint_candidates.size() << "\n"
        << "[AP-DIAG-045] Current topology nodes      : "
        << model.nodes.size() << "\n"
        << "[AP-DIAG-045] Report: "
        << report_path.string() << "\n"
        << "[AP-DIAG-045] Contact sheet: "
        << (crop_dir / "AP-DIAG-045_contact_sheet.png").string()
        << "\n"
        << "[AP-DIAG-045] Diagnostic only; no production source modified.\n";

    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr
            << "[AP-DIAG-045] ERROR: "
            << error.what() << "\n";
        return 1;
    }
}

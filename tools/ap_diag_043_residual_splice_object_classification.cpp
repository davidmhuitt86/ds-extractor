#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace eke::dx::wire {
namespace {

struct ReplayMapping {
    std::string endpoint_id;
    std::string splice_id;
};

struct InkMetrics {
    double center_density_5x5 = 0.0;
    double center_density_9x9 = 0.0;
    double local_density_17x17 = 0.0;
    double horizontal_support = 0.0;
    double vertical_support = 0.0;
    double axis_support = 0.0;
};

struct ObjectMatch {
    bool matched = false;
    double distance = 0.0;
    std::string id;
    std::string kind;
};

struct ClassificationRecord {
    std::string endpoint_id;
    std::string splice_id;
    std::string node_type;
    double node_x = 0.0;
    double node_y = 0.0;
    std::size_t degree = 0U;
    std::size_t unique_segments = 0U;
    std::size_t repeated_segment_incidents = 0U;
    ObjectMatch nearest_component;
    ObjectMatch nearest_connector;
    InkMetrics ink;
    std::string classification;
    std::vector<std::string> basis;
};

struct Summary {
    std::size_t total = 0U;
    std::size_t connector_body = 0U;
    std::size_t component_body = 0U;
    std::size_t ambiguous_object = 0U;
    std::size_t crossing_candidate = 0U;
    std::size_t real_splice_candidate = 0U;
    std::size_t undetermined = 0U;
};

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

double point_to_rect_distance(Point2D p, const BoundingBox& r) {
    const double left = r.x;
    const double right = r.x + r.width;
    const double top = r.y;
    const double bottom = r.y + r.height;

    const bool inside =
        p.x >= left && p.x <= right && p.y >= top && p.y <= bottom;

    if (inside) {
        return (std::min)(
            std::min(p.x - left, right - p.x),
            std::min(p.y - top, bottom - p.y));
    }

    const double dx =
        p.x < left ? left - p.x :
        p.x > right ? p.x - right : 0.0;
    const double dy =
        p.y < top ? top - p.y :
        p.y > bottom ? p.y - bottom : 0.0;

    return std::sqrt(dx * dx + dy * dy);
}

double dark_density(
    const cv::Mat& gray,
    int cx,
    int cy,
    int half_width) {

    const int left = (std::max)(0, cx - half_width);
    const int right = (std::min)(gray.cols - 1, cx + half_width);
    const int top = (std::max)(0, cy - half_width);
    const int bottom = (std::min)(gray.rows - 1, cy + half_width);

    std::size_t dark = 0U;
    std::size_t total = 0U;

    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            ++total;
            if (gray.at<unsigned char>(y, x) < 150U)
                ++dark;
        }
    }

    return total == 0U
        ? 0.0
        : static_cast<double>(dark) / static_cast<double>(total);
}

double ray_support(
    const cv::Mat& gray,
    int cx,
    int cy,
    int dx,
    int dy) {

    std::size_t dark = 0U;
    std::size_t total = 0U;

    for (int t = 5; t <= 18; ++t) {
        const int x = cx + dx * t;
        const int y = cy + dy * t;

        for (int w = -1; w <= 1; ++w) {
            int sx = x;
            int sy = y;

            if (dx != 0)
                sy += w;
            else
                sx += w;

            if (sx < 0 || sy < 0 || sx >= gray.cols || sy >= gray.rows)
                continue;

            ++total;
            if (gray.at<unsigned char>(sy, sx) < 150U)
                ++dark;
        }
    }

    return total == 0U
        ? 0.0
        : static_cast<double>(dark) / static_cast<double>(total);
}

InkMetrics measure_ink(const cv::Mat& gray, Point2D p) {
    InkMetrics result;
    const int cx = static_cast<int>(std::lround(p.x));
    const int cy = static_cast<int>(std::lround(p.y));

    result.center_density_5x5 = dark_density(gray, cx, cy, 2);
    result.center_density_9x9 = dark_density(gray, cx, cy, 4);
    result.local_density_17x17 = dark_density(gray, cx, cy, 8);

    const double left = ray_support(gray, cx, cy, -1, 0);
    const double right = ray_support(gray, cx, cy, 1, 0);
    const double up = ray_support(gray, cx, cy, 0, -1);
    const double down = ray_support(gray, cx, cy, 0, 1);

    result.horizontal_support = (left + right) * 0.5;
    result.vertical_support = (up + down) * 0.5;
    result.axis_support =
        (result.horizontal_support + result.vertical_support) * 0.5;

    return result;
}

bool filled_dot_candidate(const InkMetrics& ink) {
    return ink.center_density_5x5 >= 0.55 &&
           ink.center_density_9x9 >= 0.35 &&
           ink.local_density_17x17 <= 0.45 &&
           ink.axis_support <= 0.55;
}

bool crossing_candidate(const InkMetrics& ink) {
    return ink.center_density_5x5 >= 0.25 &&
           ink.horizontal_support >= 0.35 &&
           ink.vertical_support >= 0.35 &&
           ink.axis_support >= 0.38;
}

ObjectMatch nearest_component(
    const WireModel& model,
    Point2D position) {

    ObjectMatch result;
    double best = std::numeric_limits<double>::infinity();

    for (const auto& component : model.component_candidates) {
        if (component.kind == ComponentCandidateKind::DiagramFurniture)
            continue;

        const double d = point_to_rect_distance(position, component.bounds);
        if (d < best) {
            best = d;
            result.id = component.id;
            result.distance = d;

            switch (component.kind) {
            case ComponentCandidateKind::Enclosure:
                result.kind = "enclosure";
                break;
            case ComponentCandidateKind::CircularSymbol:
                result.kind = "circular_symbol";
                break;
            case ComponentCandidateKind::ChassisGround:
                result.kind = "chassis_ground";
                break;
            case ComponentCandidateKind::PrimitiveSymbol:
                result.kind = "primitive_symbol";
                break;
            default:
                result.kind = "unknown";
                break;
            }
        }
    }

    result.matched = best <= 3.0;
    if (!result.matched)
        result.id.clear();

    return result;
}

ObjectMatch nearest_connector(
    const WireModel& model,
    Point2D position) {

    ObjectMatch result;
    double best = std::numeric_limits<double>::infinity();

    for (const auto& connector : model.connector_candidates) {
        const double d = point_to_rect_distance(position, connector.bounds);
        if (d < best) {
            best = d;
            result.id = connector.id;
            result.distance = d;
            result.kind = "connector";
        }
    }

    result.matched = best <= 3.0;
    if (!result.matched)
        result.id.clear();

    return result;
}

std::vector<ReplayMapping> load_replay_mapping(
    const fs::path& report_path) {

    std::ifstream in(report_path);
    if (!in)
        throw std::runtime_error(
            "Unable to open AP-DIAG-042 report: " + report_path.string());

    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string text = buffer.str();

    const std::regex pattern(
        R"rx("endpoint_id":"([^"]+)","class":"splice_stop","terminal_splices":\["([^"]+)")rx");

    std::vector<ReplayMapping> result;
    for (std::sregex_iterator it(text.begin(), text.end(), pattern), end;
         it != end;
         ++it) {
        result.push_back({(*it)[1].str(), (*it)[2].str()});
    }

    std::sort(
        result.begin(), result.end(),
        [](const ReplayMapping& a, const ReplayMapping& b) {
            return a.endpoint_id < b.endpoint_id;
        });

    return result;
}

const EndpointCandidate* find_endpoint(
    const WireModel& model,
    const std::string& id) {

    const auto it = std::find_if(
        model.endpoint_candidates.begin(),
        model.endpoint_candidates.end(),
        [&](const EndpointCandidate& endpoint) {
            return endpoint.id == id;
        });

    return it == model.endpoint_candidates.end() ? nullptr : &*it;
}

const TopologyNode* find_node(
    const WireModel& model,
    const std::string& id) {

    const auto it = std::find_if(
        model.nodes.begin(),
        model.nodes.end(),
        [&](const TopologyNode& node) {
            return node.id == id;
        });

    return it == model.nodes.end() ? nullptr : &*it;
}

ClassificationRecord classify(
    const WireModel& model,
    const cv::Mat& gray,
    const ReplayMapping& mapping) {

    const EndpointCandidate* endpoint =
        find_endpoint(model, mapping.endpoint_id);
    if (!endpoint)
        throw std::runtime_error(
            "AP-DIAG-042 endpoint not found in current model: " +
            mapping.endpoint_id);

    const TopologyNode* node =
        find_node(model, mapping.splice_id);
    if (!node)
        throw std::runtime_error(
            "AP-DIAG-042 terminal Splice not found in current model: " +
            mapping.splice_id);

    ClassificationRecord record;
    record.endpoint_id = mapping.endpoint_id;
    record.splice_id = mapping.splice_id;
    record.node_x = node->position.x;
    record.node_y = node->position.y;

    switch (node->type) {
    case TopologyNodeType::Splice:
        record.node_type = "splice";
        break;
    case TopologyNodeType::Junction:
        record.node_type = "junction";
        break;
    case TopologyNodeType::Crossing:
        record.node_type = "crossing";
        break;
    case TopologyNodeType::Continuation:
        record.node_type = "continuation";
        break;
    case TopologyNodeType::ConductorEnd:
        record.node_type = "conductor_end";
        break;
    case TopologyNodeType::ComponentBoundary:
        record.node_type = "component_boundary";
        break;
    default:
        record.node_type = "unresolved";
        break;
    }

    std::set<std::string> segments;
    std::map<std::string, std::size_t> segment_counts;

    for (const auto& edge : model.edges) {
        if (edge.from_node != node->id && edge.to_node != node->id)
            continue;

        ++record.degree;

        if (!edge.conductor_segment.empty()) {
            segments.insert(edge.conductor_segment);
            ++segment_counts[edge.conductor_segment];
        }
    }

    record.unique_segments = segments.size();

    for (const auto& [segment, count] : segment_counts) {
        (void)segment;
        if (count > 1U)
            record.repeated_segment_incidents += count;
    }

    record.nearest_component =
        nearest_component(model, node->position);
    record.nearest_connector =
        nearest_connector(model, node->position);
    record.ink =
        measure_ink(gray, node->position);

    const bool component_hit = record.nearest_component.matched;
    const bool connector_hit = record.nearest_connector.matched;
    const bool dot = filled_dot_candidate(record.ink);
    const bool crossing = crossing_candidate(record.ink);

    if (component_hit && connector_hit) {
        record.classification = "AMBIGUOUS_OBJECT";
        record.basis.push_back("connector_bounds_near_node");
        record.basis.push_back("component_bounds_near_node");
    } else if (connector_hit) {
        record.classification = "CONNECTOR_BODY_CANDIDATE";
        record.basis.push_back("connector_bounds_near_node");
    } else if (component_hit) {
        record.classification = "COMPONENT_BODY_CANDIDATE";
        record.basis.push_back("component_bounds_near_node");
    } else if (dot) {
        record.classification = "REAL_SPLICE_CANDIDATE";
        record.basis.push_back("compact_filled_center_ink");
        record.basis.push_back("no_object_bounds_within_threshold");
    } else if (crossing) {
        record.classification = "CROSSING_CANDIDATE";
        record.basis.push_back("bidirectional_axis_ink");
        record.basis.push_back("no_filled_dot_evidence");
    } else {
        record.classification = "UNDETERMINED";
        record.basis.push_back("no_dominant_object_or_crossing_signature");
    }

    if (record.degree <= 2U)
        record.basis.push_back(
            "degree_" + std::to_string(record.degree));

    if (record.repeated_segment_incidents > 0U)
        record.basis.push_back("repeated_conductor_segment");

    if (endpoint->kind == EndpointKind::GeometricConductorEnd)
        record.basis.push_back("residual_endpoint_geometric");
    else if (endpoint->terminal_role == TerminalRole::ConnectorTerminal)
        record.basis.push_back("residual_endpoint_connector_terminal");
    else if (endpoint->terminal_role == TerminalRole::ComponentTerminal)
        record.basis.push_back("residual_endpoint_component_terminal");

    return record;
}

void write_report(
    const fs::path& report_path,
    const WireModel& model,
    const std::vector<ClassificationRecord>& records,
    const Summary& summary) {

    std::ofstream out(report_path);
    if (!out)
        throw std::runtime_error(
            "Unable to create AP-DIAG-043 report: " +
            report_path.string());

    out << "{\n"
        << "  \"schema_version\": 1,\n"
        << "  \"ap\": \"AP-DIAG-043\",\n"
        << "  \"status\": \"diagnostic_only\",\n"
        << "  \"production_logic_modified\": false,\n"
        << "  \"source\": {\n"
        << "    \"source_id\": \"" << json_escape(model.source_id) << "\",\n"
        << "    \"page\": " << model.page << ",\n"
        << "    \"width\": " << model.image_width << ",\n"
        << "    \"height\": " << model.image_height << "\n"
        << "  },\n"
        << "  \"population\": {\n"
        << "    \"current_model_wires\": " << model.wires.size() << ",\n"
        << "    \"current_model_endpoints\": "
        << model.endpoint_candidates.size() << ",\n"
        << "    \"current_model_nodes\": " << model.nodes.size() << ",\n"
        << "    \"current_model_edges\": " << model.edges.size() << ",\n"
        << "    \"terminal_splice_records\": " << summary.total << "\n"
        << "  },\n"
        << "  \"classification_counts\": {\n"
        << "    \"connector_body_candidate\": " << summary.connector_body << ",\n"
        << "    \"component_body_candidate\": " << summary.component_body << ",\n"
        << "    \"ambiguous_object\": " << summary.ambiguous_object << ",\n"
        << "    \"crossing_candidate\": " << summary.crossing_candidate << ",\n"
        << "    \"real_splice_candidate\": " << summary.real_splice_candidate << ",\n"
        << "    \"undetermined\": " << summary.undetermined << "\n"
        << "  },\n"
        << "  \"records\": [\n";

    for (std::size_t i = 0; i < records.size(); ++i) {
        const auto& record = records[i];

        out << "    {\n"
            << "      \"endpoint_id\": \""
            << json_escape(record.endpoint_id) << "\",\n"
            << "      \"splice_id\": \""
            << json_escape(record.splice_id) << "\",\n"
            << "      \"node_type\": \"" << record.node_type << "\",\n"
            << "      \"node_position\": {\"x\": "
            << record.node_x << ", \"y\": " << record.node_y << "},\n"
            << "      \"degree\": " << record.degree << ",\n"
            << "      \"unique_segments\": " << record.unique_segments << ",\n"
            << "      \"repeated_segment_incidents\": "
            << record.repeated_segment_incidents << ",\n"
            << "      \"nearest_component\": {\n"
            << "        \"matched\": "
            << (record.nearest_component.matched ? "true" : "false") << ",\n"
            << "        \"distance\": " << record.nearest_component.distance << ",\n"
            << "        \"id\": \""
            << json_escape(record.nearest_component.id) << "\",\n"
            << "        \"kind\": \"" << record.nearest_component.kind << "\"\n"
            << "      },\n"
            << "      \"nearest_connector\": {\n"
            << "        \"matched\": "
            << (record.nearest_connector.matched ? "true" : "false") << ",\n"
            << "        \"distance\": " << record.nearest_connector.distance << ",\n"
            << "        \"id\": \""
            << json_escape(record.nearest_connector.id) << "\"\n"
            << "      },\n"
            << "      \"ink\": {\n"
            << "        \"center_density_5x5\": "
            << record.ink.center_density_5x5 << ",\n"
            << "        \"center_density_9x9\": "
            << record.ink.center_density_9x9 << ",\n"
            << "        \"local_density_17x17\": "
            << record.ink.local_density_17x17 << ",\n"
            << "        \"horizontal_support\": "
            << record.ink.horizontal_support << ",\n"
            << "        \"vertical_support\": "
            << record.ink.vertical_support << ",\n"
            << "        \"axis_support\": "
            << record.ink.axis_support << "\n"
            << "      },\n"
            << "      \"classification\": \""
            << record.classification << "\",\n"
            << "      \"basis\": [";

        for (std::size_t j = 0; j < record.basis.size(); ++j) {
            if (j != 0U)
                out << ", ";
            out << "\""
                << json_escape(record.basis[j])
                << "\"";
        }

        out << "]\n"
            << "    }";

        if (i + 1U != records.size())
            out << ",";

        out << "\n";
    }

    out << "  ]\n}\n";
}

void write_crop(
    const cv::Mat& source,
    const ClassificationRecord& record,
    const fs::path& path,
    int pad,
    int scale) {

    const int cx = static_cast<int>(std::lround(record.node_x));
    const int cy = static_cast<int>(std::lround(record.node_y));

    const int left = (std::max)(0, cx - pad);
    const int top = (std::max)(0, cy - pad);
    const int right = (std::min)(source.cols, cx + pad + 1);
    const int bottom = (std::min)(source.rows, cy + pad + 1);

    cv::Mat crop = source(
        cv::Rect(left, top, right - left, bottom - top)).clone();

    cv::Mat scaled;
    cv::resize(
        crop, scaled, cv::Size(), scale, scale, cv::INTER_NEAREST);

    const int local_x = (cx - left) * scale;
    const int local_y = (cy - top) * scale;

    cv::circle(
        scaled,
        {local_x, local_y},
        5 * scale,
        cv::Scalar(0, 0, 255),
        2 * scale,
        cv::LINE_AA);

    cv::putText(
        scaled,
        record.classification,
        {8, 18},
        cv::FONT_HERSHEY_SIMPLEX,
        0.45,
        cv::Scalar(0, 0, 255),
        1,
        cv::LINE_AA);

    if (!cv::imwrite(path.string(), scaled))
        throw std::runtime_error(
            "Unable to write AP-DIAG-043 crop: " + path.string());
}

void write_contact_sheet(
    const std::vector<fs::path>& crops,
    const fs::path& path) {

    const int tile_width = 220;
    const int tile_height = 220;
    const int columns = 5;
    const int rows =
        static_cast<int>(
            (crops.size() + columns - 1U) / columns);

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
                "Unable to read AP-DIAG-043 crop for contact sheet: " +
                crops[i].string());

        cv::Mat resized;
        cv::resize(
            image,
            resized,
            cv::Size(tile_width, tile_height));

        const int x =
            static_cast<int>(i % columns) * tile_width;
        const int y =
            static_cast<int>(i / columns) * tile_height;

        resized.copyTo(
            sheet(
                cv::Rect(x, y, tile_width, tile_height)));
    }

    if (!cv::imwrite(path.string(), sheet))
        throw std::runtime_error(
            "Unable to write AP-DIAG-043 contact sheet: " +
            path.string());
}

void bump_summary(
    Summary& summary,
    const std::string& classification) {

    ++summary.total;

    if (classification == "CONNECTOR_BODY_CANDIDATE")
        ++summary.connector_body;
    else if (classification == "COMPONENT_BODY_CANDIDATE")
        ++summary.component_body;
    else if (classification == "AMBIGUOUS_OBJECT")
        ++summary.ambiguous_object;
    else if (classification == "CROSSING_CANDIDATE")
        ++summary.crossing_candidate;
    else if (classification == "REAL_SPLICE_CANDIDATE")
        ++summary.real_splice_candidate;
    else
        ++summary.undetermined;
}

} // namespace

int run_ap_diag_043(int argc, char** argv) {
    try {
        if (argc < 3) {
            std::cerr
                << "Usage: dx-audit-residual-splice-object-classification "
                << "<image> <output_dir>\n";
            return 2;
        }

        const fs::path image_path = argv[1];
        const fs::path output_dir = argv[2];

        const fs::path ap042_path =
            output_dir / "AP-DIAG-042_exact_production_replay.json";
        const fs::path report_path =
            output_dir /
            "AP-DIAG-043_residual_splice_object_classification.json";
        const fs::path crop_dir =
            fs::path("artifacts") / "residual_splice_classification";

        fs::create_directories(output_dir);
        fs::create_directories(crop_dir);

        const std::vector<ReplayMapping> mappings =
            load_replay_mapping(ap042_path);

        if (mappings.size() != 35U) {
            throw std::runtime_error(
                "Expected exactly 35 AP-DIAG-042 splice-stop mappings; found " +
                std::to_string(mappings.size()) +
                ". Run AP-DIAG-042 against the current extraction first.");
        }

        const cv::Mat source =
            cv::imread(image_path.string(), cv::IMREAD_COLOR);

        if (source.empty())
            throw std::runtime_error(
                "Unable to load source image: " +
                image_path.string());

        cv::Mat gray;
        cv::cvtColor(source, gray, cv::COLOR_BGR2GRAY);

        ExtractionPipeline pipeline;
        const WireModel model =
            pipeline.run(
                image_path.string(),
                image_path.string());

        if (model.wires.size() != 77U) {
            throw std::runtime_error(
                "Current pipeline model is not the expected 77-wire " 
                "TRX300 baseline; found " +
                std::to_string(model.wires.size()));
        }

        std::vector<ClassificationRecord> records;
        records.reserve(mappings.size());

        Summary summary;
        std::vector<fs::path> crop_paths;
        crop_paths.reserve(mappings.size());

        for (std::size_t i = 0; i < mappings.size(); ++i) {
            const ClassificationRecord record =
                classify(model, gray, mappings[i]);

            bump_summary(summary, record.classification);
            records.push_back(record);

            std::ostringstream name;
            name << "splice-"
                 << std::setfill('0')
                 << std::setw(3)
                 << (i + 1U)
                 << ".png";

            const fs::path crop_path =
                crop_dir / name.str();

            write_crop(
                source,
                record,
                crop_path,
                32,
                4);

            crop_paths.push_back(crop_path);
        }

        write_report(
            report_path,
            model,
            records,
            summary);

        const fs::path contact_sheet =
            crop_dir / "AP-DIAG-043_contact_sheet.png";

        write_contact_sheet(
            crop_paths,
            contact_sheet);

        std::cout
            << "[AP-DIAG-043] Current model wires       : "
            << model.wires.size() << "\n"
            << "[AP-DIAG-043] Current model endpoints   : "
            << model.endpoint_candidates.size() << "\n"
            << "[AP-DIAG-043] Current topology nodes    : "
            << model.nodes.size() << "\n"
            << "[AP-DIAG-043] Terminal Splice mappings  : "
            << summary.total << "\n"
            << "[AP-DIAG-043] Connector-body candidates : "
            << summary.connector_body << "\n"
            << "[AP-DIAG-043] Component-body candidates : "
            << summary.component_body << "\n"
            << "[AP-DIAG-043] Ambiguous objects         : "
            << summary.ambiguous_object << "\n"
            << "[AP-DIAG-043] Crossing candidates       : "
            << summary.crossing_candidate << "\n"
            << "[AP-DIAG-043] Real-splice candidates    : "
            << summary.real_splice_candidate << "\n"
            << "[AP-DIAG-043] Undetermined               : "
            << summary.undetermined << "\n"
            << "[AP-DIAG-043] Report: "
            << report_path.string() << "\n"
            << "[AP-DIAG-043] Contact sheet: "
            << contact_sheet.string() << "\n"
            << "[AP-DIAG-043] Diagnostic only; no production source modified.\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr
            << "[AP-DIAG-043] ERROR: "
            << e.what() << "\n";
        return 1;
    }
}

} // namespace eke::dx::wire

int main(int argc, char** argv) {
    return eke::dx::wire::run_ap_diag_043(argc, argv);
}

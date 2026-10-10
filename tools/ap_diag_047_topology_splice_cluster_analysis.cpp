#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

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
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

namespace {
constexpr double kClusterRadiusPx = 12.0;
constexpr double kGroundTruthRadiusPx = 6.0;
constexpr double kExactDuplicateRadiusPx = 2.0;
constexpr std::size_t kExpectedWires = 77U;
constexpr std::size_t kExpectedEndpoints = 189U;
constexpr std::size_t kExpectedNodes = 532U;
constexpr std::size_t kExpectedEdges = 643U;
constexpr std::size_t kExpectedGroundTruth = 19U;
constexpr std::size_t kExpectedResiduals = 35U;

struct GroundTruth {
    std::string id;
    Point2D position{};
    std::string nearest_residual_node;
    double nearest_residual_distance = 0.0;
    bool residual_within_threshold = false;
};

struct Residual {
    std::string endpoint_id;
    std::string node_id;
    std::string nearest_ground_truth;
    double ground_truth_distance = 0.0;
    bool matches_ground_truth = false;
};

struct NodeEvidence {
    const TopologyNode* node = nullptr;
    std::size_t degree = 0;
    std::set<std::string> segments;
    std::vector<std::string> incident_edge_ids;
    std::set<std::string> adjacent_node_ids;
    std::vector<std::string> endpoint_ids;
    double nearest_ground_truth_distance = std::numeric_limits<double>::infinity();
    std::string nearest_ground_truth_id;
    double nearest_residual_distance = std::numeric_limits<double>::infinity();
    std::string nearest_residual_id;
    bool residual = false;
    bool residual_matches_ground_truth = false;
};

double distance(Point2D a, Point2D b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

std::string escape_json(const std::string& value) {
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

std::string read_all(const fs::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Unable to open required AP-DIAG-044 report: " + path.string());
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<GroundTruth> load_ground_truth(const fs::path& path) {
    const std::string text = read_all(path);
    const std::regex pattern(
        R"rx("id"\s*:\s*"(SPLICE-[0-9]+)"[\s\S]*?"source_position"\s*:\s*\{\s*"x"\s*:\s*([-+0-9.eE]+)\s*,\s*"y"\s*:\s*([-+0-9.eE]+)\s*\}[\s\S]*?"nearest_residual_node"\s*:\s*\{\s*"id"\s*:\s*"([^"]*)"[\s\S]*?"distance_px"\s*:\s*([-+0-9.eE]+)\s*\}[\s\S]*?"residual_within_threshold"\s*:\s*(true|false))rx");
    std::vector<GroundTruth> result;
    for (std::sregex_iterator it(text.begin(), text.end(), pattern), end; it != end; ++it) {
        result.push_back({
            (*it)[1].str(),
            {std::stod((*it)[2].str()), std::stod((*it)[3].str())},
            (*it)[4].str(),
            std::stod((*it)[5].str()),
            (*it)[6].str() == "true"
        });
    }
    if (result.size() != kExpectedGroundTruth) {
        throw std::runtime_error("AP-DIAG-044 source-splice records expected 19; found " + std::to_string(result.size()));
    }
    return result;
}

std::vector<Residual> load_residuals(const fs::path& path) {
    const std::string text = read_all(path);
    const std::regex pattern(
        R"rx("endpoint_id"\s*:\s*"([^"]+)"[\s\S]*?"residual_node_id"\s*:\s*"([^"]+)"[\s\S]*?"nearest_ground_truth_splice"\s*:\s*"([^"]*)"[\s\S]*?"distance_px"\s*:\s*([-+0-9.eE]+)[\s\S]*?"matches_ground_truth"\s*:\s*(true|false))rx");
    std::vector<Residual> result;
    for (std::sregex_iterator it(text.begin(), text.end(), pattern), end; it != end; ++it) {
        result.push_back({
            (*it)[1].str(), (*it)[2].str(), (*it)[3].str(),
            std::stod((*it)[4].str()), (*it)[5].str() == "true"
        });
    }
    if (result.size() != kExpectedResiduals) {
        throw std::runtime_error("AP-DIAG-044 residual records expected 35; found " + std::to_string(result.size()));
    }
    return result;
}

std::string node_type(TopologyNodeType type) {
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

void write_crop(const cv::Mat& source, Point2D center, const fs::path& path) {
    const int x = static_cast<int>(std::lround(center.x));
    const int y = static_cast<int>(std::lround(center.y));
    constexpr int pad = 36;
    const int left = std::max(0, x - pad);
    const int top = std::max(0, y - pad);
    const int right = std::min(source.cols, x + pad + 1);
    const int bottom = std::min(source.rows, y + pad + 1);
    cv::Mat crop = source(cv::Rect(left, top, right - left, bottom - top)).clone();
    cv::resize(crop, crop, cv::Size(), 4.0, 4.0, cv::INTER_NEAREST);
    cv::circle(crop, {(x - left) * 4, (y - top) * 4}, 12, cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
    if (!cv::imwrite(path.string(), crop)) throw std::runtime_error("Unable to write crop: " + path.string());
}

void write_contact_sheet(const std::vector<fs::path>& crops, const fs::path& path) {
    if (crops.empty()) throw std::runtime_error("No splice clusters produced; refusing empty contact sheet.");
    constexpr int width = 300;
    constexpr int height = 300;
    constexpr int columns = 4;
    const int rows = static_cast<int>((crops.size() + columns - 1U) / columns);
    cv::Mat sheet(rows * height, columns * width, CV_8UC3, cv::Scalar(255, 255, 255));
    for (std::size_t i = 0; i < crops.size(); ++i) {
        cv::Mat image = cv::imread(crops[i].string(), cv::IMREAD_COLOR);
        if (image.empty()) throw std::runtime_error("Unable to read cluster crop: " + crops[i].string());
        cv::resize(image, image, cv::Size(width, height));
        image.copyTo(sheet(cv::Rect(static_cast<int>(i % columns) * width,
                                    static_cast<int>(i / columns) * height, width, height)));
    }
    if (!cv::imwrite(path.string(), sheet)) throw std::runtime_error("Unable to write contact sheet: " + path.string());
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 3) {
            std::cerr << "Usage: dx-audit-topology-splice-clusters <source_image> <output_dir>\n";
            return 2;
        }

        const fs::path source_path = argv[1];
        const fs::path output_dir = argv[2];
        const fs::path audit_dir = output_dir;
        const fs::path gt_path = audit_dir / "AP-DIAG-044_splice_ground_truth.json";
        const auto ground_truth = load_ground_truth(gt_path);
        const auto residuals = load_residuals(gt_path);

        cv::Mat source = cv::imread(source_path.string(), cv::IMREAD_COLOR);
        if (source.empty()) throw std::runtime_error("Unable to load source image: " + source_path.string());
        if (source.cols != 898 || source.rows != 549) {
            throw std::runtime_error("Canonical source must be 898x549; found " +
                                     std::to_string(source.cols) + "x" + std::to_string(source.rows));
        }

        ExtractionPipeline pipeline;
        const WireModel model = pipeline.run(source_path.string(), source_path.string());
        if (model.wires.size() != kExpectedWires ||
            model.endpoint_candidates.size() != kExpectedEndpoints ||
            model.nodes.size() != kExpectedNodes ||
            model.edges.size() != kExpectedEdges) {
            throw std::runtime_error("Canonical model mismatch; expected wires/endpoints/nodes/edges = 77/189/532/643, found " +
                std::to_string(model.wires.size()) + "/" +
                std::to_string(model.endpoint_candidates.size()) + "/" +
                std::to_string(model.nodes.size()) + "/" +
                std::to_string(model.edges.size()));
        }

        std::map<std::string, NodeEvidence> evidence;
        for (const auto& node : model.nodes) {
            NodeEvidence item;
            item.node = &node;
            evidence.emplace(node.id, std::move(item));
        }
        for (const auto& edge : model.edges) {
            auto from = evidence.find(edge.from_node);
            auto to = evidence.find(edge.to_node);
            if (from != evidence.end()) {
                ++from->second.degree;
                from->second.segments.insert(edge.conductor_segment);
                from->second.incident_edge_ids.push_back(edge.id);
                from->second.adjacent_node_ids.insert(edge.to_node);
            }
            if (to != evidence.end()) {
                ++to->second.degree;
                to->second.segments.insert(edge.conductor_segment);
                to->second.incident_edge_ids.push_back(edge.id);
                to->second.adjacent_node_ids.insert(edge.from_node);
            }
        }
        for (const auto& endpoint : model.endpoint_candidates) {
            const auto found = evidence.find(endpoint.node_id);
            if (found != evidence.end()) found->second.endpoint_ids.push_back(endpoint.id);
        }

        std::map<std::string, Residual> residual_by_node;
        for (const auto& residual : residuals) {
            if (!evidence.contains(residual.node_id))
                throw std::runtime_error("Residual node does not resolve in canonical model: " + residual.node_id);
            residual_by_node[residual.node_id] = residual;
        }

        std::vector<std::string> splice_ids;
        for (auto& [id, item] : evidence) {
            if (item.node->type != TopologyNodeType::Splice) continue;
            splice_ids.push_back(id);
            for (const auto& gt : ground_truth) {
                const double d = distance(item.node->position, gt.position);
                if (d < item.nearest_ground_truth_distance) {
                    item.nearest_ground_truth_distance = d;
                    item.nearest_ground_truth_id = gt.id;
                }
            }
            for (const auto& residual : residuals) {
                const auto node_it = evidence.find(residual.node_id);
                if (node_it == evidence.end()) continue;
                const double d = distance(item.node->position, node_it->second.node->position);
                if (d < item.nearest_residual_distance) {
                    item.nearest_residual_distance = d;
                    item.nearest_residual_id = residual.node_id;
                }
            }
            const auto residual_it = residual_by_node.find(id);
            if (residual_it != residual_by_node.end()) {
                item.residual = true;
                item.residual_matches_ground_truth = residual_it->second.matches_ground_truth;
            }
        }

        // Single-linkage spatial clusters of Splice nodes only. This is an
        // observational radius, not a proposed production snap/merge tolerance.
        std::vector<std::vector<std::string>> clusters;
        std::set<std::string> visited;
        for (const auto& start_id : splice_ids) {
            if (visited.contains(start_id)) continue;
            std::vector<std::string> cluster;
            std::vector<std::string> queue{start_id};
            visited.insert(start_id);
            for (std::size_t q = 0; q < queue.size(); ++q) {
                const auto& current = evidence.at(queue[q]);
                cluster.push_back(queue[q]);
                for (const auto& candidate_id : splice_ids) {
                    if (visited.contains(candidate_id)) continue;
                    if (distance(current.node->position, evidence.at(candidate_id).node->position) <= kClusterRadiusPx) {
                        visited.insert(candidate_id);
                        queue.push_back(candidate_id);
                    }
                }
            }
            std::sort(cluster.begin(), cluster.end());
            clusters.push_back(std::move(cluster));
        }

        fs::create_directories(output_dir / "splice_cluster_analysis");
        const fs::path crop_dir = output_dir / "splice_cluster_analysis";
        std::vector<fs::path> crops;
        const fs::path report_path = output_dir / "AP-DIAG-047_topology_splice_cluster_analysis.json";
        std::ofstream out(report_path);
        if (!out) throw std::runtime_error("Unable to write report: " + report_path.string());

        std::size_t clustered_nodes = 0;
        std::size_t multi_node_clusters = 0;
        double minimum_distinct_splice_distance = std::numeric_limits<double>::infinity();
        for (std::size_t i = 0; i < splice_ids.size(); ++i) {
            for (std::size_t j = i + 1; j < splice_ids.size(); ++j) {
                const double d = distance(evidence.at(splice_ids[i]).node->position,
                                          evidence.at(splice_ids[j]).node->position);
                minimum_distinct_splice_distance = std::min(minimum_distinct_splice_distance, d);
            }
        }

        out << std::setprecision(10)
            << "{\n  \"schema_version\": 1,\n"
            << "  \"ap\": \"AP-DIAG-047\",\n"
            << "  \"status\": \"diagnostic_only\",\n"
            << "  \"production_logic_modified\": false,\n"
            << "  \"parameters\": {\"cluster_radius_px\": " << kClusterRadiusPx
            << ", \"ground_truth_match_radius_px\": " << kGroundTruthRadiusPx
            << ", \"exact_duplicate_radius_px\": " << kExactDuplicateRadiusPx
            << ", \"cluster_method\": \"single_linkage_splice_nodes_only\"},\n"
            << "  \"population\": {\"wires\": " << model.wires.size()
            << ", \"endpoints\": " << model.endpoint_candidates.size()
            << ", \"nodes\": " << model.nodes.size()
            << ", \"edges\": " << model.edges.size()
            << ", \"ground_truth_splices\": " << ground_truth.size()
            << ", \"residual_records\": " << residuals.size()
            << ", \"splice_nodes\": " << splice_ids.size() << "},\n"
            << "  \"summary\": {\"cluster_count\": " << clusters.size()
            << ", \"multi_node_cluster_count\": ";

        for (const auto& cluster : clusters) if (cluster.size() > 1U) ++multi_node_clusters;
        out << multi_node_clusters
            << ", \"minimum_pairwise_splice_node_distance_px\": ";
        if (std::isfinite(minimum_distinct_splice_distance)) out << minimum_distinct_splice_distance;
        else out << "null";
        out << "},\n  \"clusters\": [\n";

        for (std::size_t ci = 0; ci < clusters.size(); ++ci) {
            const auto& cluster = clusters[ci];
            clustered_nodes += cluster.size();
            Point2D centroid{};
            std::set<std::string> segments;
            std::set<std::string> nearby_non_splice_nodes;
            std::set<std::string> gt_ids;
            std::set<std::string> residual_ids;
            bool has_true_residual = false;
            bool has_false_residual = false;
            for (const auto& id : cluster) {
                const auto& item = evidence.at(id);
                centroid.x += item.node->position.x;
                centroid.y += item.node->position.y;
                segments.insert(item.segments.begin(), item.segments.end());
                if (!item.nearest_ground_truth_id.empty() &&
                    item.nearest_ground_truth_distance <= kGroundTruthRadiusPx)
                    gt_ids.insert(item.nearest_ground_truth_id);
                if (item.residual) {
                    residual_ids.insert(id);
                    if (item.residual_matches_ground_truth) has_true_residual = true;
                    else { has_false_residual = true; }
                }
            }
            centroid.x /= static_cast<double>(cluster.size());
            centroid.y /= static_cast<double>(cluster.size());

            for (const auto& [id, item] : evidence) {
                if (item.node->type == TopologyNodeType::Splice) continue;
                for (const auto& splice_id : cluster) {
                    if (distance(item.node->position, evidence.at(splice_id).node->position) <= kClusterRadiusPx) {
                        nearby_non_splice_nodes.insert(id);
                        break;
                    }
                }
            }

            const std::string cluster_id = std::string("CLUSTER-") + (ci + 1U < 10U ? "00" : ci + 1U < 100U ? "0" : "") + std::to_string(ci + 1U);
            const fs::path crop_path = crop_dir / (std::string("cluster-") + (ci + 1U < 10U ? "00" : ci + 1U < 100U ? "0" : "") + std::to_string(ci + 1U) + ".png");
            write_crop(source, centroid, crop_path);
            crops.push_back(crop_path);

            out << "    {\"id\": \"" << cluster_id << "\", \"member_count\": " << cluster.size()
                << ", \"centroid\": {\"x\": " << centroid.x << ", \"y\": " << centroid.y
                << "}, \"unique_conductor_segments\": " << segments.size()
                << ", \"nearby_non_splice_node_count\": " << nearby_non_splice_nodes.size()
                << ", \"ground_truth_splice_ids_within_6px\": [";
            std::size_t n = 0;
            for (const auto& id : gt_ids) out << (n++ ? ", " : "") << "\"" << escape_json(id) << "\"";
            out << "], \"has_ground_truth_residual\": " << (has_true_residual ? "true" : "false")
                << ", \"has_false_residual\": " << (has_false_residual ? "true" : "false")
                << ", \"members\": [";

            for (std::size_t mi = 0; mi < cluster.size(); ++mi) {
                const auto& id = cluster[mi];
                const auto& item = evidence.at(id);
                if (mi) out << ", ";
                out << "{\"node_id\": \"" << escape_json(id)
                    << "\", \"position\": {\"x\": " << item.node->position.x << ", \"y\": " << item.node->position.y
                    << "}, \"type\": \"" << node_type(item.node->type)
                    << "\", \"degree\": " << item.degree
                    << ", \"unique_conductor_segments\": " << item.segments.size()
                    << ", \"incident_edge_ids\": [";
                for (std::size_t ei = 0; ei < item.incident_edge_ids.size(); ++ei)
                    out << (ei ? ", " : "") << "\"" << escape_json(item.incident_edge_ids[ei]) << "\"";
                out << "], \"incident_conductor_segments\": [";
                std::size_t si = 0;
                for (const auto& segment : item.segments) out << (si++ ? ", " : "") << "\"" << escape_json(segment) << "\"";
                out << "], \"adjacent_node_ids\": [";
                std::size_t ai = 0;
                for (const auto& adjacent : item.adjacent_node_ids)
                    out << (ai++ ? ", " : "") << "\"" << escape_json(adjacent) << "\"";
                out << "], \"endpoint_ids\": [";
                for (std::size_t ei = 0; ei < item.endpoint_ids.size(); ++ei)
                    out << (ei ? ", " : "") << "\"" << escape_json(item.endpoint_ids[ei]) << "\"";
                out << "], \"residual\": " << (item.residual ? "true" : "false")
                    << ", \"residual_matches_ground_truth\": " << (item.residual ? (item.residual_matches_ground_truth ? "true" : "false") : "null")
                    << ", \"nearest_ground_truth_id\": \"" << escape_json(item.nearest_ground_truth_id)
                    << "\", \"nearest_ground_truth_distance_px\": " << item.nearest_ground_truth_distance
                    << ", \"nearest_residual_node_id\": \"" << escape_json(item.nearest_residual_id)
                    << "\", \"nearest_residual_node_distance_px\": ";
                if (std::isfinite(item.nearest_residual_distance)) out << item.nearest_residual_distance;
                else out << "null";
                out << "}";
            }

            out << "], \"nearby_non_splice_nodes\": [";
            std::size_t ni = 0;
            for (const auto& id : nearby_non_splice_nodes) {
                const auto& item = evidence.at(id);
                out << (ni++ ? ", " : "") << "{\"node_id\": \"" << escape_json(id)
                    << "\", \"position\": {\"x\": " << item.node->position.x << ", \"y\": " << item.node->position.y
                    << "}, \"type\": \"" << node_type(item.node->type)
                    << "\", \"degree\": " << item.degree << "}";
            }
            out << "], \"crop\": \"" << escape_json(crop_path.string()) << "\"}";
            out << (ci + 1U == clusters.size() ? "\n" : ",\n");
        }
        out << "  ],\n  \"pairwise_proximity\": {\"exact_duplicate_pairs_le_2px\": [";
        bool first = true;
        for (std::size_t i = 0; i < splice_ids.size(); ++i) {
            for (std::size_t j = i + 1; j < splice_ids.size(); ++j) {
                const auto& a = evidence.at(splice_ids[i]);
                const auto& b = evidence.at(splice_ids[j]);
                const double d = distance(a.node->position, b.node->position);
                if (d > kExactDuplicateRadiusPx) continue;
                out << (first ? "" : ", ") << "{\"node_a\": \"" << escape_json(splice_ids[i])
                    << "\", \"node_b\": \"" << escape_json(splice_ids[j]) << "\", \"distance_px\": " << d << "}";
                first = false;
            }
        }
        out << "], \"pairs_le_6px\": [";
        first = true;
        for (std::size_t i = 0; i < splice_ids.size(); ++i) {
            for (std::size_t j = i + 1; j < splice_ids.size(); ++j) {
                const auto& a = evidence.at(splice_ids[i]);
                const auto& b = evidence.at(splice_ids[j]);
                const double d = distance(a.node->position, b.node->position);
                if (d > kGroundTruthRadiusPx) continue;
                out << (first ? "" : ", ") << "{\"node_a\": \"" << escape_json(splice_ids[i])
                    << "\", \"node_b\": \"" << escape_json(splice_ids[j]) << "\", \"distance_px\": " << d << "}";
                first = false;
            }
        }
        out << "]},\n  \"outputs\": {\"contact_sheet\": \""
            << escape_json((crop_dir / "AP-DIAG-047_contact_sheet.png").string())
            << "\", \"crop_count\": " << crops.size()
            << "},\n  \"invariants\": {\"all_splice_nodes_clustered_once\": "
            << (clustered_nodes == splice_ids.size() ? "true" : "false")
            << ", \"all_35_residual_nodes_resolved\": true, \"production_logic_modified\": false}\n}\n";
        out.close();

        write_contact_sheet(crops, crop_dir / "AP-DIAG-047_contact_sheet.png");
        std::cout << "[AP-DIAG-047] Model wires                 : " << model.wires.size() << "\n"
                  << "[AP-DIAG-047] Model endpoints             : " << model.endpoint_candidates.size() << "\n"
                  << "[AP-DIAG-047] Model topology nodes        : " << model.nodes.size() << "\n"
                  << "[AP-DIAG-047] Model topology edges        : " << model.edges.size() << "\n"
                  << "[AP-DIAG-047] Splice nodes analyzed       : " << splice_ids.size() << "\n"
                  << "[AP-DIAG-047] Spatial clusters            : " << clusters.size() << "\n"
                  << "[AP-DIAG-047] Multi-node clusters         : " << multi_node_clusters << "\n"
                  << "[AP-DIAG-047] Report: " << report_path.string() << "\n"
                  << "[AP-DIAG-047] Contact sheet: " << (crop_dir / "AP-DIAG-047_contact_sheet.png").string() << "\n"
                  << "[AP-DIAG-047] Diagnostic only; no production source modified.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "[AP-DIAG-047] ERROR: " << error.what() << "\n";
        return 1;
    }
}

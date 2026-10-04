#include "eke_dx_wire/core/geometry.hpp"
#include "eke_dx_wire/image/conductor_evidence_evaluator.hpp"
#include "eke_dx_wire/image/conductor_normalizer.hpp"
#include "eke_dx_wire/image/ground_approach_conductor_recovery.hpp"
#include "eke_dx_wire/image/image_loader.hpp"
#include "eke_dx_wire/image/geometry_ownership_classifier.hpp"
#include "eke_dx_wire/image/morphology_detector.hpp"
#include "eke_dx_wire/image/normalizer.hpp"
#include "eke_dx_wire/image/shape_detector.hpp"
#include "eke_dx_wire/image/text_region_detector.hpp"
#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"
#include "eke_dx_wire/topology/endpoint_reconstructor.hpp"
#include "eke_dx_wire/topology/gap_interpreter.hpp"
#include "eke_dx_wire/topology/topology_reconstructor.hpp"
#include "eke_dx_wire/topology/wire_reconstructor.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace eke::dx::wire {
namespace {

struct ResidualRecord {
    const EndpointCandidate* baseline = nullptr;
    std::string terminal_splice_id;
};

struct CounterfactualSummary {
    std::string name;
    std::size_t added_mask_pixels = 0;
    std::size_t conductor_segments = 0;
    std::size_t topology_nodes = 0;
    std::size_t topology_edges = 0;
    std::size_t splice_nodes = 0;
    std::size_t endpoint_candidates = 0;
    std::size_t wires = 0;
    std::size_t baseline_residual_endpoints_resolved = 0;
    std::size_t baseline_residual_endpoints_ambiguous = 0;
    std::size_t baseline_residual_endpoints_unresolved = 0;
    std::size_t terminal_splices_fully_resolved = 0;
    std::size_t terminal_splices_partially_resolved = 0;
    std::size_t terminal_splices_unresolved = 0;
    cv::Mat mask;
};

struct TerminalSpliceRecord {
    std::string splice_id;
    Point2D position {};
    std::string current_object_class;
    std::string current_object_id;
    double current_object_distance = -1.0;
    std::vector<std::string> baseline_residual_endpoint_ids;
};

struct EndpointMatch {
    std::string endpoint_id;
    double distance = 0.0;
    bool participates_in_wire = false;
};

std::string json_escape(const std::string& value) {
    std::ostringstream out;
    for (const char c : value) {
        switch (c) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20U) {
                out << "\\u"
                    << std::hex
                    << static_cast<int>(static_cast<unsigned char>(c))
                    << std::dec;
            } else {
                out << c;
            }
        }
    }
    return out.str();
}

const char* component_kind_name(ComponentCandidateKind kind) {
    switch (kind) {
    case ComponentCandidateKind::Enclosure: return "component_enclosure";
    case ComponentCandidateKind::CircularSymbol: return "component_circular_symbol";
    case ComponentCandidateKind::ChassisGround: return "component_chassis_ground";
    case ComponentCandidateKind::PrimitiveSymbol: return "component_primitive";
    case ComponentCandidateKind::DiagramFurniture: return "diagram_furniture";
    case ComponentCandidateKind::Unknown: return "component_unknown";
    }
    return "component_unknown";
}

struct NodeAdjacency {
    std::vector<const TopologyEdge*> edges;
};

std::unordered_map<std::string, NodeAdjacency>
build_adjacency(const std::vector<TopologyEdge>& edges) {
    std::unordered_map<std::string, NodeAdjacency> result;
    result.reserve(edges.size() * 2U);
    for (const auto& edge : edges) {
        result[edge.from_node].edges.push_back(&edge);
        result[edge.to_node].edges.push_back(&edge);
    }
    for (auto& [id, adjacency] : result) {
        std::sort(
            adjacency.edges.begin(), adjacency.edges.end(),
            [](const auto* a, const auto* b) {
                return a->id < b->id;
            });
    }
    return result;
}

const TopologyNode* find_node(
    const std::unordered_map<std::string, const TopologyNode*>& by_id,
    const std::string& id) {
    const auto it = by_id.find(id);
    return it == by_id.end() ? nullptr : it->second;
}

const TopologyEdge* find_edge(
    const std::unordered_map<std::string, const TopologyEdge*>& by_id,
    const std::string& id) {
    const auto it = by_id.find(id);
    return it == by_id.end() ? nullptr : it->second;
}

const std::string& other_node(
    const TopologyEdge& edge,
    const std::string& node_id) {
    return edge.from_node == node_id ? edge.to_node : edge.from_node;
}

bool is_distribution_node(const TopologyNode& node) {
    return node.type == TopologyNodeType::Splice ||
           node.type == TopologyNodeType::Junction;
}

const char* topology_kind_name(TopologyNodeType kind) {
    switch (kind) {
    case TopologyNodeType::ConductorEnd: return "conductor_end";
    case TopologyNodeType::Continuation: return "continuation";
    case TopologyNodeType::Junction: return "junction";
    case TopologyNodeType::Splice: return "splice";
    case TopologyNodeType::Crossing: return "crossing";
    case TopologyNodeType::ComponentBoundary: return "component_boundary";
    case TopologyNodeType::Unresolved: return "unresolved";
    }
    return "unresolved";
}

std::unordered_set<std::string> wire_endpoint_ids(
    const std::vector<Wire>& wires) {
    std::unordered_set<std::string> ids;
    ids.reserve(wires.size() * 2U);
    for (const auto& wire : wires) {
        ids.insert(wire.start_endpoint);
        ids.insert(wire.end_endpoint);
    }
    return ids;
}

std::vector<const EndpointCandidate*> residual_endpoints(
    const WireModel& model) {
    const auto claimed = wire_endpoint_ids(model.wires);
    std::vector<const EndpointCandidate*> result;
    const auto adjacency = build_adjacency(model.edges);

    for (const auto& endpoint : model.endpoint_candidates) {
        if (claimed.contains(endpoint.id))
            continue;
        if (endpoint.kind == EndpointKind::Splice ||
            endpoint.kind == EndpointKind::Unresolved)
            continue;

        const auto it = adjacency.find(endpoint.node_id);
        if (it == adjacency.end() || it->second.edges.size() != 1U)
            continue;

        result.push_back(&endpoint);
    }

    std::sort(
        result.begin(), result.end(),
        [](const auto* a, const auto* b) {
            return a->id < b->id;
        });
    return result;
}

const TopologyNode* terminal_splice_for_endpoint(
    const EndpointCandidate& endpoint,
    const WireModel& model) {

    const auto adjacency = build_adjacency(model.edges);
    std::unordered_map<std::string, const TopologyNode*> nodes;
    nodes.reserve(model.nodes.size());
    for (const auto& node : model.nodes)
        nodes.emplace(node.id, &node);

    std::unordered_map<std::string, const TopologyEdge*> edges;
    edges.reserve(model.edges.size());
    for (const auto& edge : model.edges)
        edges.emplace(edge.id, &edge);

    const auto start_it = adjacency.find(endpoint.node_id);
    if (start_it == adjacency.end() || start_it->second.edges.size() != 1U)
        return nullptr;

    std::string current_node = endpoint.node_id;
    std::string previous_edge = start_it->second.edges.front()->id;
    std::unordered_set<std::string> visited;
    visited.insert(current_node);

    for (int step = 0; step < 1000; ++step) {
        const auto* arrival = find_edge(edges, previous_edge);
        if (arrival == nullptr)
            return nullptr;

        current_node = other_node(*arrival, current_node);
        if (!visited.insert(current_node).second)
            return nullptr;

        const auto* node = find_node(nodes, current_node);
        if (node == nullptr)
            return nullptr;

        if (current_node != endpoint.node_id) {
            const auto endpoint_here =
                std::find_if(
                    model.endpoint_candidates.begin(),
                    model.endpoint_candidates.end(),
                    [&](const EndpointCandidate& candidate) {
                        return candidate.node_id == current_node;
                    });
            if (endpoint_here != model.endpoint_candidates.end())
                return nullptr;
        }

        const auto adj_it = adjacency.find(current_node);
        if (adj_it == adjacency.end() || adj_it->second.edges.empty())
            return nullptr;

        const auto& incident = adj_it->second.edges;

        if (incident.size() == 2U) {
            const TopologyEdge* next = nullptr;
            for (const auto* candidate : incident) {
                if (candidate->id != previous_edge) {
                    next = candidate;
                    break;
                }
            }
            if (next == nullptr)
                return nullptr;

            if (is_distribution_node(*node)) {
                const auto* next_edge = next;
                const std::string& arrival_segment = arrival->conductor_segment;
                const std::string& next_segment = next_edge->conductor_segment;
                if (arrival_segment.empty() ||
                    arrival_segment != next_segment) {
                    return node->type == TopologyNodeType::Splice
                        ? node
                        : nullptr;
                }
            }

            previous_edge = next->id;
            continue;
        }

        if (incident.size() > 2U) {
            if (!is_distribution_node(*node))
                return nullptr;

            std::size_t matches = 0U;
            const TopologyEdge* match = nullptr;
            for (const auto* candidate : incident) {
                if (candidate->id == previous_edge)
                    continue;
                if (!arrival->conductor_segment.empty() &&
                    arrival->conductor_segment ==
                        candidate->conductor_segment) {
                    ++matches;
                    match = candidate;
                }
            }

            if (matches == 0U)
                return node->type == TopologyNodeType::Splice
                    ? node
                    : nullptr;
            if (matches != 1U)
                return nullptr;

            previous_edge = match->id;
            continue;
        }

        return nullptr;
    }

    return nullptr;
}

std::vector<TerminalSpliceRecord> build_terminal_splices(
    const WireModel& baseline,
    const std::vector<const EndpointCandidate*>& residuals) {

    std::map<std::string, TerminalSpliceRecord> records;

    for (const auto* endpoint : residuals) {
        const auto* node = terminal_splice_for_endpoint(*endpoint, baseline);
        if (node == nullptr)
            continue;

        auto& record = records[node->id];
        record.splice_id = node->id;
        record.position = node->position;
        record.baseline_residual_endpoint_ids.push_back(endpoint->id);
    }

    std::vector<TerminalSpliceRecord> result;
    result.reserve(records.size());
    for (auto& [id, record] : records) {
        std::sort(
            record.baseline_residual_endpoint_ids.begin(),
            record.baseline_residual_endpoint_ids.end());
        result.push_back(std::move(record));
    }

    std::sort(
        result.begin(), result.end(),
        [](const auto& a, const auto& b) {
            if (a.position.y != b.position.y)
                return a.position.y < b.position.y;
            if (a.position.x != b.position.x)
                return a.position.x < b.position.x;
            return a.splice_id < b.splice_id;
        });
    return result;
}

double rect_distance(
    const Point2D& point,
    const BoundingBox& box) {
    const double left = static_cast<double>(box.x);
    const double right = left + box.width;
    const double top = static_cast<double>(box.y);
    const double bottom = top + box.height;
    const double dx =
        point.x < left ? left - point.x :
        point.x > right ? point.x - right : 0.0;
    const double dy =
        point.y < top ? top - point.y :
        point.y > bottom ? point.y - bottom : 0.0;
    return std::hypot(dx, dy);
}

bool inside(
    const Point2D& point,
    const BoundingBox& box) {
    return point.x >= box.x &&
           point.x <= box.x + box.width &&
           point.y >= box.y &&
           point.y <= box.y + box.height;
}

std::pair<std::string, std::string> nearest_current_object(
    const Point2D& point,
    const WireModel& baseline,
    double& distance) {

    distance = std::numeric_limits<double>::infinity();
    std::pair<std::string, std::string> result{"none", ""};

    for (const auto& connector : baseline.connector_candidates) {
        const double d = rect_distance(point, connector.bounds);
        if (d < distance) {
            distance = d;
            result = {"connector_candidate", connector.id};
        }
    }

    for (const auto& component : baseline.component_candidates) {
        if (component.kind == ComponentCandidateKind::DiagramFurniture)
            continue;
        const double d = rect_distance(point, component.bounds);
        if (d < distance) {
            distance = d;
            result = {component_kind_name(component.kind), component.id};
        }
    }

    if (!std::isfinite(distance))
        distance = -1.0;

    if (result.first == "none")
        distance = -1.0;

    return result;
}

cv::Mat build_boundary_mask(
    const WireModel& baseline,
    const cv::Size& size,
    int thickness) {

    cv::Mat mask = cv::Mat::zeros(size, CV_8UC1);

    auto add_rect_ring = [&](const BoundingBox& box) {
        const int x = std::max(0, box.x);
        const int y = std::max(0, box.y);
        const int right = std::min(size.width - 1, box.x + box.width);
        const int bottom = std::min(size.height - 1, box.y + box.height);
        if (right < x || bottom < y)
            return;

        cv::rectangle(
            mask,
            cv::Rect(
                x,
                y,
                right - x + 1,
                bottom - y + 1),
            cv::Scalar(255),
            thickness);
    };

    // AP-DIAG-041 diagnostic scope: only established rectangular component
    // enclosures and established connector bounds participate. Small
    // primitives/circles are deliberately excluded so this experiment does
    // not become a blanket "erase component vicinity" test.
    for (const auto& component : baseline.component_candidates) {
        if (component.kind != ComponentCandidateKind::Enclosure)
            continue;
        add_rect_ring(component.bounds);
    }

    for (const auto& connector : baseline.connector_candidates)
        add_rect_ring(connector.bounds);

    return mask;
}

cv::Mat build_filled_object_mask(
    const WireModel& baseline,
    const cv::Size& size) {

    cv::Mat mask = cv::Mat::zeros(size, CV_8UC1);
    for (const auto& component : baseline.component_candidates) {
        if (component.kind != ComponentCandidateKind::Enclosure)
            continue;

        const int x = std::max(0, component.bounds.x);
        const int y = std::max(0, component.bounds.y);
        const int right = std::min(size.width - 1,
            component.bounds.x + component.bounds.width);
        const int bottom = std::min(size.height - 1,
            component.bounds.y + component.bounds.height);
        if (right >= x && bottom >= y) {
            cv::rectangle(
                mask,
                cv::Rect(x, y, right - x + 1, bottom - y + 1),
                cv::Scalar(255),
                cv::FILLED);
        }
    }

    for (const auto& connector : baseline.connector_candidates) {
        const int x = std::max(0, connector.bounds.x);
        const int y = std::max(0, connector.bounds.y);
        const int right = std::min(size.width - 1,
            connector.bounds.x + connector.bounds.width);
        const int bottom = std::min(size.height - 1,
            connector.bounds.y + connector.bounds.height);
        if (right >= x && bottom >= y) {
            cv::rectangle(
                mask,
                cv::Rect(x, y, right - x + 1, bottom - y + 1),
                cv::Scalar(255),
                cv::FILLED);
        }
    }

    return mask;
}

std::size_t count_nonzero(const cv::Mat& image) {
    return static_cast<std::size_t>(cv::countNonZero(image));
}

std::vector<EndpointMatch> matches_for_baseline_endpoint(
    const EndpointCandidate& baseline_endpoint,
    const std::vector<EndpointCandidate>& counter_endpoints,
    const std::unordered_set<std::string>& counter_wire_endpoint_ids,
    double radius) {

    std::vector<EndpointMatch> matches;
    for (const auto& endpoint : counter_endpoints) {
        const double d = distance(
            baseline_endpoint.position,
            endpoint.position);
        if (d <= radius) {
            matches.push_back({
                endpoint.id,
                d,
                counter_wire_endpoint_ids.contains(endpoint.id)
            });
        }
    }

    std::sort(
        matches.begin(), matches.end(),
        [](const auto& a, const auto& b) {
            if (a.distance != b.distance)
                return a.distance < b.distance;
            return a.endpoint_id < b.endpoint_id;
        });
    return matches;
}

CounterfactualSummary run_counterfactual(
    const std::string& name,
    const ExtractionConfig& config,
    const cv::Mat& normalized,
    const WireModel& baseline,
    const cv::Mat& extra_mask,
    double endpoint_match_radius) {

    CounterfactualSummary summary;
    summary.name = name;
    summary.mask = extra_mask.clone();
    summary.added_mask_pixels = count_nonzero(extra_mask);

    ShapeDetector shape_detector(config.shapes);
    const ShapeDetectionArtifacts shapes =
        shape_detector.detect(normalized, baseline.source_id, baseline.page);

    TextRegionDetector text_detector(config.text);
    const TextDetectionArtifacts text_regions =
        text_detector.detect(normalized, baseline.source_id, baseline.page);

    cv::Mat base_exclusion = shapes.exclusion_mask.clone();
    if (base_exclusion.empty()) {
        base_exclusion = cv::Mat::zeros(normalized.size(), CV_8UC1);
    }
    if (!text_regions.exclusion_mask.empty()) {
        cv::bitwise_or(
            base_exclusion,
            text_regions.exclusion_mask,
            base_exclusion);
    }

    cv::Mat exclusion = base_exclusion.clone();
    cv::bitwise_or(extra_mask, exclusion, exclusion);

    MorphologyWireDetector detector(config.morphology);
    DetectionArtifacts detected =
        detector.detect(
            normalized,
            baseline.source_id,
            baseline.page,
            exclusion);

    GeometryOwnershipClassifier ownership(config.geometry_ownership);
    const GeometryOwnershipArtifacts ownership_artifacts =
        ownership.classify(
            detected.conductor_segments,
            baseline.component_candidates,
            text_regions.regions);

    GroundApproachConductorRecovery recovery(
        config.ground_approach_recovery);
    const GroundApproachRecoveryArtifacts recovered =
        recovery.recover(
            detected.binary,
            shapes.regions,
            ownership_artifacts.conductor_candidates,
            exclusion,
            baseline.source_id,
            baseline.page);

    std::vector<ConductorSegment> conductor_candidates =
        ownership_artifacts.conductor_candidates;
    conductor_candidates.insert(
        conductor_candidates.end(),
        recovered.conductor_segments.begin(),
        recovered.conductor_segments.end());

    ConductorEvidenceEvaluator evidence_evaluator(config.conductor_evidence);
    const ConductorEvidenceArtifacts evidence =
        evidence_evaluator.evaluate(
            conductor_candidates,
            normalized);

    ConductorNormalizer normalizer(config.geometry);
    const std::vector<ConductorSegment> normalized_segments =
        normalizer.normalize(evidence.accepted);

    TopologyReconstructor topology(config.topology);
    TopologyArtifacts graph =
        topology.reconstruct(
            normalized_segments,
            baseline.source_id,
            baseline.page);

    GapInterpreter gap_interpreter(config.gap_interpretation);
    const GapInterpretationArtifacts gaps =
        gap_interpreter.interpret(
            graph.nodes,
            graph.edges,
            normalized,
            baseline.source_id,
            baseline.page);
    graph.edges.insert(
        graph.edges.end(),
        gaps.inferred_edges.begin(),
        gaps.inferred_edges.end());

    EndpointReconstructor endpoint_reconstructor;
    const EndpointArtifacts endpoints =
        endpoint_reconstructor.reconstruct(
            graph.nodes,
            graph.edges,
            normalized,
            baseline.source_id,
            baseline.page);

    WireReconstructor wire_reconstructor;
    const WireReconstructionArtifacts wires =
        wire_reconstructor.reconstruct(
            graph.nodes,
            graph.edges,
            endpoints.candidates,
            normalized_segments,
            baseline.source_id,
            baseline.page);

    summary.conductor_segments = normalized_segments.size();
    summary.topology_nodes = graph.nodes.size();
    summary.topology_edges = graph.edges.size();
    summary.endpoint_candidates = endpoints.candidates.size();
    summary.wires = wires.wires.size();

    for (const auto& node : graph.nodes) {
        if (node.type == TopologyNodeType::Splice)
            ++summary.splice_nodes;
    }

    const auto counter_wire_endpoints =
        wire_endpoint_ids(wires.wires);

    std::unordered_map<std::string, const EndpointCandidate*> counter_by_id;
    counter_by_id.reserve(endpoints.candidates.size());
    for (const auto& endpoint : endpoints.candidates)
        counter_by_id.emplace(endpoint.id, &endpoint);

    for (const auto* baseline_endpoint : residual_endpoints(baseline)) {
        const auto matches = matches_for_baseline_endpoint(
            *baseline_endpoint,
            endpoints.candidates,
            counter_wire_endpoints,
            endpoint_match_radius);

        std::size_t resolved_matches = 0U;
        for (const auto& match : matches) {
            if (match.participates_in_wire)
                ++resolved_matches;
        }

        if (resolved_matches == 1U) {
            ++summary.baseline_residual_endpoints_resolved;
        } else if (resolved_matches > 1U) {
            ++summary.baseline_residual_endpoints_ambiguous;
        } else {
            ++summary.baseline_residual_endpoints_unresolved;
        }
    }

    const auto baseline_splices = build_terminal_splices(
        baseline,
        residual_endpoints(baseline));

    for (const auto& splice : baseline_splices) {
        std::size_t resolved = 0U;
        std::size_t ambiguous = 0U;

        for (const auto& endpoint_id : splice.baseline_residual_endpoint_ids) {
            const auto it = std::find_if(
                baseline.endpoint_candidates.begin(),
                baseline.endpoint_candidates.end(),
                [&](const EndpointCandidate& endpoint) {
                    return endpoint.id == endpoint_id;
                });
            if (it == baseline.endpoint_candidates.end())
                continue;

            const auto matches = matches_for_baseline_endpoint(
                *it,
                endpoints.candidates,
                counter_wire_endpoints,
                endpoint_match_radius);

            std::size_t resolved_matches = 0U;
            for (const auto& match : matches) {
                if (match.participates_in_wire)
                    ++resolved_matches;
            }

            if (resolved_matches == 1U)
                ++resolved;
            else if (resolved_matches > 1U)
                ++ambiguous;
        }

        if (resolved == splice.baseline_residual_endpoint_ids.size()) {
            ++summary.terminal_splices_fully_resolved;
        } else if (resolved > 0U || ambiguous > 0U) {
            ++summary.terminal_splices_partially_resolved;
        } else {
            ++summary.terminal_splices_unresolved;
        }
    }

    return summary;
}

void write_summary_json(
    const std::filesystem::path& path,
    const WireModel& baseline,
    const std::vector<TerminalSpliceRecord>& splices,
    const std::vector<CounterfactualSummary>& scenarios,
    const std::string& image_path,
    double endpoint_match_radius,
    int boundary_mask_thickness) {

    std::ofstream out(path);
    if (!out)
        throw std::runtime_error("Unable to create diagnostic report: " + path.string());

    const auto residuals = residual_endpoints(baseline);

    out << "{\n";
    out << "  \"schema_version\": 1,\n";
    out << "  \"ap\": \"AP-DIAG-041\",\n";
    out << "  \"status\": \"diagnostic_only\",\n";
    out << "  \"production_logic_modified\": false,\n";
    out << "  \"source\": {\n";
    out << "    \"image_path\": \"" << json_escape(image_path) << "\",\n";
    out << "    \"source_id\": \"" << json_escape(baseline.source_id) << "\",\n";
    out << "    \"page\": " << baseline.page << ",\n";
    out << "    \"width\": " << baseline.image_width << ",\n";
    out << "    \"height\": " << baseline.image_height << "\n";
    out << "  },\n";
    out << "  \"parameters\": {\n";
    out << "    \"boundary_mask_thickness\": " << boundary_mask_thickness << ",\n";
    out << "    \"endpoint_match_radius\": " << endpoint_match_radius << "\n";
    out << "  },\n";
    out << "  \"baseline\": {\n";
    out << "    \"wires\": " << baseline.wires.size() << ",\n";
    out << "    \"conductor_segments\": " << baseline.conductor_segments.size() << ",\n";
    out << "    \"topology_nodes\": " << baseline.nodes.size() << ",\n";
    out << "    \"topology_edges\": " << baseline.edges.size() << ",\n";
    out << "    \"endpoint_candidates\": " << baseline.endpoint_candidates.size() << ",\n";
    out << "    \"residual_zero_wire_endpoints\": " << residuals.size() << ",\n";
    out << "    \"terminal_splices\": " << splices.size() << "\n";
    out << "  },\n";

    out << "  \"terminal_splices\": [\n";
    for (std::size_t i = 0; i < splices.size(); ++i) {
        const auto& splice = splices[i];
        out << "    {\n";
        out << "      \"splice_id\": \"" << json_escape(splice.splice_id) << "\",\n";
        out << "      \"x\": " << splice.position.x << ",\n";
        out << "      \"y\": " << splice.position.y << ",\n";
        out << "      \"current_object_class\": \"" <<
            json_escape(splice.current_object_class) << "\",\n";
        out << "      \"current_object_id\": \"" <<
            json_escape(splice.current_object_id) << "\",\n";
        out << "      \"current_object_distance\": " <<
            splice.current_object_distance << ",\n";
        out << "      \"baseline_residual_endpoint_ids\": [";
        for (std::size_t j = 0; j < splice.baseline_residual_endpoint_ids.size(); ++j) {
            if (j != 0) out << ", ";
            out << "\"" << json_escape(splice.baseline_residual_endpoint_ids[j]) << "\"";
        }
        out << "]\n";
        out << "    }";
        if (i + 1 != splices.size())
            out << ",";
        out << "\n";
    }
    out << "  ],\n";

    out << "  \"scenarios\": [\n";
    for (std::size_t i = 0; i < scenarios.size(); ++i) {
        const auto& scenario = scenarios[i];
        out << "    {\n";
        out << "      \"name\": \"" << json_escape(scenario.name) << "\",\n";
        out << "      \"additional_mask_pixels\": " << scenario.added_mask_pixels << ",\n";
        out << "      \"conductor_segments\": " << scenario.conductor_segments << ",\n";
        out << "      \"topology_nodes\": " << scenario.topology_nodes << ",\n";
        out << "      \"topology_edges\": " << scenario.topology_edges << ",\n";
        out << "      \"splice_nodes\": " << scenario.splice_nodes << ",\n";
        out << "      \"endpoint_candidates\": " << scenario.endpoint_candidates << ",\n";
        out << "      \"wires\": " << scenario.wires << ",\n";
        out << "      \"baseline_residual_endpoints_resolved\": " <<
            scenario.baseline_residual_endpoints_resolved << ",\n";
        out << "      \"baseline_residual_endpoints_ambiguous\": " <<
            scenario.baseline_residual_endpoints_ambiguous << ",\n";
        out << "      \"baseline_residual_endpoints_unresolved\": " <<
            scenario.baseline_residual_endpoints_unresolved << ",\n";
        out << "      \"terminal_splices_fully_resolved\": " <<
            scenario.terminal_splices_fully_resolved << ",\n";
        out << "      \"terminal_splices_partially_resolved\": " <<
            scenario.terminal_splices_partially_resolved << ",\n";
        out << "      \"terminal_splices_unresolved\": " <<
            scenario.terminal_splices_unresolved << "\n";
        out << "    }";
        if (i + 1 != scenarios.size())
            out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";
}

} // namespace

int run_ap_diag_041(
    const std::string& image_path,
    const std::string& output_dir,
    int boundary_mask_thickness,
    double endpoint_match_radius) {

    ExtractionPipeline pipeline;
    const WireModel baseline =
        pipeline.run(image_path, image_path);

    if (baseline.image_width <= 0 || baseline.image_height <= 0)
        throw std::runtime_error("Baseline extraction produced invalid image dimensions.");

    const cv::Mat source = ImageLoader::load(image_path);
    const cv::Mat normalized = ImageNormalizer::normalize(source);

    const cv::Mat boundary_mask =
        build_boundary_mask(
            baseline,
            normalized.size(),
            boundary_mask_thickness);

    const cv::Mat filled_mask =
        build_filled_object_mask(baseline, normalized.size());

    std::vector<CounterfactualSummary> scenarios;

    const ExtractionConfig config {};
    scenarios.push_back(
        run_counterfactual(
            "established_object_boundary_ring",
            config,
            normalized,
            baseline,
            boundary_mask,
            endpoint_match_radius));

    scenarios.push_back(
        run_counterfactual(
            "established_object_bounds_filled_upper_bound",
            config,
            normalized,
            baseline,
            filled_mask,
            endpoint_match_radius));

    std::vector<TerminalSpliceRecord> terminal_splices =
        build_terminal_splices(
            baseline,
            residual_endpoints(baseline));

    for (auto& splice : terminal_splices) {
        double object_distance = -1.0;
        auto object = nearest_current_object(
            splice.position,
            baseline,
            object_distance);
        splice.current_object_class = object.first;
        splice.current_object_id = object.second;
        splice.current_object_distance = object_distance;
    }

    std::filesystem::create_directories(output_dir);

    const std::filesystem::path report_path =
        std::filesystem::path(output_dir) /
        "AP-DIAG-041_residual_splice_object_mask.json";

    write_summary_json(
        report_path,
        baseline,
        terminal_splices,
        scenarios,
        image_path,
        endpoint_match_radius,
        boundary_mask_thickness);

    for (const auto& scenario : scenarios) {
        const std::filesystem::path mask_path =
            std::filesystem::path(output_dir) /
            (scenario.name + "_mask.png");
        cv::imwrite(mask_path.string(), scenario.mask);
    }

    std::cout
        << "[AP-DIAG-041] Baseline wires                : "
        << baseline.wires.size() << "\n"
        << "[AP-DIAG-041] Baseline residual endpoints    : "
        << residual_endpoints(baseline).size() << "\n"
        << "[AP-DIAG-041] Baseline terminal Splices      : "
        << terminal_splices.size() << "\n";

    for (const auto& scenario : scenarios) {
        std::cout
            << "[AP-DIAG-041] Scenario " << scenario.name << "\n"
            << "               mask pixels                : "
            << scenario.added_mask_pixels << "\n"
            << "               conductor segments       : "
            << scenario.conductor_segments << "\n"
            << "               topology nodes            : "
            << scenario.topology_nodes << "\n"
            << "               topology edges            : "
            << scenario.topology_edges << "\n"
            << "               splice nodes              : "
            << scenario.splice_nodes << "\n"
            << "               endpoint candidates       : "
            << scenario.endpoint_candidates << "\n"
            << "               WireReconstructor wires   : "
            << scenario.wires << "\n"
            << "               baseline residual resolved : "
            << scenario.baseline_residual_endpoints_resolved << "\n"
            << "               baseline residual ambiguous: "
            << scenario.baseline_residual_endpoints_ambiguous << "\n"
            << "               baseline residual unresolved: "
            << scenario.baseline_residual_endpoints_unresolved << "\n"
            << "               terminal Splices fully    : "
            << scenario.terminal_splices_fully_resolved << "\n"
            << "               terminal Splices partial  : "
            << scenario.terminal_splices_partially_resolved << "\n"
            << "               terminal Splices unresolved: "
            << scenario.terminal_splices_unresolved << "\n";
    }

    std::cout
        << "[AP-DIAG-041] Report: "
        << report_path.string() << "\n"
        << "[AP-DIAG-041] No production source was modified.\n";

    return 0;
}

} // namespace eke::dx::wire

int main(int argc, char** argv) {
    try {
        const std::string image_path =
            argc >= 2 ? argv[1] : "samples/trx300ODG.png";
        const std::string output_dir =
            argc >= 3 ? argv[2] : "artifacts/audit";
        const int boundary_mask_thickness =
            argc >= 4 ? std::stoi(argv[3]) : 2;
        const double endpoint_match_radius =
            argc >= 5 ? std::stod(argv[4]) : 8.0;

        return eke::dx::wire::run_ap_diag_041(
            image_path,
            output_dir,
            boundary_mask_thickness,
            endpoint_match_radius);
    }
    catch (const std::exception& ex) {
        std::cerr << "[AP-DIAG-041] FAILED: " << ex.what() << "\n";
        return 1;
    }
}

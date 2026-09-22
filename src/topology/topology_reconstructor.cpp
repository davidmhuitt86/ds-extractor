#include "eke_dx_wire/topology/topology_reconstructor.hpp"

#include "eke_dx_wire/core/ids.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace eke::dx::wire {
namespace {

struct SplitPoint {
    Point2D point {};
    bool endpoint = false;
};

struct SegmentWork {
    std::vector<SplitPoint> points;
};

bool near(double a, double b, double tolerance) {
    return std::abs(a - b) <= tolerance;
}

bool same_point(Point2D a, Point2D b, double tolerance) {
    return distance(a, b) <= tolerance;
}

bool point_on_segment(Point2D p, const Segment2D& s, double tolerance) {
    if (s.is_horizontal(tolerance)) {
        return near(p.y, s.a.y, tolerance) &&
               p.x >= std::min(s.a.x, s.b.x) - tolerance &&
               p.x <= std::max(s.a.x, s.b.x) + tolerance;
    }

    if (s.is_vertical(tolerance)) {
        return near(p.x, s.a.x, tolerance) &&
               p.y >= std::min(s.a.y, s.b.y) - tolerance &&
               p.y <= std::max(s.a.y, s.b.y) + tolerance;
    }

    return false;
}

bool is_endpoint(Point2D p, const Segment2D& s, double tolerance) {
    return same_point(p, s.a, tolerance) || same_point(p, s.b, tolerance);
}

void add_split_point(
    SegmentWork& work,
    Point2D point,
    bool endpoint,
    double tolerance) {

    for (auto& existing : work.points) {
        if (same_point(existing.point, point, tolerance)) {
            existing.endpoint = existing.endpoint || endpoint;
            return;
        }
    }

    work.points.push_back({point, endpoint});
}

std::string point_key(Point2D p) {
    std::ostringstream out;
    out.precision(12);
    out << p.x << "," << p.y;
    return out.str();
}

Point2D intersection(const Segment2D& a, const Segment2D& b) {
    if (a.is_horizontal()) {
        return {b.a.x, a.a.y};
    }
    return {a.a.x, b.a.y};
}

} // namespace

TopologyReconstructor::TopologyReconstructor(TopologyConfig config)
    : config_(config) {}

TopologyArtifacts TopologyReconstructor::reconstruct(
    const std::vector<ConductorSegment>& segments,
    const std::string& source_id,
    int page) const {

    TopologyArtifacts result;
    std::vector<SegmentWork> work(segments.size());

    for (std::size_t i = 0; i < segments.size(); ++i) {
        add_split_point(work[i], segments[i].geometry.a, true,
                        config_.snap_tolerance);
        add_split_point(work[i], segments[i].geometry.b, true,
                        config_.snap_tolerance);
    }

    for (std::size_t i = 0; i < segments.size(); ++i) {
        for (std::size_t j = i + 1; j < segments.size(); ++j) {
            const auto& a = segments[i].geometry;
            const auto& b = segments[j].geometry;

            const bool perpendicular =
                (a.is_horizontal(config_.intersection_tolerance) &&
                 b.is_vertical(config_.intersection_tolerance)) ||
                (a.is_vertical(config_.intersection_tolerance) &&
                 b.is_horizontal(config_.intersection_tolerance));

            if (!perpendicular) {
                continue;
            }

            const Point2D p = intersection(a, b);

            if (!point_on_segment(p, a, config_.intersection_tolerance) ||
                !point_on_segment(p, b, config_.intersection_tolerance)) {
                continue;
            }

            add_split_point(
                work[i], p, is_endpoint(p, a, config_.snap_tolerance),
                config_.snap_tolerance);

            add_split_point(
                work[j], p, is_endpoint(p, b, config_.snap_tolerance),
                config_.snap_tolerance);
        }
    }

    struct NodeEvidence {
        Point2D position {};
        int endpoint_incidents = 0;
        int interior_incidents = 0;
    };

    std::vector<NodeEvidence> evidence;

    for (const auto& segment_work : work) {
        for (const auto& split : segment_work.points) {
            auto it = std::find_if(
                evidence.begin(), evidence.end(),
                [&](const NodeEvidence& e) {
                    return same_point(
                        e.position, split.point, config_.snap_tolerance);
                });

            if (it == evidence.end()) {
                evidence.push_back(
                    {split.point, split.endpoint ? 1 : 0,
                     split.endpoint ? 0 : 1});
            } else if (split.endpoint) {
                ++it->endpoint_incidents;
            } else {
                ++it->interior_incidents;
            }
        }
    }

    std::sort(
        evidence.begin(), evidence.end(),
        [](const NodeEvidence& a, const NodeEvidence& b) {
            if (a.position.y != b.position.y) {
                return a.position.y < b.position.y;
            }
            return a.position.x < b.position.x;
        });

    for (const auto& e : evidence) {
        TopologyNodeType type = TopologyNodeType::ConductorEnd;

        if (e.interior_incidents >= 2 && e.endpoint_incidents == 0) {
            type = TopologyNodeType::Crossing;
        } else if (e.endpoint_incidents + e.interior_incidents >= 3 ||
                   (e.endpoint_incidents > 0 && e.interior_incidents > 0)) {
            type = TopologyNodeType::Junction;
        } else if (e.endpoint_incidents + e.interior_incidents == 2) {
            type = TopologyNodeType::Continuation;
        }

        std::ostringstream canonical;
        canonical << source_id << ":" << page << ":" << point_key(e.position);

        TopologyNode node;
        node.id = stable_id("topology-node", canonical.str());
        node.position = e.position;
        node.type = type;
        node.electrically_connective =
            type != TopologyNodeType::Crossing;

        result.nodes.push_back(std::move(node));
    }

    auto node_for_point = [&](Point2D point) -> const TopologyNode& {
        auto it = std::find_if(
            result.nodes.begin(), result.nodes.end(),
            [&](const TopologyNode& node) {
                return same_point(
                    node.position, point, config_.snap_tolerance);
            });

        return *it;
    };

    for (std::size_t i = 0; i < segments.size(); ++i) {
        auto& points = work[i].points;

        const bool horizontal =
            segments[i].geometry.is_horizontal(config_.intersection_tolerance);

        std::sort(
            points.begin(), points.end(),
            [&](const SplitPoint& a, const SplitPoint& b) {
                return horizontal
                    ? a.point.x < b.point.x
                    : a.point.y < b.point.y;
            });

        points.erase(
            std::unique(
                points.begin(), points.end(),
                [&](const SplitPoint& a, const SplitPoint& b) {
                    return same_point(
                        a.point, b.point, config_.snap_tolerance);
                }),
            points.end());

        for (std::size_t k = 1; k < points.size(); ++k) {
            const auto& a = node_for_point(points[k - 1].point);
            const auto& b = node_for_point(points[k].point);

            if (a.id == b.id) {
                continue;
            }

            std::ostringstream canonical;
            canonical << source_id << ":" << page << ":"
                      << segments[i].id << ":" << a.id << ":" << b.id;

            TopologyEdge edge;
            edge.id = stable_id("topology-edge", canonical.str());
            edge.from_node = a.id;
            edge.to_node = b.id;
            edge.conductor_segment = segments[i].id;
            result.edges.push_back(std::move(edge));
        }
    }

    // A splice is a connective topology node with three or more
    // reconstructed conductor edges. It is an electrical distribution
    // node, not a wire endpoint. Classify it only after edge splitting so
    // the actual graph degree is available; a T-connection may originate
    // from one interior segment plus one segment endpoint.
    std::unordered_map<std::string, std::size_t> degree;
    degree.reserve(result.nodes.size());
    for (const auto& edge : result.edges) {
        ++degree[edge.from_node];
        ++degree[edge.to_node];
    }

    for (auto& node : result.nodes) {
        if (node.type == TopologyNodeType::Junction &&
            node.electrically_connective &&
            degree[node.id] >= 3) {
            node.type = TopologyNodeType::Splice;
        }
    }

    std::sort(
        result.edges.begin(), result.edges.end(),
        [](const TopologyEdge& a, const TopologyEdge& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

#include "eke_dx_wire/image/diagram_furniture_classifier.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

namespace eke::dx::wire {
namespace {

bool eligible(const ComponentCandidate& candidate) {
    return candidate.kind == ComponentCandidateKind::CircularSymbol ||
           candidate.kind == ComponentCandidateKind::PrimitiveSymbol;
}

double center_x(const ComponentCandidate& c) {
    return c.bounds.x + c.bounds.width / 2.0;
}

double center_y(const ComponentCandidate& c) {
    return c.bounds.y + c.bounds.height / 2.0;
}

// Shortest gap between two axis-aligned boxes; 0 when they touch or overlap.
double bbox_gap(const BoundingBox& a, const BoundingBox& b) {
    const double a_left = a.x, a_right = a.x + a.width;
    const double a_top = a.y, a_bottom = a.y + a.height;
    const double b_left = b.x, b_right = b.x + b.width;
    const double b_top = b.y, b_bottom = b.y + b.height;

    const double dx = (std::max)({a_left - b_right, b_left - a_right, 0.0});
    const double dy = (std::max)({a_top - b_bottom, b_top - a_bottom, 0.0});
    return std::hypot(dx, dy);
}

// Single-linkage clustering of `values`: sorts, then starts a new cluster
// whenever the gap to the previous value exceeds tolerance. Returns a
// cluster id per input index (not per sorted position).
std::vector<std::size_t> cluster_1d(
    const std::vector<double>& values, double tolerance) {

    const std::size_t n = values.size();
    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return values[a] < values[b];
    });

    std::vector<std::size_t> cluster_id(n, 0);
    std::size_t current_cluster = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (i > 0 && values[order[i]] - values[order[i - 1]] > tolerance) {
            ++current_cluster;
        }
        cluster_id[order[i]] = current_cluster;
    }
    return cluster_id;
}

class UnionFind {
public:
    explicit UnionFind(std::size_t n) : parent_(n) {
        std::iota(parent_.begin(), parent_.end(), 0);
    }

    std::size_t find(std::size_t x) {
        while (parent_[x] != x) {
            parent_[x] = parent_[parent_[x]];
            x = parent_[x];
        }
        return x;
    }

    void unite(std::size_t a, std::size_t b) {
        a = find(a);
        b = find(b);
        if (a != b) parent_[a] = b;
    }

private:
    std::vector<std::size_t> parent_;
};

} // namespace

DiagramFurnitureClassifier::DiagramFurnitureClassifier(DiagramFurnitureConfig config)
    : config_(config) {}

std::vector<ComponentCandidate> DiagramFurnitureClassifier::classify(
    std::vector<ComponentCandidate> candidates) const {

    std::vector<std::size_t> eligible_indices;
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        if (eligible(candidates[i])) {
            eligible_indices.push_back(i);
        }
    }

    if (eligible_indices.size() < config_.minimum_cluster_size) {
        return candidates;
    }

    const std::size_t n = eligible_indices.size();

    // Phase 1: spatial proximity. Only shapes packed close together can be
    // part of the same table - this is what keeps a diode here and an
    // unrelated connector there from being chained into one group just
    // because a schematic's parts routinely share drawing-grid coordinates.
    UnionFind spatial(n);
    for (std::size_t a = 0; a < n; ++a) {
        for (std::size_t b = a + 1; b < n; ++b) {
            const double gap = bbox_gap(
                candidates[eligible_indices[a]].bounds,
                candidates[eligible_indices[b]].bounds);
            if (gap <= config_.max_neighbor_gap_px) {
                spatial.unite(a, b);
            }
        }
    }

    std::unordered_map<std::size_t, std::vector<std::size_t>> spatial_clusters;
    for (std::size_t local = 0; local < n; ++local) {
        spatial_clusters[spatial.find(local)].push_back(local);
    }

    // Phase 2: within each spatially contiguous cluster, check for row and
    // column regularity. Alignment tolerance is applied per-cluster so a
    // grid's own local geometry decides its rows/columns, not the whole
    // page's coordinate space.
    for (const auto& [root, members] : spatial_clusters) {
        (void)root;
        if (members.size() < config_.minimum_cluster_size) {
            continue;
        }

        std::vector<double> xs, ys;
        xs.reserve(members.size());
        ys.reserve(members.size());
        for (const std::size_t local : members) {
            const auto& c = candidates[eligible_indices[local]];
            xs.push_back(center_x(c));
            ys.push_back(center_y(c));
        }

        const std::vector<std::size_t> row_ids = cluster_1d(ys, config_.alignment_tolerance_px);
        const std::vector<std::size_t> col_ids = cluster_1d(xs, config_.alignment_tolerance_px);

        std::unordered_map<std::size_t, bool> distinct_rows, distinct_cols;
        for (const auto id : row_ids) distinct_rows[id] = true;
        for (const auto id : col_ids) distinct_cols[id] = true;

        if (distinct_rows.size() >= config_.minimum_rows &&
            distinct_cols.size() >= config_.minimum_columns) {
            for (const std::size_t local : members) {
                candidates[eligible_indices[local]].kind =
                    ComponentCandidateKind::DiagramFurniture;
            }
        }
    }

    return candidates;
}

} // namespace eke::dx::wire

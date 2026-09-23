#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <cstddef>
#include <vector>

namespace eke::dx::wire {

struct DiagramFurnitureConfig {
    // Minimum number of shapes in a spatially contiguous grid before it is
    // treated as tabular diagram content. This has to clear more than "a
    // few real circuit symbols happen to line up": a real component's own
    // internal contact/pin grid (a relay with several contacts, a
    // multi-pin connector) is itself small and locally grid-shaped, so
    // shape arrangement alone cannot tell it apart from a reference table
    // at a handful of members. Calibrated against the TRX300 reference
    // fixture, where the switch-continuity table forms one 50-shape grid
    // while the largest real component contact grids are 9-10 shapes;
    // this default sits with margin above the largest observed real
    // cluster and well below the table, but may need revisiting against
    // other source diagrams.
    std::size_t minimum_cluster_size = 16;

    // A grid needs distinct rows *and* distinct columns to be tabular; a
    // single row or column of aligned symbols (a fuse block, a row of
    // relay contacts) is common, real circuit layout and must not match.
    std::size_t minimum_rows = 2;
    std::size_t minimum_columns = 3;

    // Shape centers within this many pixels (on the relevant axis) are
    // considered the same row/column.
    double alignment_tolerance_px = 6.0;

    // Two shapes are only considered part of the same candidate table when
    // the gap between their bounding boxes is within this many pixels.
    // Without a spatial bound, row/column alignment alone chains together
    // unrelated symbols that merely happen to share a schematic's implicit
    // drawing grid (e.g. a diode here and a connector pin block there),
    // which is common in circuit layout and is not evidence of a table.
    double max_neighbor_gap_px = 45.0;
};

/**
 * Diagram furniture - legend/color-key tables, switch-continuity charts,
 * title blocks - is drawn with the same small circle/rectangle primitives
 * ComponentCandidateClassifier already buckets as CircularSymbol or
 * PrimitiveSymbol, so shape alone cannot distinguish them from real
 * circuit symbols. This stage looks at *arrangement* instead: a dense,
 * regular row-and-column grid of small shapes is the signature of tabular
 * diagram content, not individual circuit symbols spread across a
 * schematic.
 *
 * Detection is two-phase: first candidates are grouped into spatially
 * contiguous clusters (bounding-box gap below max_neighbor_gap_px), then
 * each cluster is checked for row/column regularity. The spatial phase
 * matters because row/column alignment alone is not distinctive in a
 * schematic - real circuit symbols are routinely drawn on the same
 * implicit grid - so only a *local*, dense block that also forms a grid
 * counts as furniture. Matching candidates are re-tagged
 * ComponentCandidateKind::DiagramFurniture; everything else, including a
 * real multi-pin connector's single row of pins, passes through
 * unchanged.
 *
 * This is a re-tagging step, not a deletion: furniture candidates keep
 * their id/bounds/shape_ids/evidence so nothing is silently dropped from
 * the model, but their kind marks them as non-circuit content so
 * consumers (audits, exports, terminal/identity resolution) can exclude
 * them from component counts and circuit interpretation.
 */
class DiagramFurnitureClassifier {
public:
    explicit DiagramFurnitureClassifier(DiagramFurnitureConfig config = {});

    [[nodiscard]] std::vector<ComponentCandidate> classify(
        std::vector<ComponentCandidate> candidates) const;

private:
    DiagramFurnitureConfig config_;
};

} // namespace eke::dx::wire

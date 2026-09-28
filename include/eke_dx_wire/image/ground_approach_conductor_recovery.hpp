#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/image/shape_detector.hpp"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace eke::dx::wire {

// AP-DIAG-FIX-007: AP-DIAG-AUDIT-006 established that MorphologyWireDetector's
// two fixed-orientation (horizontal, vertical), fixed-minimum-length
// morphological openings cannot preserve every real approach conductor
// immediately above an already-detected ChassisGround symbol - specifically
// a short (< vertical_kernel_length) straight run, or one that includes a
// diagonal jog. This is a narrowly-scoped, LOCAL recovery: it only ever
// looks in a small, bounded window directly above a symbol ShapeDetector
// has already accepted as a genuine ChassisGround, using the same binary
// threshold image MorphologyWireDetector already computed. It never scans
// the whole diagram and never runs when the standard detector already
// reaches the symbol.
struct GroundApproachRecoveryConfig {
    // Half-width, in pixels, of the local search corridor centered on the
    // ground symbol's own horizontal center. Wide enough to keep a modest
    // L-bend or diagonal jog inside the window without becoming a
    // diagram-wide search.
    int corridor_half_width = 12;

    // How far above the ground symbol's own (already shape-detector-padded)
    // top edge the local search window extends.
    int search_height = 20;

    // If any conductor segment MorphologyWireDetector already produced has
    // an endpoint within this distance of the ground symbol's anchor point,
    // the standard detector already reached the symbol and no recovery is
    // attempted - this is a gap-filling mechanism only, never a duplicate
    // or competing extraction of ink the pipeline already has.
    double already_covered_distance = 8.0;

    // A row (or column, for the near-horizontal part of an L-bend) whose
    // ink run is wider than this is treated as outside the "thin
    // approach wire" shape this recovery is scoped to, and is a strong
    // signal recovery is looking at something else (a component outline,
    // a text glyph, a filled shape) - recovery is abandoned for that
    // symbol rather than guessed.
    int max_run_width = 30;

    // The centerline is traced row by row starting at the ground symbol
    // (the one certain reference point) and moving outward. A bend or
    // diagonal jog can momentarily present more than one disjoint ink run
    // in a single row (e.g. the rounded corner of an L-bend); when that
    // happens, the run nearest the already-traced path is taken, exactly
    // like following a single wire through a corner. This is bounded by
    // two safety limits, both required to hold or recovery is abandoned:
    // no row may show more than max_disjoint_runs_per_row runs (a true
    // branch/junction, rather than one bend, tends to sustain multiple
    // runs), and the chosen run's center may not jump more than
    // max_row_to_row_jump pixels from the previously accepted point (a
    // genuinely different, unrelated piece of ink jumping into the
    // corridor is rejected rather than silently adopted).
    int max_disjoint_runs_per_row = 6;
    double max_row_to_row_jump = 24.0;

    // A connected ink blob smaller than this many pixels is treated as
    // noise, not a real approach conductor.
    int min_component_pixels = 4;

    // cv::approxPolyDP epsilon used to reduce the traced centerline to a
    // small number of straight ConductorSegments while preserving real
    // bends and diagonals (never collapsing a bent path into one straight
    // line).
    double polyline_simplify_epsilon = 1.5;
};

struct GroundApproachRecoveryArtifacts {
    std::vector<ConductorSegment> conductor_segments;
};

class GroundApproachConductorRecovery {
public:
    explicit GroundApproachConductorRecovery(
        GroundApproachRecoveryConfig config = {});

    // binary: the same adaptive-threshold conductor binary
    // MorphologyWireDetector already computed (DetectionArtifacts::binary) -
    // reused, not recomputed, so recovered geometry is extracted from the
    // exact same ink representation as every other conductor in the model.
    // shapes: ShapeDetector's full region list; only ShapeKind::ChassisGround
    // / ShapeRole::Exclusion regions are examined.
    // existing_conductor_segments: MorphologyWireDetector's own output, used
    // solely to detect symbols the standard detector already reached (no
    // recovery attempted for those).
    // exclusion_mask: the same component/text exclusion mask already used to
    // blank ink before standard conductor extraction - reused here as a
    // boundary-safety guard so recovery never reads ink already claimed by
    // another recognized component or text region.
    [[nodiscard]] GroundApproachRecoveryArtifacts recover(
        const cv::Mat& binary,
        const std::vector<ShapeRegion>& shapes,
        const std::vector<ConductorSegment>& existing_conductor_segments,
        const cv::Mat& exclusion_mask,
        const std::string& source_id,
        int page) const;

private:
    GroundApproachRecoveryConfig config_;
};

} // namespace eke::dx::wire

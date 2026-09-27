// AP-DIAG-FIX-007: focused unit coverage for GroundApproachConductorRecovery
// in isolation, using synthetic cv::Mat fixtures that reproduce the
// diagonal-jog and L-bend/short-vertical-run shapes AP-DIAG-AUDIT-006 found
// in the real TRX300 diagram, plus explicit negative controls proving the
// recovery does not fabricate geometry, does not cross into unrelated ink,
// and preserves ambiguity rather than guessing.
//
// tests/test_ground_approach_recovery_production.cpp is the companion
// production-path regression against the real canonical sample; this file
// exercises the recovery algorithm's own decision boundaries directly.

#include "eke_dx_wire/image/ground_approach_conductor_recovery.hpp"

#include <opencv2/imgproc.hpp>

#include <cassert>
#include <iostream>

using namespace eke::dx::wire;

namespace {

ShapeRegion chassis_ground_region(
    const std::string& id, int x, int y, int width, int height) {
    ShapeRegion region;
    region.id = id;
    region.kind = ShapeKind::ChassisGround;
    region.role = ShapeRole::Exclusion;
    region.bounds = {x, y, width, height};
    region.confidence = 0.95;
    return region;
}

cv::Mat blank_binary(int width = 100, int height = 100) {
    return cv::Mat::zeros(height, width, CV_8UC1);
}

void draw_line(
    cv::Mat& binary, cv::Point a, cv::Point b, int thickness = 1) {
    cv::line(binary, a, b, cv::Scalar(255), thickness);
}

} // namespace

int main() {
    // CASE 1: a diagonal jog into a short vertical run, anchored directly
    // above a ChassisGround symbol at (50, 60) - the exact shape
    // AP-DIAG-AUDIT-006 found for component-candidate-shape-region-
    // 5aa211846bb7891d, reproduced synthetically.
    {
        cv::Mat binary = blank_binary();
        draw_line(binary, {40, 44}, {50, 50}); // diagonal jog
        draw_line(binary, {50, 50}, {50, 59}); // short vertical run to the bar

        const ShapeRegion ground = chassis_ground_region("ground-1", 40, 60, 20, 13);

        GroundApproachConductorRecovery recovery;
        const auto result = recovery.recover(
            binary, {ground}, /*existing_conductor_segments=*/{},
            /*exclusion_mask=*/{}, "test", 0);

        assert(!result.conductor_segments.empty());
        // The recovered path reaches the anchor point exactly.
        bool reaches_anchor = false;
        for (const auto& segment : result.conductor_segments) {
            if ((segment.geometry.a.x == 50.0 && segment.geometry.a.y == 60.0) ||
                (segment.geometry.b.x == 50.0 && segment.geometry.b.y == 60.0)) {
                reaches_anchor = true;
            }
        }
        assert(reaches_anchor);
        // Not simplified into one artificial straight segment - a real
        // diagonal-then-vertical bend requires at least two.
        assert(result.conductor_segments.size() >= 2);
    }

    // CASE 2: a short L-bend into a vertical run - the shape
    // AP-DIAG-AUDIT-006 found for component-candidate-shape-region-
    // 6b6ccc2d59afe578, reproduced synthetically.
    {
        cv::Mat binary = blank_binary();
        draw_line(binary, {35, 40}, {50, 40}); // horizontal top of the bend
        draw_line(binary, {50, 40}, {50, 59}); // vertical run down to the bar

        const ShapeRegion ground = chassis_ground_region("ground-2", 40, 60, 20, 13);

        GroundApproachConductorRecovery recovery;
        const auto result = recovery.recover(
            binary, {ground}, {}, {}, "test", 0);

        assert(!result.conductor_segments.empty());
        assert(result.conductor_segments.size() >= 2);
    }

    // NEGATIVE CONTROL: already covered. When an existing conductor
    // segment (as MorphologyWireDetector would have produced) already
    // reaches the anchor, no recovery is attempted - this is a gap-filler
    // only, never a competing or duplicate extraction.
    {
        cv::Mat binary = blank_binary();
        draw_line(binary, {50, 40}, {50, 59});

        const ShapeRegion ground = chassis_ground_region("ground-3", 40, 60, 20, 13);
        ConductorSegment existing;
        existing.geometry = Segment2D{{50.0, 40.0}, {50.0, 60.0}};

        GroundApproachConductorRecovery recovery;
        const auto result = recovery.recover(
            binary, {ground}, {existing}, {}, "test", 0);

        assert(result.conductor_segments.empty());
    }

    // NEGATIVE CONTROL: no ink at all reaching the symbol - nothing is
    // fabricated when there is no raster evidence.
    {
        cv::Mat binary = blank_binary();

        const ShapeRegion ground = chassis_ground_region("ground-4", 40, 60, 20, 13);

        GroundApproachConductorRecovery recovery;
        const auto result = recovery.recover(
            binary, {ground}, {}, {}, "test", 0);

        assert(result.conductor_segments.empty());
    }

    // NEGATIVE CONTROL: disconnected, unrelated ink nearby (not touching
    // the ground symbol's own edge) must not be adopted merely because it
    // is present in the search window.
    {
        cv::Mat binary = blank_binary();
        // A short, isolated horizontal mark well above the symbol, not
        // connected to anything reaching the anchor row.
        draw_line(binary, {45, 45}, {55, 45});

        const ShapeRegion ground = chassis_ground_region("ground-5", 40, 60, 20, 13);

        GroundApproachConductorRecovery recovery;
        const auto result = recovery.recover(
            binary, {ground}, {}, {}, "test", 0);

        assert(result.conductor_segments.empty());
    }

    // NEGATIVE CONTROL: a wide blob (e.g. a component outline or filled
    // shape the approach wire happens to touch) crossing the corridor is
    // outside the "thin approach wire" shape this recovery is scoped to.
    // No geometry is fabricated by arbitrarily picking a path through it.
    // A tight max_run_width makes this reachable within the corridor.
    {
        cv::Mat binary = blank_binary();
        draw_line(binary, {50, 45}, {50, 59}); // the run to the bar
        cv::rectangle(
            binary, {38, 40}, {62, 44}, cv::Scalar(255), cv::FILLED);

        const ShapeRegion ground = chassis_ground_region("ground-6", 40, 60, 20, 13);

        GroundApproachRecoveryConfig config;
        config.max_run_width = 10;
        GroundApproachConductorRecovery recovery(config);
        const auto result = recovery.recover(
            binary, {ground}, {}, {}, "test", 0);

        assert(result.conductor_segments.empty());
    }

    // NEGATIVE CONTROL: ink that exists but lies inside the exclusion
    // mask (as it would for a component or text region already claimed
    // elsewhere) is not read at all - boundary safety reuses the same
    // exclusion mechanism the rest of conductor extraction already
    // trusts.
    {
        cv::Mat binary = blank_binary();
        draw_line(binary, {50, 40}, {50, 59});

        cv::Mat exclusion = blank_binary();
        cv::rectangle(
            exclusion, {30, 30}, {70, 60}, cv::Scalar(255), cv::FILLED);

        const ShapeRegion ground = chassis_ground_region("ground-7", 40, 60, 20, 13);

        GroundApproachConductorRecovery recovery;
        const auto result = recovery.recover(
            binary, {ground}, {}, exclusion, "test", 0);

        assert(result.conductor_segments.empty());
    }

    // Non-ChassisGround shapes are never examined.
    {
        cv::Mat binary = blank_binary();
        draw_line(binary, {50, 40}, {50, 59});

        ShapeRegion rectangle;
        rectangle.id = "rect-1";
        rectangle.kind = ShapeKind::Rectangle;
        rectangle.role = ShapeRole::Exclusion;
        rectangle.bounds = {40, 60, 20, 13};

        GroundApproachConductorRecovery recovery;
        const auto result = recovery.recover(
            binary, {rectangle}, {}, {}, "test", 0);

        assert(result.conductor_segments.empty());
    }

    // Determinism: identical input produces identical output.
    {
        cv::Mat binary = blank_binary();
        draw_line(binary, {40, 44}, {50, 50});
        draw_line(binary, {50, 50}, {50, 59});
        const ShapeRegion ground = chassis_ground_region("ground-8", 40, 60, 20, 13);

        GroundApproachConductorRecovery recovery;
        const auto result_a = recovery.recover(binary, {ground}, {}, {}, "test", 0);
        const auto result_b = recovery.recover(binary, {ground}, {}, {}, "test", 0);

        assert(result_a.conductor_segments.size() == result_b.conductor_segments.size());
        for (std::size_t i = 0; i < result_a.conductor_segments.size(); ++i) {
            assert(result_a.conductor_segments[i].id ==
                   result_b.conductor_segments[i].id);
            assert(result_a.conductor_segments[i].geometry.a.x ==
                   result_b.conductor_segments[i].geometry.a.x);
            assert(result_a.conductor_segments[i].geometry.a.y ==
                   result_b.conductor_segments[i].geometry.a.y);
            assert(result_a.conductor_segments[i].geometry.b.x ==
                   result_b.conductor_segments[i].geometry.b.x);
            assert(result_a.conductor_segments[i].geometry.b.y ==
                   result_b.conductor_segments[i].geometry.b.y);
        }
    }

    std::cout << "ground approach conductor recovery unit tests passed\n";
    return 0;
}

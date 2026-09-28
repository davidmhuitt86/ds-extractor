// AP-DIAG-FIX-007: production-path regression proving the actual
// ExtractionPipeline recovers the missing ground-approach conductor for
// both genuine ChassisGround symbols AP-DIAG-AUDIT-006 found unresolved,
// using the real canonical TRX300 sample - not a synthetic cv::Mat
// fixture handed directly to GroundApproachConductorRecovery.
//
// AP-DIAG-AUDIT-006 established that MorphologyWireDetector's
// fixed-orientation, fixed-minimum-length morphological openings could
// not preserve the real, continuous approach-conductor ink immediately
// above component-candidate-shape-region-5aa211846bb7891d (a diagonal
// jog into a short vertical run) or component-candidate-shape-region-
// 6b6ccc2d59afe578 (an L-bend into a short vertical run), so neither
// produced a resolved Ground endpoint. This test runs the real,
// unmodified ExtractionPipeline against samples/trx300ODG.png and
// asserts both are now resolved - it fails against the pre-fix pipeline
// (4 of 6 resolved) and passes after GroundApproachConductorRecovery is
// wired in.

#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <set>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

namespace {

const EndpointCandidate* ground_endpoint_for(
    const WireModel& model, const std::string& component_id) {
    for (const auto& endpoint : model.endpoint_candidates) {
        if (endpoint.kind == EndpointKind::Ground &&
            endpoint.component_id == component_id) {
            return &endpoint;
        }
    }
    return nullptr;
}

} // namespace

int main() {
    const fs::path image_path =
        fs::path(DX_WIRE_SOURCE_DIR) / "samples" / "trx300ODG.png";

    if (!fs::exists(image_path)) {
        std::cerr << "missing source image: " << image_path << "\n";
        return 1;
    }

    // Same source_id convention as the canonical dx-extract CLI - see
    // tests/test_rejected_geometry_handoff.cpp for why this must match
    // exactly (every ID is content-addressed from source_id, not the
    // resolved filesystem path).
    ExtractionPipeline pipeline;
    const WireModel model =
        pipeline.run(image_path.string(), "samples/trx300ODG.png");

    const std::set<std::string> genuine_chassis_ground = {
        "component-candidate-shape-region-0edf5ce35037fec3",
        "component-candidate-shape-region-1acbaeb7ac6ac87a",
        "component-candidate-shape-region-a434a925670e9b65",
        "component-candidate-shape-region-791276441805b2e0",
        "component-candidate-shape-region-5aa211846bb7891d",
        "component-candidate-shape-region-6b6ccc2d59afe578",
    };

    int resolved_count = 0;
    for (const auto& component_id : genuine_chassis_ground) {
        if (ground_endpoint_for(model, component_id) != nullptr) {
            ++resolved_count;
        }
    }
    assert(resolved_count == 6);

    // CASE 1: diagonal-jog ground approach.
    {
        const std::string component_id =
            "component-candidate-shape-region-5aa211846bb7891d";
        const EndpointCandidate* ground = ground_endpoint_for(model, component_id);
        assert(ground != nullptr);
        assert(ground->confidence == ConfidenceClass::High);

        // The recovered geometry is connected to the ground-symbol
        // approach: an accepted conductor segment with real (non-zero)
        // length terminates at the ground symbol's own anchor point
        // (719, 547) - the recovered path, not a fabricated single point.
        bool found_recovery_evidence = false;
        for (const auto& segment : model.conductor_segments) {
            const bool touches_anchor =
                (segment.geometry.a.x == 719.0 && segment.geometry.a.y == 547.0) ||
                (segment.geometry.b.x == 719.0 && segment.geometry.b.y == 547.0);
            if (touches_anchor && segment.geometry.length() > 0.0) {
                found_recovery_evidence = true;
                break;
            }
        }
        assert(found_recovery_evidence);
    }

    // CASE 2: short L-bend / vertical ground approach.
    {
        const std::string component_id =
            "component-candidate-shape-region-6b6ccc2d59afe578";
        const EndpointCandidate* ground = ground_endpoint_for(model, component_id);
        assert(ground != nullptr);
        assert(ground->confidence == ConfidenceClass::High);
    }

    // No unrelated Wire is created: every one of the 4 previously
    // resolved ground symbols still resolves to exactly the same
    // endpoint coordinates and confidence as established by
    // AP-DIAG-AUDIT-003/006 (regression safety for the successful cases).
    struct KnownGood {
        std::string component_id;
        double x;
        double y;
        ConfidenceClass confidence;
    };
    const KnownGood known_good[] = {
        {"component-candidate-shape-region-0edf5ce35037fec3", 585.5, 585,
         ConfidenceClass::High},
        {"component-candidate-shape-region-1acbaeb7ac6ac87a", 629.5, 585,
         ConfidenceClass::High},
        {"component-candidate-shape-region-a434a925670e9b65", 935, 542,
         ConfidenceClass::Medium},
        {"component-candidate-shape-region-791276441805b2e0", 656, 546,
         ConfidenceClass::High},
    };
    for (const auto& expected : known_good) {
        const EndpointCandidate* ground =
            ground_endpoint_for(model, expected.component_id);
        assert(ground != nullptr);
        assert(ground->position.x == expected.x);
        assert(ground->position.y == expected.y);
        assert(ground->confidence == expected.confidence);
    }

    // All 6 genuine ChassisGround recognitions still exist and no new
    // one was fabricated.
    int chassis_ground_count = 0;
    for (const auto& recognition : model.component_symbol_recognitions) {
        if (recognition.symbol_kind == ComponentSymbolKind::ChassisGround) {
            ++chassis_ground_count;
        }
    }
    assert(chassis_ground_count == 6);

    std::cout << "ground approach recovery production test passed\n";
    return 0;
}

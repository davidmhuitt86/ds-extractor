// AP-DIAG-FIX-013: production-path regression for connector interaction evidence.
// This test exercises the canonical image through ExtractionPipeline and checks
// that every emitted crossing record retains the interaction classification
// calculated from conductor endpoint geometry. Unknown here means the pipeline
// dropped the classification before constructing the observation record.

#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

int main() {
    const fs::path image_path =
        fs::path(DX_WIRE_SOURCE_DIR) / "samples" / "trx300ODG.png";

    if (!fs::exists(image_path)) {
        std::cerr << "missing source image: " << image_path << "\n";
        return 1;
    }

    ExtractionPipeline pipeline;
    const WireModel model =
        pipeline.run(image_path.string(), "samples/trx300ODG.png");

    if (model.connector_pins.empty() ||
        model.connector_conductor_crossing_evidence.empty()) {
        std::cerr
            << "canonical sample produced no connector pins/crossing evidence; "
            << "propagation regression cannot be evaluated\n";
        return 1;
    }

    std::size_t pass_through_count = 0;
    std::size_t termination_count = 0;

    for (const auto& evidence : model.connector_conductor_crossing_evidence) {
        switch (evidence.interaction) {
        case ConnectorConductorInteractionKind::PassThrough:
            ++pass_through_count;
            break;
        case ConnectorConductorInteractionKind::Termination:
            ++termination_count;
            break;
        case ConnectorConductorInteractionKind::Unknown:
            std::cerr << "crossing evidence " << evidence.id
                      << " lost its calculated interaction classification\n";
            return 1;
        }
    }

    assert(pass_through_count + termination_count ==
           model.connector_conductor_crossing_evidence.size());

    std::cout
        << "connector interaction propagation test passed: "
        << pass_through_count << " pass-through, "
        << termination_count << " termination\n";
    return 0;
}

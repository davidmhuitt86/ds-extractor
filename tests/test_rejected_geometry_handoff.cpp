// AP-DIAG-FIX-006: production-path regression proving the actual
// ExtractionPipeline handoff of RejectedGeometryEvidence into
// TerminalLocationDetector - not a manually constructed fixture passed
// directly to the detector (AP-DIAG-AUDIT-005 identified that gap
// explicitly: every existing test of TerminalLocationDetector's ownership
// logic bypasses ExtractionPipeline entirely).
//
// Before AP-DIAG-FIX-006: extraction_pipeline.cpp moved the local
// rejected_geometry vector into model.rejected_geometry BEFORE passing it
// (now moved-from, empirically empty on this build - AP-DIAG-AUDIT-005)
// into TerminalLocationDetector::detect(). The target endpoint's sole
// ownership evidence is RejectedGeometryEvidence entries (its owning
// component has zero SymbolPrimitives), so it resolved to
// geometric/unresolved instead of component_terminal.
//
// This test runs the real, unmodified ExtractionPipeline against the real
// canonical TRX300 sample and asserts the corrected result directly from
// its output - it fails against the pre-fix ordering and passes after the
// one-statement reorder.
//
// AP-DIAG-017: samples/trx300ODG.png was permanently re-cropped (removing
// the switch matrix region so it no longer needs masking before every
// extraction run). This is a deliberate re-baseline, not a guess:
// endpoint-candidate-f116d72e55f48cc6 was located by building this exact
// codebase, running the real ExtractionPipeline against the new cropped
// image, and independently confirming it is still an endpoint whose
// owning component (component-candidate-shape-region-c8c5b83bef6a9843)
// owns zero SymbolPrimitives and is resolved exclusively via
// RejectedGeometryEvidence - the same real-world code path this test
// exists to guard.

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

    // The canonical extraction (dx-extract) invokes
    // ExtractionPipeline::run(image_path, source_id) with the literal CLI
    // argument "samples/trx300ODG.png" for BOTH the file to load and the
    // source_id used to content-address every generated ID (stable_id
    // hashes source_id, not the resolved filesystem path). Reproducing
    // the target ID verbatim requires the exact same source_id string,
    // while still loading the image from this build's own
    // DX_WIRE_SOURCE_DIR-based path so the test is not tied to the working
    // directory ctest happens to run from.
    ExtractionPipeline pipeline;
    const WireModel model =
        pipeline.run(image_path.string(), "samples/trx300ODG.png");

    const std::string target_id = "endpoint-candidate-f116d72e55f48cc6";
    const EndpointCandidate* target = nullptr;
    for (const auto& endpoint : model.endpoint_candidates) {
        if (endpoint.id == target_id) {
            target = &endpoint;
            break;
        }
    }

    if (target == nullptr) {
        std::cerr << target_id << " not found in endpoint_candidates\n";
        return 1;
    }

    // 1-2. rejected_geometry was produced and populated, and the detector
    // actually consumed it: proven by the resulting classification below,
    // which is only reachable via TerminalLocationDetector's
    // RejectedGeometryEvidence ownership-evidence branch (this component
    // owns zero SymbolPrimitives - see AP-DIAG-AUDIT-004/005).
    assert(!model.rejected_geometry.empty());

    // 3-6. The target endpoint is now correctly attributed.
    assert(target->kind == EndpointKind::ComponentTerminal);
    assert(target->confidence == ConfidenceClass::High);
    assert(target->component_id ==
           "component-candidate-shape-region-c8c5b83bef6a9843");

    // No fabricated SymbolPrimitive was introduced to make this pass - the
    // owning component's ownership evidence is exclusively the
    // RejectedGeometryEvidence entries already produced upstream.
    bool component_owns_primitive = false;
    for (const auto& primitive : model.symbol_primitives) {
        if (primitive.component_id == target->component_id) {
            component_owns_primitive = true;
            break;
        }
    }
    assert(!component_owns_primitive);

    bool component_has_rejected_geometry_evidence = false;
    for (const auto& evidence : model.rejected_geometry) {
        if (evidence.associated_object_id == target->component_id) {
            component_has_rejected_geometry_evidence = true;
            break;
        }
    }
    assert(component_has_rejected_geometry_evidence);

    std::cout << "rejected geometry handoff test passed\n";
    return 0;
}

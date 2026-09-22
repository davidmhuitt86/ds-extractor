#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/image/morphology_detector.hpp"
#include "eke_dx_wire/image/conductor_normalizer.hpp"
#include "eke_dx_wire/image/conductor_evidence_evaluator.hpp"
#include "eke_dx_wire/image/rejected_geometry_classifier.hpp"
#include "eke_dx_wire/image/geometry_ownership_classifier.hpp"
#include "eke_dx_wire/topology/topology_reconstructor.hpp"
#include "eke_dx_wire/topology/gap_interpreter.hpp"
#include "eke_dx_wire/image/shape_detector.hpp"
#include "eke_dx_wire/image/text_region_detector.hpp"
#include "eke_dx_wire/topology/terminal_location_detector.hpp"
#include "eke_dx_wire/topology/distribution_decomposer.hpp"

#include <string>

namespace eke::dx::wire {

struct ExtractionConfig {
    MorphologyConfig morphology {};
    ShapeDetectorConfig shapes {};
    TextDetectorConfig text {};
    TerminalLocationConfig terminals {};
    GeometryNormalizationConfig geometry {};
    ConductorEvidenceConfig conductor_evidence {};
    GeometryClassificationConfig rejected_geometry_classification {};
    GeometryOwnershipConfig geometry_ownership {};
    TopologyConfig topology {};
    GapInterpretationConfig gap_interpretation {};
    DistributionDecompositionConfig distribution {};
};

class ExtractionPipeline {
public:
    explicit ExtractionPipeline(ExtractionConfig config = {});

    [[nodiscard]] WireModel run(
        const std::string& image_path,
        const std::string& source_id) const;

private:
    ExtractionConfig config_;
};

} // namespace eke::dx::wire

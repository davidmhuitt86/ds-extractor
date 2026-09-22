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
#include "eke_dx_wire/topology/text_recognition_provider.hpp"

#include <memory>
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

    // AP-WIRE-008: recognition is an injectable boundary. The default is
    // deliberately no-op so the extractor never invents OCR results.
    std::shared_ptr<const TextRecognitionProvider> text_recognition_provider;
};

class ExtractionPipeline {
public:
    explicit ExtractionPipeline(ExtractionConfig config = {});

    [[nodiscard]] WireModel run(
        const std::string& image_path,
        const std::string& source_id) const;

private:
    ExtractionConfig config_;
    std::shared_ptr<const TextRecognitionProvider> text_recognition_provider_;
};

} // namespace eke::dx::wire

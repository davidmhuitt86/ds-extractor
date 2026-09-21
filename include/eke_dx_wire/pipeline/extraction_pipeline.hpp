#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/image/morphology_detector.hpp"
#include "eke_dx_wire/image/conductor_normalizer.hpp"
#include "eke_dx_wire/topology/topology_reconstructor.hpp"
#include "eke_dx_wire/topology/gap_interpreter.hpp"
#include "eke_dx_wire/image/shape_detector.hpp"

#include <string>

namespace eke::dx::wire {

struct ExtractionConfig {
    MorphologyConfig morphology {};
    ShapeDetectorConfig shapes {};
    GeometryNormalizationConfig geometry {};
    TopologyConfig topology {};
    GapInterpretationConfig gap_interpretation {};
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

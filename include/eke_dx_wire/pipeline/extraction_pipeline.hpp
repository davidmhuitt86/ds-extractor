#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/image/morphology_detector.hpp"

#include <string>

namespace eke::dx::wire {

struct ExtractionConfig {
    MorphologyConfig morphology {};
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

#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include "eke_dx_wire/image/image_loader.hpp"
#include "eke_dx_wire/image/normalizer.hpp"

namespace eke::dx::wire {

ExtractionPipeline::ExtractionPipeline(ExtractionConfig config)
    : config_(config) {}

WireModel ExtractionPipeline::run(
    const std::string& image_path,
    const std::string& source_id) const {

    const cv::Mat source = ImageLoader::load(image_path);
    const cv::Mat normalized = ImageNormalizer::normalize(source);

    MorphologyWireDetector detector(config_.morphology);
    const DetectionArtifacts detected =
        detector.detect(normalized, source_id, 0);

    WireModel model;
    model.source_id = source_id;
    model.page = 0;
    model.image_width = normalized.cols;
    model.image_height = normalized.rows;
    model.conductor_segments = detected.conductor_segments;

    // v0.1 ends at observable conductor extraction.
    // Topology and endpoint-to-endpoint wire identity are later stages.
    return model;
}

} // namespace eke::dx::wire

#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include "eke_dx_wire/image/image_loader.hpp"
#include "eke_dx_wire/image/normalizer.hpp"
#include "eke_dx_wire/image/conductor_normalizer.hpp"
#include "eke_dx_wire/topology/topology_reconstructor.hpp"
#include "eke_dx_wire/topology/endpoint_reconstructor.hpp"

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

    ConductorNormalizer normalizer(config_.geometry);
    const std::vector<ConductorSegment> normalized_segments =
        normalizer.normalize(detected.conductor_segments);

    TopologyReconstructor topology(config_.topology);
    const TopologyArtifacts graph =
        topology.reconstruct(normalized_segments, source_id, 0);

    EndpointReconstructor endpoints;
    const EndpointArtifacts endpoint_artifacts =
        endpoints.reconstruct(graph.nodes, graph.edges, source_id, 0);

    WireModel model;
    model.source_id = source_id;
    model.page = 0;
    model.image_width = normalized.cols;
    model.image_height = normalized.rows;
    model.conductor_segments = normalized_segments;
    model.nodes = graph.nodes;
    model.edges = graph.edges;
    model.endpoint_candidates = endpoint_artifacts.candidates;

    // Endpoint candidates are not yet true engineering endpoints. They are
    // geometric evidence that a conductor terminates in the extracted graph.
    // Component, connector, splice, ground, and external-connection evidence
    // must be resolved before constructing Wire objects.
    return model;
}

} // namespace eke::dx::wire

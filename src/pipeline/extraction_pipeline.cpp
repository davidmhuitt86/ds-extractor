#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include "eke_dx_wire/image/image_loader.hpp"
#include "eke_dx_wire/image/normalizer.hpp"
#include "eke_dx_wire/image/conductor_normalizer.hpp"
#include "eke_dx_wire/image/shape_detector.hpp"
#include "eke_dx_wire/topology/topology_reconstructor.hpp"
#include "eke_dx_wire/topology/endpoint_reconstructor.hpp"
#include "eke_dx_wire/topology/gap_interpreter.hpp"

namespace eke::dx::wire {

ExtractionPipeline::ExtractionPipeline(ExtractionConfig config)
    : config_(config) {}

WireModel ExtractionPipeline::run(
    const std::string& image_path,
    const std::string& source_id) const {

    const cv::Mat source = ImageLoader::load(image_path);
    const cv::Mat normalized = ImageNormalizer::normalize(source);

    ShapeDetector shape_detector;
    const ShapeDetectionArtifacts shapes =
        shape_detector.detect(normalized, source_id, 0);

    MorphologyWireDetector detector(config_.morphology);
    DetectionArtifacts detected =
        detector.detect(
            normalized, source_id, 0, shapes.exclusion_mask);
    detected.shapes = shapes;

    ConductorNormalizer normalizer(config_.geometry);
    const std::vector<ConductorSegment> normalized_segments =
        normalizer.normalize(detected.conductor_segments);

    TopologyReconstructor topology(config_.topology);
    TopologyArtifacts graph =
        topology.reconstruct(normalized_segments, source_id, 0);

    GapInterpreter gap_interpreter(config_.gap_interpretation);
    const GapInterpretationArtifacts gap_artifacts =
        gap_interpreter.interpret(
            graph.nodes, graph.edges, normalized, source_id, 0);

    graph.edges.insert(
        graph.edges.end(),
        gap_artifacts.inferred_edges.begin(),
        gap_artifacts.inferred_edges.end());

    EndpointReconstructor endpoints;
    const EndpointArtifacts endpoint_artifacts =
        endpoints.reconstruct(
            graph.nodes, graph.edges, normalized, source_id, 0);

    WireModel model;
    model.source_id = source_id;
    model.page = 0;
    model.image_width = normalized.cols;
    model.image_height = normalized.rows;
    model.conductor_segments = normalized_segments;
    model.nodes = graph.nodes;
    model.edges = graph.edges;
    model.endpoint_candidates = endpoint_artifacts.candidates;

    // Endpoint candidates remain evidence-bearing geometric candidates.
    // Semantic component/connector/splice/ground classification and Wire
    // construction remain later stages.
    return model;
}

} // namespace eke::dx::wire

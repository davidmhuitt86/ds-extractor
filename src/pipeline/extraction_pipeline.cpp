#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include "eke_dx_wire/image/image_loader.hpp"
#include "eke_dx_wire/image/normalizer.hpp"
#include "eke_dx_wire/image/conductor_normalizer.hpp"
#include "eke_dx_wire/image/shape_detector.hpp"
#include "eke_dx_wire/image/component_candidate_classifier.hpp"
#include "eke_dx_wire/topology/topology_reconstructor.hpp"
#include "eke_dx_wire/topology/endpoint_reconstructor.hpp"
#include "eke_dx_wire/topology/gap_interpreter.hpp"
#include "eke_dx_wire/topology/wire_reconstructor.hpp"
#include "eke_dx_wire/topology/distribution_decomposer.hpp"

#include <algorithm>

namespace eke::dx::wire {

ExtractionPipeline::ExtractionPipeline(ExtractionConfig config)
    : config_(config) {}

WireModel ExtractionPipeline::run(
    const std::string& image_path,
    const std::string& source_id) const {

    const cv::Mat source = ImageLoader::load(image_path);
    const cv::Mat normalized = ImageNormalizer::normalize(source);

    ShapeDetector shape_detector(config_.shapes);
    const ShapeDetectionArtifacts shapes =
        shape_detector.detect(normalized, source_id, 0);

    MorphologyWireDetector detector(config_.morphology);
    DetectionArtifacts detected =
        detector.detect(
            normalized, source_id, 0, shapes.exclusion_mask);
    detected.shapes = shapes;

    ComponentCandidateClassifier component_classifier;
    const std::vector<ComponentCandidate> component_candidates =
        component_classifier.classify(shapes);

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
    model.component_candidates = component_candidates;
    model.conductor_segments = normalized_segments;
    model.nodes = graph.nodes;
    model.edges = graph.edges;
    model.endpoint_candidates = endpoint_artifacts.candidates;

    WireReconstructor wire_reconstructor;
    const WireReconstructionArtifacts wire_artifacts =
        wire_reconstructor.reconstruct(
            graph.nodes,
            graph.edges,
            endpoint_artifacts.candidates,
            normalized_segments,
            source_id,
            0);
    model.wires = wire_artifacts.wires;

    DistributionDecomposer distribution_decomposer(config_.distribution);
    const DistributionDecompositionArtifacts distribution_artifacts =
        distribution_decomposer.decompose(
            graph.nodes,
            graph.edges,
            endpoint_artifacts.candidates,
            normalized_segments,
            source_id,
            0);

    model.electrical_nets = distribution_artifacts.nets;
    model.wires.insert(
        model.wires.end(),
        distribution_artifacts.wires.begin(),
        distribution_artifacts.wires.end());

    std::sort(
        model.wires.begin(),
        model.wires.end(),
        [](const Wire& a, const Wire& b) {
            return a.id < b.id;
        });

    return model;
}

} // namespace eke::dx::wire

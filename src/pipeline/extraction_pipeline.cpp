#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include "eke_dx_wire/image/image_loader.hpp"
#include "eke_dx_wire/core/extraction_audit.hpp"
#include "eke_dx_wire/image/normalizer.hpp"
#include "eke_dx_wire/image/conductor_normalizer.hpp"
#include "eke_dx_wire/image/conductor_evidence_evaluator.hpp"
#include "eke_dx_wire/image/rejected_geometry_classifier.hpp"
#include "eke_dx_wire/image/shape_detector.hpp"
#include "eke_dx_wire/image/component_candidate_classifier.hpp"
#include "eke_dx_wire/image/text_region_detector.hpp"

#include <opencv2/core.hpp>
#include "eke_dx_wire/topology/topology_reconstructor.hpp"
#include "eke_dx_wire/topology/endpoint_reconstructor.hpp"
#include "eke_dx_wire/topology/gap_interpreter.hpp"
#include "eke_dx_wire/topology/wire_reconstructor.hpp"
#include "eke_dx_wire/topology/distribution_decomposer.hpp"
#include "eke_dx_wire/topology/terminal_location_detector.hpp"
#include "eke_dx_wire/topology/terminal_semantic_evidence_builder.hpp"
#include "eke_dx_wire/topology/terminal_semantic_resolver.hpp"
#include "eke_dx_wire/topology/circuit_role_resolver.hpp"
#include "eke_dx_wire/topology/topology_semantic_resolver.hpp"
#include "eke_dx_wire/topology/wire_model_validator.hpp"

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

    TextRegionDetector text_detector(config_.text);
    const TextDetectionArtifacts text_regions =
        text_detector.detect(normalized, source_id, 0);

    cv::Mat combined_exclusion = shapes.exclusion_mask.clone();
    if (combined_exclusion.empty()) {
        combined_exclusion = cv::Mat::zeros(
            normalized.size(), CV_8UC1);
    }
    if (!text_regions.exclusion_mask.empty()) {
        cv::bitwise_or(
            combined_exclusion,
            text_regions.exclusion_mask,
            combined_exclusion);
    }

    MorphologyWireDetector detector(config_.morphology);
    DetectionArtifacts detected =
        detector.detect(
            normalized, source_id, 0, combined_exclusion);
    detected.shapes = shapes;

    ComponentCandidateClassifier component_classifier;
    const std::vector<ComponentCandidate> component_candidates =
        component_classifier.classify(shapes);

    ConductorEvidenceEvaluator evidence_evaluator(config_.conductor_evidence);
    const ConductorEvidenceArtifacts evidence =
        evidence_evaluator.evaluate(
            detected.conductor_segments,
            normalized);

    std::vector<RejectedGeometryEvidence> rejected_geometry =
        evidence.rejected;
    RejectedGeometryClassifier rejected_classifier(
        config_.rejected_geometry_classification);
    rejected_classifier.classify(
        rejected_geometry,
        component_candidates,
        text_regions.regions);

    ConductorNormalizer normalizer(config_.geometry);
    const std::vector<ConductorSegment> normalized_segments =
        normalizer.normalize(evidence.accepted);

    TopologyReconstructor topology(config_.topology);
    TopologyArtifacts graph =
        topology.reconstruct(normalized_segments, source_id, 0);

    TopologySemanticResolver topology_semantic_resolver;
    graph.nodes = topology_semantic_resolver.resolve(
        graph.nodes,
        {});

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
    model.text_regions = text_regions.regions;
    model.conductor_segments = normalized_segments;
    model.rejected_geometry = std::move(rejected_geometry);
    model.nodes = graph.nodes;
    model.edges = graph.edges;
    model.endpoint_candidates = endpoint_artifacts.candidates;

    TerminalLocationDetector terminal_detector(config_.terminals);
    const TerminalLocationArtifacts terminal_artifacts =
        terminal_detector.detect(
            component_candidates,
            endpoint_artifacts.candidates);
    model.terminal_candidates = terminal_artifacts.candidates;

    // AP-SEMANTIC-001: convert independently located terminal candidates
    // into semantic evidence, then resolve endpoint identity before any
    // downstream stage consumes endpoint kinds.
    TerminalSemanticEvidenceBuilder semantic_evidence_builder;
    const std::vector<TerminalSemanticEvidence> semantic_evidence =
        semantic_evidence_builder.build(model.terminal_candidates);

    TerminalSemanticResolver semantic_resolver;
    model.endpoint_candidates =
        semantic_resolver.resolve(
            endpoint_artifacts.candidates,
            semantic_evidence);

    WireReconstructor wire_reconstructor;
    const WireReconstructionArtifacts wire_artifacts =
        wire_reconstructor.reconstruct(
            graph.nodes,
            graph.edges,
            model.endpoint_candidates,
            normalized_segments,
            source_id,
            0);
    model.wires = wire_artifacts.wires;

    DistributionDecomposer distribution_decomposer(config_.distribution);
    const DistributionDecompositionArtifacts distribution_artifacts =
        distribution_decomposer.decompose(
            graph.nodes,
            graph.edges,
            model.endpoint_candidates,
            normalized_segments,
            source_id,
            0);

    CircuitRoleResolver circuit_role_resolver;
    const CircuitRoleResolutionArtifacts role_artifacts =
        circuit_role_resolver.resolve(
            distribution_artifacts.nets,
            model.endpoint_candidates,
            {});

    model.electrical_nets = role_artifacts.nets;
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

    WireModelValidator validator;
    model.wire_validation = validator.validate(model);
    model.audit = build_extraction_audit(
        model,
        gap_artifacts.inferred_edges.size());

    return model;
}

} // namespace eke::dx::wire

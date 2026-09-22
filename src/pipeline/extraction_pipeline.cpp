#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"

#include "eke_dx_wire/image/image_loader.hpp"
#include "eke_dx_wire/core/extraction_audit.hpp"
#include "eke_dx_wire/image/normalizer.hpp"
#include "eke_dx_wire/image/conductor_normalizer.hpp"
#include "eke_dx_wire/image/conductor_evidence_evaluator.hpp"
#include "eke_dx_wire/image/rejected_geometry_classifier.hpp"
#include "eke_dx_wire/image/geometry_ownership_classifier.hpp"
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
#include "eke_dx_wire/topology/circuit_role_evidence_builder.hpp"
#include "eke_dx_wire/topology/semantic_evidence_associator.hpp"
#include "eke_dx_wire/topology/text_evidence_interpreter.hpp"
#include "eke_dx_wire/topology/text_recognition_provider.hpp"
#include "eke_dx_wire/topology/topology_semantic_resolver.hpp"
#include "eke_dx_wire/topology/wire_model_validator.hpp"

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <utility>

namespace eke::dx::wire {

ExtractionPipeline::ExtractionPipeline(ExtractionConfig config)
    : config_(std::move(config)),
      text_recognition_provider_(
          config_.text_recognition_provider
              ? config_.text_recognition_provider
              : std::make_shared<NullTextRecognitionProvider>()) {}

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

    // AP-GEOMETRY-006: determine whether line-like geometry is actually
    // owned by a graphical object before it can enter conductor topology.
    // Endpoint proximity alone is not ownership; real wires commonly
    // terminate at component and connector boundaries.
    GeometryOwnershipClassifier ownership_classifier(config_.geometry_ownership);
    const GeometryOwnershipArtifacts ownership =
        ownership_classifier.classify(
            detected.conductor_segments,
            component_candidates,
            text_regions.regions);

    ConductorEvidenceEvaluator evidence_evaluator(config_.conductor_evidence);
    const ConductorEvidenceArtifacts evidence =
        evidence_evaluator.evaluate(
            ownership.conductor_candidates,
            normalized);

    std::vector<RejectedGeometryEvidence> rejected_geometry =
        ownership.rejected;
    rejected_geometry.insert(
        rejected_geometry.end(),
        evidence.rejected.begin(),
        evidence.rejected.end());

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

    // AP-WIRE-008: recognition is an explicit provider boundary. The
    // provider receives detected regions plus the normalized source image
    // and returns only recognized-text evidence. It cannot mutate topology.
    const std::vector<TextRecognitionEvidence> recognized =
        text_recognition_provider_->recognize(
            normalized,
            model.text_regions,
            source_id,
            0);

    std::unordered_set<std::string> known_text_regions;
    known_text_regions.reserve(model.text_regions.size());
    for (const auto& region : model.text_regions) {
        known_text_regions.insert(region.id);
    }

    const std::string provider_id =
        text_recognition_provider_->provider_id();

    for (const auto& evidence : recognized) {
        // Provider output is evidence, not authority. The pipeline only
        // accepts observations that refer to an actual detected text region
        // and contain usable recognition confidence/text.
        if (evidence.text_region_id.empty() ||
            evidence.raw_text.empty() ||
            evidence.confidence == ConfidenceClass::Unresolved ||
            known_text_regions.find(evidence.text_region_id) ==
                known_text_regions.end()) {
            continue;
        }

        TextRecognitionEvidence accepted = evidence;
        if (accepted.provider.empty()) {
            accepted.provider = provider_id;
        }
        model.text_recognition_evidence.push_back(std::move(accepted));
    }

    std::sort(
        model.text_recognition_evidence.begin(),
        model.text_recognition_evidence.end(),
        [](const TextRecognitionEvidence& a, const TextRecognitionEvidence& b) {
            if (a.text_region_id != b.text_region_id) {
                return a.text_region_id < b.text_region_id;
            }
            if (a.raw_text != b.raw_text) {
                return a.raw_text < b.raw_text;
            }
            if (a.confidence != b.confidence) {
                return static_cast<int>(a.confidence) <
                    static_cast<int>(b.confidence);
            }
            return a.provider < b.provider;
        });
    model.conductor_segments = normalized_segments;
    model.rejected_geometry = std::move(rejected_geometry);
    model.nodes = graph.nodes;
    model.edges = graph.edges;
    model.endpoint_candidates = endpoint_artifacts.candidates;

    TerminalLocationDetector terminal_detector(config_.terminals);
    const TerminalLocationArtifacts terminal_artifacts =
        terminal_detector.detect(
            component_candidates,
            endpoint_artifacts.candidates,
            rejected_geometry);
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

    // AP-WIRE-006/AP-WIRE-008: associations are created only after endpoint
    // reconstruction and semantic endpoint resolution, so recognized text
    // can be spatially related to the actual endpoint objects.
    SemanticEvidenceAssociator semantic_associator;
    model.semantic_associations = semantic_associator.associate(
        model.text_regions,
        model.component_candidates,
        model.endpoint_candidates);

    TextEvidenceInterpreter text_interpreter;
    model.text_semantic_evidence = text_interpreter.interpret(
        model.text_recognition_evidence);

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

    // AP-NETWORK-002: circuit-role inference consumes explicit semantic
    // evidence, not topology shape or endpoint count. At this stage the
    // builder can only promote roles already established on endpoints.
    // Future symbol/text/function interpretation stages may contribute
    // additional CircuitRoleEvidence without changing the resolver.
    CircuitRoleEvidenceBuilder role_evidence_builder;
    const std::vector<CircuitRoleEvidence> role_evidence =
        role_evidence_builder.build(model.endpoint_candidates);

    CircuitRoleResolver circuit_role_resolver;
    const CircuitRoleResolutionArtifacts role_artifacts =
        circuit_role_resolver.resolve(
            distribution_artifacts.nets,
            model.endpoint_candidates,
            role_evidence);

    model.electrical_nets = role_artifacts.nets;

    // AP-WIRE-004: ordinary endpoint-to-endpoint reconstruction and
    // distribution decomposition may discover the same physical path from
    // different traversal strategies. Wire identity is deterministic from
    // its source/page/endpoints, so duplicate IDs represent the same wire
    // artifact and must not be emitted twice.
    std::unordered_set<std::string> emitted_wire_ids;
    emitted_wire_ids.reserve(
        model.wires.size() + distribution_artifacts.wires.size());

    for (const auto& wire : model.wires) {
        emitted_wire_ids.insert(wire.id);
    }

    for (const auto& wire : distribution_artifacts.wires) {
        if (emitted_wire_ids.insert(wire.id).second) {
            model.wires.push_back(wire);
        }
    }

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

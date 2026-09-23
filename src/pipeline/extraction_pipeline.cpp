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
#include "eke_dx_wire/topology/endpoint_semantic_reconstructor.hpp"
#include "eke_dx_wire/topology/connector_terminal_model.hpp"
#include "eke_dx_wire/topology/circuit_role_resolver.hpp"
#include "eke_dx_wire/topology/circuit_role_evidence_builder.hpp"
#include "eke_dx_wire/topology/semantic_evidence_associator.hpp"
#include "eke_dx_wire/topology/semantic_observation_resolver.hpp"
#include "eke_dx_wire/topology/engineering_object_semantic_resolver.hpp"
#include "eke_dx_wire/topology/engineering_object_semantic_applier.hpp"
#include "eke_dx_wire/topology/component_identity_evidence_builder.hpp"
#include "eke_dx_wire/topology/component_identity_resolver.hpp"
#include "eke_dx_wire/topology/component_identity_registry.hpp"
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
              : std::make_shared<NullTextRecognitionProvider>()),
      component_identity_registry_(
          config_.component_identity_registry
              ? config_.component_identity_registry
              : std::make_shared<NullComponentIdentityRegistry>()) {}

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

    for (const auto& recognition_evidence : recognized) {
        // Provider output is evidence, not authority. The pipeline only
        // accepts observations that refer to an actual detected text region
        // and contain usable recognition confidence/text.
        if (recognition_evidence.text_region_id.empty() ||
            recognition_evidence.raw_text.empty() ||
            recognition_evidence.confidence == ConfidenceClass::Unresolved ||
            known_text_regions.find(recognition_evidence.text_region_id) ==
                known_text_regions.end()) {
            continue;
        }

        TextRecognitionEvidence accepted = recognition_evidence;
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

    // AP-WIRE-019: reconstruct endpoint semantic identity from all
    // terminal evidence without selecting a winner when evidence conflicts.
    EndpointSemanticReconstructor endpoint_semantic_reconstructor;
    const EndpointSemanticReconstructionArtifacts endpoint_semantic_artifacts =
        endpoint_semantic_reconstructor.reconstruct(
            endpoint_artifacts.candidates,
            semantic_evidence);
    model.endpoint_candidates = endpoint_semantic_artifacts.endpoints;
    model.endpoint_semantic_reconstructions =
        endpoint_semantic_artifacts.reconstructions;

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

    // AP-WIRE-014: resolve recognized semantic observations to the nearest
    // engineering object without mutating geometry or topology. Ambiguous
    // associations remain unresolved and are omitted from the resolved set.
    EngineeringObjectSemanticResolver object_semantic_resolver;
    model.engineering_object_semantics = object_semantic_resolver.resolve(
        model.text_semantic_evidence,
        model.semantic_associations);

    // AP-WIRE-015: materialize resolved labels into engineering-object
    // semantic fields. This enriches the model only; geometry, topology,
    // wire identity, and electrical connectivity remain unchanged.
    EngineeringObjectSemanticApplier object_semantic_applier;
    object_semantic_applier.apply(
        model.component_candidates,
        model.endpoint_candidates,
        model.engineering_object_semantics);

    // AP-WIRE-020: materialize explicit connector and connector-terminal
    // objects only from already established connector-boundary evidence.
    // This stage does not infer connector identity, pin numbers, or topology.
    ConnectorTerminalModelBuilder connector_terminal_builder;
    const ConnectorModelArtifacts connector_artifacts =
        connector_terminal_builder.build(
            model.component_candidates,
            model.terminal_candidates,
            model.endpoint_candidates);
    model.connector_candidates = connector_artifacts.connectors;
    model.connector_terminals = connector_artifacts.terminals;

    // AP-WIRE-016: preserve component/connector labels as explicit
    // identity-bearing evidence. This is evidence only; canonical component
    // identity remains unresolved until a registry-backed resolver exists.
    ComponentIdentityEvidenceBuilder component_identity_builder;
    model.component_identity_evidence =
        component_identity_builder.build(
            model.engineering_object_semantics);

    // AP-WIRE-017: resolve unambiguous component labels into explicit
    // engineering identity while preserving conflicts as unresolved.
    ComponentIdentityResolver component_identity_resolver;
    model.component_identity_resolutions =
        component_identity_resolver.resolve(
            model.component_identity_evidence);

    // AP-WIRE-018: canonicalize only identities explicitly resolved by
    // AP-WIRE-017. Registry misses and conflicts remain explicit artifacts;
    // topology and wire identity are never mutated by registry lookup.
    ComponentIdentityCanonicalizer component_identity_canonicalizer;
    model.component_identity_canonicalizations =
        component_identity_canonicalizer.canonicalize(
            model.component_identity_resolutions,
            *component_identity_registry_);

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

    // AP-WIRE-012: recognized role-bearing text is resolved through its
    // spatial endpoint association before it is allowed to influence
    // electrical-net role resolution.
    SemanticObservationResolver observation_resolver;
    const std::vector<CircuitRoleEvidence> observation_evidence =
        observation_resolver.resolve(
            model.text_semantic_evidence,
            model.semantic_associations);

    // AP-NETWORK-002/AP-WIRE-012: circuit-role inference consumes explicit
    // endpoint semantics plus deterministically resolved text observations.
    CircuitRoleEvidenceBuilder role_evidence_builder;
    std::vector<CircuitRoleEvidence> role_evidence =
        role_evidence_builder.build(model.endpoint_candidates);
    role_evidence.insert(
        role_evidence.end(),
        observation_evidence.begin(),
        observation_evidence.end());

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

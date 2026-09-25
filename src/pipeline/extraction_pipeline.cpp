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
#include "eke_dx_wire/image/diagram_furniture_classifier.hpp"
#include "eke_dx_wire/image/symbol_geometry_extractor.hpp"
#include "eke_dx_wire/image/text_region_detector.hpp"

#include <opencv2/core.hpp>
#include "eke_dx_wire/topology/topology_reconstructor.hpp"
#include "eke_dx_wire/topology/endpoint_reconstructor.hpp"
#include "eke_dx_wire/topology/gap_interpreter.hpp"
#include "eke_dx_wire/topology/wire_reconstructor.hpp"
#include "eke_dx_wire/topology/physical_wire_identity_reconstructor.hpp"
#include "eke_dx_wire/topology/wire_identity_key.hpp"
#include "eke_dx_wire/topology/terminal_location_detector.hpp"
#include "eke_dx_wire/topology/terminal_recognizer.hpp"
#include "eke_dx_wire/topology/terminal_semantic_evidence_builder.hpp"
#include "eke_dx_wire/topology/endpoint_semantic_reconstructor.hpp"
#include "eke_dx_wire/topology/connector_terminal_model.hpp"
#include "eke_dx_wire/topology/conductor_boundary_resolver.hpp"
#include "eke_dx_wire/topology/component_symbol_recognizer.hpp"
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
#include "eke_dx_wire/topology/wire_semantic_resolver.hpp"
#include "eke_dx_wire/topology/symbol_family_recognizer.hpp"
#include "eke_dx_wire/topology/electrical_net_resolver.hpp"

#include <algorithm>
#include <map>
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
              : std::make_shared<NullComponentIdentityRegistry>()),
      symbol_recognition_provider_(
          config_.symbol_recognition_provider
              ? config_.symbol_recognition_provider
              : std::make_shared<NullSymbolRecognitionProvider>()) {}

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
    // AP-GEOMETRY: legend/color-key tables, switch-continuity charts, and
    // other tabular diagram content are drawn with the same small
    // circle/rectangle primitives as real circuit symbols, so this
    // re-tags grid-arranged candidates as DiagramFurniture before any
    // downstream stage treats them as circuit components.
    DiagramFurnitureClassifier furniture_classifier(config_.diagram_furniture);
    const std::vector<ComponentCandidate> component_candidates =
        furniture_classifier.classify(component_classifier.classify(shapes));

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

    // AP-WIRE-021: establish explicit symbol-recognition objects from the
    // already classified component candidates. This stage does not infer
    // component identity, mutate topology, or invent terminals.
    ComponentSymbolRecognizer symbol_recognizer;
    model.component_symbol_recognitions =
        symbol_recognizer.recognize(model.component_candidates);

    // AP-WIRE-023: extract internal symbol geometry from each real
    // (non-DiagramFurniture) component's already-detected region. This
    // stage observes geometry only; it does not assign symbol identity,
    // create endpoints, or touch topology/wires/electrical nets.
    SymbolGeometryExtractor symbol_geometry_extractor(config_.symbol_geometry);
    const SymbolGeometryExtractionArtifacts symbol_geometry_artifacts =
        symbol_geometry_extractor.extract(
            normalized, model.component_candidates, normalized_segments,
            source_id, 0);
    model.component_symbol_geometries = symbol_geometry_artifacts.geometries;
    model.symbol_primitives = symbol_geometry_artifacts.primitives;

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

    // AP-WIRE-024: recognize additional component-terminal associations
    // from AP-WIRE-023 internal geometry and conservative conductor-to-
    // component boundary alignment. Existing endpoint objects are the only
    // objects that may be associated; this stage never creates endpoints or
    // mutates topology, wires, or electrical nets.
    TerminalRecognizer terminal_recognizer(config_.terminal_recognition);
    const TerminalRecognitionArtifacts recognition_artifacts =
        terminal_recognizer.recognize(
            model.component_candidates,
            model.component_symbol_geometries,
            model.symbol_primitives,
            model.endpoint_candidates,
            model.nodes,
            model.edges,
            model.terminal_candidates);
    model.terminal_candidates.insert(
        model.terminal_candidates.end(),
        recognition_artifacts.candidates.begin(),
        recognition_artifacts.candidates.end());
    std::sort(
        model.terminal_candidates.begin(),
        model.terminal_candidates.end(),
        [](const TerminalCandidate& a, const TerminalCandidate& b) {
            return a.id < b.id;
        });

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

    // AP-WIRE-030: resolve each endpoint's Conductor Boundary and
    // engineering-terminal association from already-produced evidence
    // (TerminalCandidate, EndpointSemanticReconstruction, ConnectorTerminal)
    // only. This stage creates no topology, no components, no terminals,
    // and no Wires; component association and terminal identity remain
    // independently tracked per AP-WIRE-030 Sec 15.
    ConductorBoundaryResolver conductor_boundary_resolver;
    const ConductorBoundaryResolutionArtifacts conductor_boundary_artifacts =
        conductor_boundary_resolver.resolve(
            model.endpoint_candidates,
            model.terminal_candidates,
            model.endpoint_semantic_reconstructions,
            model.connector_terminals);
    model.conductor_boundary_evidence = conductor_boundary_artifacts.evidence;
    model.conductor_boundary_resolutions =
        conductor_boundary_artifacts.resolutions;

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

    // AP-WIRE-026A: symbol-family recognition consumes AP-WIRE-023 symbol
    // geometry plus already-resolved component identity text (never a
    // label alone) and optional provider observations. It creates no
    // endpoints/terminals/connectors and never touches topology, wires,
    // or electrical nets.
    const std::vector<SymbolRecognitionObservation> symbol_observations =
        symbol_recognition_provider_->recognize(
            model.component_candidates,
            model.component_symbol_geometries,
            model.symbol_primitives,
            source_id,
            0);
    SymbolFamilyRecognizer symbol_family_recognizer;
    const SymbolFamilyRecognitionArtifacts symbol_family_artifacts =
        symbol_family_recognizer.recognize(
            model.component_candidates,
            model.component_symbol_geometries,
            model.component_identity_canonicalizations,
            symbol_observations);
    model.symbol_family_evidence = symbol_family_artifacts.evidence;
    model.symbol_family_resolutions = symbol_family_artifacts.resolutions;

    // AP-WIRE-031: the authoritative physical-Wire-identity reconstruction
    // stage. Internally runs the conservative AP-WIRE-013 WireReconstructor
    // pass, then extends physical identity through Splice/Junction/Crossing
    // nodes only where explicit conductor-segment-sharing evidence
    // justifies it, consuming AP-WIRE-030's Conductor Boundary resolutions
    // to gate which endpoints are eligible. No topology mutation, no
    // ElectricalNet consultation.
    PhysicalWireIdentityReconstructor physical_wire_identity_reconstructor;
    const PhysicalWireIdentityArtifacts wire_artifacts =
        physical_wire_identity_reconstructor.reconstruct(
            graph.nodes,
            graph.edges,
            model.endpoint_candidates,
            normalized_segments,
            model.conductor_boundary_resolutions,
            source_id,
            0);
    model.wires = wire_artifacts.wires;

    // AP-WIRE-012: recognized role-bearing text is resolved through its
    // spatial endpoint association before it is allowed to influence
    // electrical-net role resolution.
    SemanticObservationResolver observation_resolver;
    const std::vector<CircuitRoleEvidence> observation_evidence =
        observation_resolver.resolve(
            model.text_semantic_evidence,
            model.semantic_associations);

    // AP-WIRE-022: resolve electrical-net topology, role evidence, and
    // deterministic net membership through one explicit model boundary.
    ElectricalNetResolver electrical_net_resolver(config_.distribution);
    const ElectricalNetResolutionArtifacts net_artifacts =
        electrical_net_resolver.resolve(
            graph.nodes,
            graph.edges,
            model.endpoint_candidates,
            normalized_segments,
            observation_evidence,
            source_id,
            0);

    model.electrical_nets = net_artifacts.nets;

    // AP-WIRE-004: ordinary endpoint-to-endpoint reconstruction and
    // distribution decomposition may discover the same physical path from
    // different traversal strategies. Wire identity is deterministic from
    // its source/page/endpoints, so duplicate IDs represent the same wire
    // artifact and must not be emitted twice.
    //
    // AP-DIAG-FIX-004: PhysicalWireIdentityReconstructor orders a wire's
    // endpoints "smaller endpoint id first", while DistributionDecomposer
    // (invoked from within ElectricalNetResolver below) always orders them
    // "anchor endpoint first" regardless of lexicographic order. The same
    // physical wire discovered by both therefore gets two different
    // Wire::id values (Wire::id is itself derived from start/end), which
    // let duplicate Wire records through this dedup set when it compared
    // raw ids. Deduplicate on canonical_wire_identity_key(), which
    // normalizes endpoint ordering (and sorts topology_edges/
    // conductor_segments, since reversing traversal direction also
    // reverses path order) so the same physical wire compares equal
    // regardless of which reconstructor discovered it first - see
    // docs/AP-DIAG-FIX-004_Physical_Wire_Record_Deduplication.md.
    std::unordered_set<std::string> emitted_wire_keys;
    emitted_wire_keys.reserve(
        model.wires.size() + net_artifacts.wires.size());

    for (const auto& wire : model.wires) {
        emitted_wire_keys.insert(canonical_wire_identity_key(wire));
    }

    // AP-WIRE-031: DistributionDecomposer (invoked from within
    // ElectricalNetResolver, unmodified by this AP) is a second,
    // pre-existing physical-Wire producer - anchor-based rather than
    // conductor-segment-sharing-based. Its wires are annotated with the
    // same identity_status/identity_evidence_ids contract here at the
    // pipeline merge boundary, without touching ElectricalNetResolver's
    // or DistributionDecomposer's own code: a wire it produces already
    // required a uniquely-identified Ground/ExternalConnection anchor and
    // a tree-shaped electrically-connective path (see
    // DistributionDecomposer::decompose), which is itself explicit
    // evidence, not a guess - so it is annotated Resolved, with
    // provenance to both endpoints' AP-WIRE-030 boundary resolutions.
    std::map<std::string, const ConductorBoundaryResolution*>
        boundary_resolution_by_endpoint;
    for (const auto& resolution : model.conductor_boundary_resolutions) {
        boundary_resolution_by_endpoint.emplace(
            resolution.endpoint_id, &resolution);
    }
    for (auto wire : net_artifacts.wires) {
        if (emitted_wire_keys.insert(canonical_wire_identity_key(wire)).second) {
            wire.identity_status = WireIdentityStatus::Resolved;
            const auto start_it =
                boundary_resolution_by_endpoint.find(wire.start_endpoint);
            if (start_it != boundary_resolution_by_endpoint.end()) {
                wire.identity_evidence_ids.push_back(start_it->second->id);
            }
            const auto end_it =
                boundary_resolution_by_endpoint.find(wire.end_endpoint);
            if (end_it != boundary_resolution_by_endpoint.end()) {
                wire.identity_evidence_ids.push_back(end_it->second->id);
            }
            model.wires.push_back(std::move(wire));
        }
    }

    std::sort(
        model.wires.begin(),
        model.wires.end(),
        [](const Wire& a, const Wire& b) {
            return a.id < b.id;
        });

    // AP-WIRE-025: attach defensible wire semantics from already-existing
    // evidence. This stage is a pure read-only projection - it must run
    // after wires/electrical nets are final and must never feed back into
    // any of the structures above.
    WireSemanticResolver wire_semantic_resolver;
    const WireSemanticResolutionArtifacts wire_semantic_artifacts =
        wire_semantic_resolver.resolve(
            model.wires,
            model.endpoint_candidates,
            model.endpoint_semantic_reconstructions,
            model.connector_terminals,
            model.electrical_nets);
    model.wire_semantics = wire_semantic_artifacts.resolutions;

    WireModelValidator validator;
    model.wire_validation = validator.validate(model);
    model.audit = build_extraction_audit(
        model,
        gap_artifacts.inferred_edges.size());

    return model;
}

} // namespace eke::dx::wire

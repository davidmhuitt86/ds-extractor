// AP-DIAG-AUDIT-008: audit-only diagnostic tool. Runs the unmodified
// ExtractionPipeline and dumps a full evidence inventory for every
// ComponentCandidate, drawn directly from the already-computed WireModel
// (including collections - TerminalCandidate, RejectedGeometryEvidence,
// SemanticAssociation, TextRecognitionEvidence - that the existing
// topology.json/extraction_audit.json exporters do not currently
// serialize). It performs NO interpretation, classification, or
// semantic-identity inference: every field here is a direct, literal
// read of an already-established model collection. It does not modify
// the pipeline, the resolver, or any existing export.
//
// Usage:
//   dx-audit-component-semantic <image> <output.json> [--scope <scope.json>]

#include "eke_dx_wire/pipeline/extraction_pipeline.hpp"
#include "eke_dx_wire/ingest/extraction_scope_io.hpp"
#include "eke_dx_wire/ingest/source_scoper.hpp"
#include "eke_dx_wire/image/image_loader.hpp"

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace eke::dx::wire;

namespace {

std::string json_escape(const std::string& value) {
    std::string result;
    for (const char ch : value) {
        switch (ch) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += ch; break;
        }
    }
    return result;
}

const char* confidence_name(ConfidenceClass c) {
    switch (c) {
    case ConfidenceClass::High: return "high";
    case ConfidenceClass::Medium: return "medium";
    case ConfidenceClass::Low: return "low";
    case ConfidenceClass::Unresolved: return "unresolved";
    }
    return "unresolved";
}

const char* component_candidate_kind_name(ComponentCandidateKind k) {
    switch (k) {
    case ComponentCandidateKind::Enclosure: return "enclosure";
    case ComponentCandidateKind::CircularSymbol: return "circular_symbol";
    case ComponentCandidateKind::ChassisGround: return "chassis_ground";
    case ComponentCandidateKind::PrimitiveSymbol: return "primitive_symbol";
    case ComponentCandidateKind::DiagramFurniture: return "diagram_furniture";
    case ComponentCandidateKind::Unknown: return "unknown";
    }
    return "unknown";
}

const char* symbol_primitive_kind_name(SymbolPrimitiveKind k) {
    switch (k) {
    case SymbolPrimitiveKind::Line: return "line";
    case SymbolPrimitiveKind::Circle: return "circle";
    case SymbolPrimitiveKind::Rectangle: return "rectangle";
    case SymbolPrimitiveKind::TerminalLead: return "terminal_lead";
    case SymbolPrimitiveKind::Unknown: return "unknown";
    }
    return "unknown";
}

const char* terminal_candidate_kind_name(TerminalCandidateKind k) {
    switch (k) {
    case TerminalCandidateKind::ComponentBoundary: return "component_boundary";
    case TerminalCandidateKind::ConnectorBoundary: return "connector_boundary";
    case TerminalCandidateKind::GroundConnection: return "ground_connection";
    case TerminalCandidateKind::Unknown: return "unknown";
    }
    return "unknown";
}

const char* endpoint_kind_name(EndpointKind k) {
    switch (k) {
    case EndpointKind::GeometricConductorEnd: return "geometric";
    case EndpointKind::ComponentTerminal: return "component_terminal";
    case EndpointKind::ConnectorTerminal: return "connector_terminal";
    case EndpointKind::Splice: return "splice";
    case EndpointKind::Ground: return "ground";
    case EndpointKind::ExternalConnection: return "external_connection";
    case EndpointKind::Unresolved: return "unresolved";
    }
    return "unresolved";
}

const char* rejected_geometry_class_name(RejectedGeometryClass c) {
    switch (c) {
    case RejectedGeometryClass::ComponentAssociated: return "component_associated";
    case RejectedGeometryClass::ConnectorAssociated: return "connector_associated";
    case RejectedGeometryClass::TextAssociated: return "text_associated";
    case RejectedGeometryClass::Unresolved: return "unresolved";
    }
    return "unresolved";
}

const char* symbol_family_name(SymbolFamily f) {
    switch (f) {
    case SymbolFamily::Ground: return "ground";
    case SymbolFamily::Lamp: return "lamp";
    case SymbolFamily::Switch: return "switch";
    case SymbolFamily::Relay: return "relay";
    case SymbolFamily::Motor: return "motor";
    case SymbolFamily::Diode: return "diode";
    case SymbolFamily::Alternator: return "alternator";
    case SymbolFamily::Battery: return "battery";
    case SymbolFamily::Solenoid: return "solenoid";
    case SymbolFamily::Coil: return "coil";
    case SymbolFamily::Fuse: return "fuse";
    case SymbolFamily::Unknown: return "unknown";
    }
    return "unknown";
}

const char* symbol_family_status_name(SymbolFamilyResolutionStatus s) {
    switch (s) {
    case SymbolFamilyResolutionStatus::Resolved: return "resolved";
    case SymbolFamilyResolutionStatus::Unresolved: return "unresolved";
    case SymbolFamilyResolutionStatus::Conflicted: return "conflicted";
    }
    return "unresolved";
}

const char* component_symbol_kind_name(ComponentSymbolKind k) {
    switch (k) {
    case ComponentSymbolKind::Enclosure: return "enclosure";
    case ComponentSymbolKind::CircularSymbol: return "circular_symbol";
    case ComponentSymbolKind::ChassisGround: return "chassis_ground";
    case ComponentSymbolKind::PrimitiveSymbol: return "primitive_symbol";
    case ComponentSymbolKind::DiagramFurniture: return "diagram_furniture";
    case ComponentSymbolKind::Unknown: return "unknown";
    }
    return "unknown";
}

const char* component_symbol_recognition_status_name(ComponentSymbolRecognitionStatus s) {
    switch (s) {
    case ComponentSymbolRecognitionStatus::Recognized: return "recognized";
    case ComponentSymbolRecognitionStatus::GeometricallyClassified: return "geometrically_classified";
    case ComponentSymbolRecognitionStatus::Unresolved: return "unresolved";
    case ComponentSymbolRecognitionStatus::Conflicted: return "conflicted";
    }
    return "unresolved";
}

const char* electrical_component_status_name(ElectricalComponentResolutionStatus s) {
    switch (s) {
    case ElectricalComponentResolutionStatus::Resolved: return "resolved";
    case ElectricalComponentResolutionStatus::Unresolved: return "unresolved";
    case ElectricalComponentResolutionStatus::Rejected: return "rejected";
    }
    return "unresolved";
}

const char* electrical_component_rejection_reason_name(ElectricalComponentRejectionReason r) {
    switch (r) {
    case ElectricalComponentRejectionReason::DiagramFurniture: return "diagram_furniture";
    case ElectricalComponentRejectionReason::ChassisGroundReference: return "chassis_ground_reference";
    case ElectricalComponentRejectionReason::ConnectorInterface: return "connector_interface";
    case ElectricalComponentRejectionReason::NotApplicable: return "not_applicable";
    }
    return "not_applicable";
}

void write_string_array(std::ostream& out, const std::vector<std::string>& values, const std::string& indent) {
    out << "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out << ", ";
        out << "\"" << json_escape(values[i]) << "\"";
    }
    out << "]";
    (void)indent;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: dx-audit-component-semantic <image> <output.json> [--scope <scope.json>]\n";
        return 1;
    }

    const std::string image_path = argv[1];
    const std::string output_path = argv[2];
    std::string scope_path;
    for (int i = 3; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--scope" && i + 1 < argc) {
            scope_path = argv[++i];
        }
    }

    std::string effective_image_path = image_path;
    std::string scope_identity = "unscoped";
    if (!scope_path.empty()) {
        const ExtractionScope scope = ExtractionScopeIO::load(scope_path);
        const cv::Mat original = ImageLoader::load(image_path);

        SourceScoper scoper;
        const ScopedSourceArtifacts scoped = scoper.apply(original, scope, image_path);

        const fs::path scoped_image_path =
            fs::path(output_path).parent_path() / "scoped_source.png";
        if (!cv::imwrite(scoped_image_path.string(), scoped.scoped_image)) {
            std::cerr << "unable to write scoped source image\n";
            return 1;
        }

        effective_image_path = scoped_image_path.string();
        scope_identity = scope_path;
    }

    ExtractionPipeline pipeline;
    const WireModel model = pipeline.run(effective_image_path, image_path);

    // ---- index existing evidence by component/endpoint id --------------
    std::map<std::string, std::vector<const SymbolPrimitive*>> primitives_by_component;
    for (const auto& p : model.symbol_primitives) primitives_by_component[p.component_id].push_back(&p);

    std::map<std::string, const ComponentSymbolRecognition*> recognition_by_component;
    for (const auto& r : model.component_symbol_recognitions) recognition_by_component[r.component_id] = &r;

    std::map<std::string, const SymbolFamilyResolution*> family_by_component;
    for (const auto& r : model.symbol_family_resolutions) family_by_component[r.component_id] = &r;

    std::map<std::string, std::vector<const TerminalCandidate*>> terminals_by_component;
    for (const auto& t : model.terminal_candidates) terminals_by_component[t.component_candidate_id].push_back(&t);

    std::map<std::string, std::vector<const EndpointCandidate*>> endpoints_by_component;
    for (const auto& e : model.endpoint_candidates)
        if (!e.component_id.empty()) endpoints_by_component[e.component_id].push_back(&e);

    std::map<std::string, std::vector<const RejectedGeometryEvidence*>> rejected_by_object;
    for (const auto& r : model.rejected_geometry)
        if (!r.associated_object_id.empty()) rejected_by_object[r.associated_object_id].push_back(&r);

    std::map<std::string, const ElectricalComponent*> electrical_by_component;
    for (const auto& e : model.electrical_components) electrical_by_component[e.component_candidate_id] = &e;

    std::map<std::string, std::vector<const SemanticAssociation*>> associations_by_component;
    for (const auto& a : model.semantic_associations)
        if (a.target_kind == SemanticAssociationTargetKind::Component)
            associations_by_component[a.target_id].push_back(&a);

    std::map<std::string, const TextRecognitionEvidence*> text_recognition_by_region;
    for (const auto& t : model.text_recognition_evidence) text_recognition_by_region[t.text_region_id] = &t;

    std::map<std::string, const TopologyEdge*> edges_by_id;
    for (const auto& e : model.edges) edges_by_id[e.id] = &e;

    std::map<std::string, std::vector<const TopologyEdge*>> edges_by_node;
    for (const auto& e : model.edges) {
        edges_by_node[e.from_node].push_back(&e);
        edges_by_node[e.to_node].push_back(&e);
    }

    // ---- write JSON ------------------------------------------------------
    std::ofstream out(output_path);
    out << "{\n";
    out << "  \"schema\": \"trx300-component-semantic-audit-v1\",\n";
    out << "  \"ap_id\": \"AP-DIAG-AUDIT-008\",\n";
    out << "  \"source_id\": \"" << json_escape(model.source_id) << "\",\n";
    out << "  \"scope\": \"" << json_escape(scope_identity) << "\",\n";
    out << "  \"component_candidate_count\": " << model.component_candidates.size() << ",\n";
    out << "  \"electrical_component_count\": " << model.electrical_components.size() << ",\n";
    out << "  \"expected_trx300_semantic_target\": 24,\n";
    out << "  \"candidates\": [\n";

    for (std::size_t ci = 0; ci < model.component_candidates.size(); ++ci) {
        const auto& c = model.component_candidates[ci];

        out << "    {\n";
        out << "      \"id\": \"" << json_escape(c.id) << "\",\n";
        out << "      \"kind\": \"" << component_candidate_kind_name(c.kind) << "\",\n";
        out << "      \"bounds\": {\"x\": " << c.bounds.x << ", \"y\": " << c.bounds.y
            << ", \"width\": " << c.bounds.width << ", \"height\": " << c.bounds.height << "},\n";
        out << "      \"center\": {\"x\": " << (c.bounds.x + c.bounds.width / 2.0)
            << ", \"y\": " << (c.bounds.y + c.bounds.height / 2.0) << "},\n";
        out << "      \"confidence\": \"" << confidence_name(c.confidence) << "\",\n";
        out << "      \"semantic_labels\": ";
        write_string_array(out, c.semantic_labels, "      ");
        out << ",\n";
        out << "      \"provenance\": \"NOT_PRESENT_IN_CURRENT_MODEL\",\n";

        const auto elec_it = electrical_by_component.find(c.id);
        out << "      \"electrical_component_status\": \""
            << (elec_it != electrical_by_component.end()
                    ? electrical_component_status_name(elec_it->second->status)
                    : "not_evaluated")
            << "\",\n";
        out << "      \"electrical_component_rejection_reason\": \""
            << (elec_it != electrical_by_component.end()
                    ? electrical_component_rejection_reason_name(elec_it->second->rejection_reason)
                    : "not_applicable")
            << "\",\n";

        // component_symbol_recognition
        const auto recog_it = recognition_by_component.find(c.id);
        if (recog_it != recognition_by_component.end()) {
            out << "      \"component_symbol_recognition\": {\n"
                << "        \"present\": true,\n"
                << "        \"symbol_kind\": \"" << component_symbol_kind_name(recog_it->second->symbol_kind) << "\",\n"
                << "        \"status\": \"" << component_symbol_recognition_status_name(recog_it->second->status) << "\",\n"
                << "        \"confidence\": \"" << confidence_name(recog_it->second->confidence) << "\"\n"
                << "      },\n";
        } else {
            out << "      \"component_symbol_recognition\": {\"present\": false},\n";
        }

        // symbol_family
        const auto family_it = family_by_component.find(c.id);
        if (family_it != family_by_component.end()) {
            out << "      \"symbol_family\": {\n"
                << "        \"present\": true,\n"
                << "        \"family\": \"" << symbol_family_name(family_it->second->family) << "\",\n"
                << "        \"status\": \"" << symbol_family_status_name(family_it->second->status) << "\",\n"
                << "        \"confidence\": \"" << confidence_name(family_it->second->confidence) << "\",\n"
                << "        \"evidence_count\": " << family_it->second->evidence_ids.size() << "\n"
                << "      },\n";
        } else {
            out << "      \"symbol_family\": {\"present\": false},\n";
        }

        // symbol_primitives
        const auto prim_it = primitives_by_component.find(c.id);
        out << "      \"symbol_primitives\": [";
        if (prim_it != primitives_by_component.end()) {
            for (std::size_t i = 0; i < prim_it->second.size(); ++i) {
                const auto* p = prim_it->second[i];
                if (i) out << ", ";
                out << "{\"id\": \"" << json_escape(p->id) << "\", \"kind\": \""
                    << symbol_primitive_kind_name(p->kind) << "\", \"confidence\": \""
                    << confidence_name(p->confidence) << "\", \"area\": " << p->area << "}";
            }
        }
        out << "],\n";

        // terminal_candidates
        const auto term_it = terminals_by_component.find(c.id);
        out << "      \"terminal_candidates\": [";
        if (term_it != terminals_by_component.end()) {
            for (std::size_t i = 0; i < term_it->second.size(); ++i) {
                const auto* t = term_it->second[i];
                if (i) out << ", ";
                out << "{\"id\": \"" << json_escape(t->id) << "\", \"kind\": \""
                    << terminal_candidate_kind_name(t->kind) << "\", \"confidence\": \""
                    << confidence_name(t->confidence) << "\", \"distance_to_component\": "
                    << t->distance_to_component << ", \"endpoint_id\": \""
                    << json_escape(t->endpoint_id) << "\"}";
            }
        }
        out << "],\n";

        // endpoint_candidates (owned by this component, EndpointCandidate.component_id)
        const auto ep_it = endpoints_by_component.find(c.id);
        std::vector<std::string> node_ids;
        std::vector<std::string> endpoint_ids;
        out << "      \"endpoint_candidates\": [";
        if (ep_it != endpoints_by_component.end()) {
            for (std::size_t i = 0; i < ep_it->second.size(); ++i) {
                const auto* e = ep_it->second[i];
                endpoint_ids.push_back(e->id);
                node_ids.push_back(e->node_id);
                if (i) out << ", ";
                out << "{\"id\": \"" << json_escape(e->id) << "\", \"kind\": \""
                    << endpoint_kind_name(e->kind) << "\", \"confidence\": \""
                    << confidence_name(e->confidence) << "\", \"node_id\": \""
                    << json_escape(e->node_id) << "\"}";
            }
        }
        out << "],\n";

        // topology nodes/edges reachable from those endpoints
        std::set<std::string> edge_ids;
        for (const auto& node_id : node_ids) {
            const auto it = edges_by_node.find(node_id);
            if (it != edges_by_node.end()) {
                for (const auto* e : it->second) edge_ids.insert(e->id);
            }
        }
        out << "      \"topology_nodes\": ";
        write_string_array(out, node_ids, "      ");
        out << ",\n";
        out << "      \"topology_edges\": ";
        write_string_array(out, std::vector<std::string>(edge_ids.begin(), edge_ids.end()), "      ");
        out << ",\n";

        std::set<std::string> conductor_segment_ids;
        for (const auto& edge_id : edge_ids) {
            const auto* edge = edges_by_id.at(edge_id);
            if (!edge->conductor_segment.empty()) conductor_segment_ids.insert(edge->conductor_segment);
        }
        out << "      \"conductor_segments_referenced\": ";
        write_string_array(out, std::vector<std::string>(conductor_segment_ids.begin(), conductor_segment_ids.end()), "      ");
        out << ",\n";

        // wires whose start/end endpoint is one of this component's endpoints
        std::set<std::string> endpoint_id_set(endpoint_ids.begin(), endpoint_ids.end());
        std::vector<std::string> wire_ids;
        std::vector<std::string> net_ids;
        for (const auto& w : model.wires) {
            if (endpoint_id_set.count(w.start_endpoint) || endpoint_id_set.count(w.end_endpoint)) {
                wire_ids.push_back(w.id);
            }
        }
        for (const auto& n : model.electrical_nets) {
            for (const auto& eid : n.endpoint_ids) {
                if (endpoint_id_set.count(eid)) { net_ids.push_back(n.id); break; }
            }
        }
        out << "      \"physical_wires\": ";
        write_string_array(out, wire_ids, "      ");
        out << ",\n";
        out << "      \"electrical_nets\": ";
        write_string_array(out, net_ids, "      ");
        out << ",\n";

        // rejected geometry directly associated with this component
        const auto rej_it = rejected_by_object.find(c.id);
        out << "      \"rejected_geometry_associated\": [";
        if (rej_it != rejected_by_object.end()) {
            for (std::size_t i = 0; i < rej_it->second.size(); ++i) {
                const auto* r = rej_it->second[i];
                if (i) out << ", ";
                out << "{\"id\": \"" << json_escape(r->id) << "\", \"reason\": \""
                    << json_escape(r->reason) << "\", \"classification\": \""
                    << rejected_geometry_class_name(r->classification) << "\", \"measurement\": "
                    << r->measurement << "}";
            }
        }
        out << "],\n";

        // semantic associations (geometric text-region proximity - NOT the
        // recognized text content, which is a separate, independently
        // absent-or-present evidence source: text_recognition_evidence)
        const auto assoc_it = associations_by_component.find(c.id);
        out << "      \"semantic_associations\": [";
        if (assoc_it != associations_by_component.end()) {
            for (std::size_t i = 0; i < assoc_it->second.size(); ++i) {
                const auto* a = assoc_it->second[i];
                if (i) out << ", ";
                const auto text_it = text_recognition_by_region.find(a->text_region_id);
                out << "{\"text_region_id\": \"" << json_escape(a->text_region_id)
                    << "\", \"distance\": " << a->distance << ", \"confidence\": \""
                    << confidence_name(a->confidence) << "\", \"recognized_text_available\": "
                    << (text_it != text_recognition_by_region.end() ? "true" : "false");
                if (text_it != text_recognition_by_region.end()) {
                    out << ", \"raw_text\": \"" << json_escape(text_it->second->raw_text) << "\"";
                }
                out << "}";
            }
        }
        out << "]\n";

        out << "    }";
        if (ci + 1 != model.component_candidates.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n";
    out << "  \"global_context\": {\n";
    out << "    \"text_recognition_evidence_count\": " << model.text_recognition_evidence.size() << ",\n";
    out << "    \"semantic_associations_count\": " << model.semantic_associations.size() << ",\n";
    out << "    \"terminal_candidates_count\": " << model.terminal_candidates.size() << ",\n";
    out << "    \"connector_candidates_count\": " << model.connector_candidates.size() << ",\n";
    out << "    \"physical_wires_count\": " << model.wires.size() << ",\n";
    out << "    \"electrical_nets_count\": " << model.electrical_nets.size() << "\n";
    out << "  }\n";
    out << "}\n";

    std::cout << "component semantic audit written to " << output_path << "\n";
    return 0;
}

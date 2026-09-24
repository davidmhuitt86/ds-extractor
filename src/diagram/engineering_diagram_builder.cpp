#include "eke_dx_wire/diagram/engineering_diagram_builder.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace eke::dx::wire {
namespace {

void issue(
    DiagramValidationReport& report,
    std::string category,
    std::string code,
    std::string object_id,
    std::string detail) {
    report.issues.push_back({
        std::move(category), std::move(code),
        std::move(object_id), std::move(detail)});
}

DiagramObjectStatus from_canonicalization_status(
    ComponentIdentityCanonicalizationStatus status) {
    switch (status) {
    case ComponentIdentityCanonicalizationStatus::Resolved:
        return DiagramObjectStatus::Resolved;
    case ComponentIdentityCanonicalizationStatus::Conflicted:
        return DiagramObjectStatus::Conflicted;
    case ComponentIdentityCanonicalizationStatus::NotFound:
        return DiagramObjectStatus::Unresolved;
    }
    return DiagramObjectStatus::Unresolved;
}

} // namespace

EngineeringDiagram EngineeringDiagramBuilder::build(const WireModel& model) const {
    EngineeringDiagram diagram;
    diagram.source_id = model.source_id;
    diagram.page = model.page;
    diagram.image_width = model.image_width;
    diagram.image_height = model.image_height;

    // ---- lookup indices (read-only) --------------------------------
    std::unordered_set<std::string> endpoint_ids_set;
    for (const auto& endpoint : model.endpoint_candidates) {
        endpoint_ids_set.insert(endpoint.id);
    }
    std::unordered_set<std::string> component_ids_set;
    for (const auto& component : model.component_candidates) {
        component_ids_set.insert(component.id);
    }
    std::unordered_set<std::string> connector_ids_set;
    for (const auto& connector : model.connector_candidates) {
        connector_ids_set.insert(connector.id);
    }
    std::unordered_set<std::string> edge_ids_set;
    for (const auto& edge : model.edges) {
        edge_ids_set.insert(edge.id);
    }
    std::unordered_set<std::string> wire_ids_set;
    for (const auto& wire : model.wires) {
        wire_ids_set.insert(wire.id);
    }

    std::map<std::string, std::vector<std::string>> terminal_ids_by_component;
    for (const auto& terminal : model.terminal_candidates) {
        if (!terminal.component_candidate_id.empty()) {
            terminal_ids_by_component[terminal.component_candidate_id].push_back(terminal.id);
        }
    }
    std::map<std::string, std::vector<std::string>> endpoint_ids_by_component;
    for (const auto& endpoint : model.endpoint_candidates) {
        if (!endpoint.component_id.empty()) {
            endpoint_ids_by_component[endpoint.component_id].push_back(endpoint.id);
        }
    }
    std::map<std::string, std::string> symbol_geometry_by_component;
    for (const auto& geometry : model.component_symbol_geometries) {
        symbol_geometry_by_component[geometry.component_id] = geometry.id;
    }
    std::map<std::string, std::string> symbol_family_resolution_by_component;
    for (const auto& resolution : model.symbol_family_resolutions) {
        symbol_family_resolution_by_component[resolution.component_id] = resolution.id;
    }
    std::map<std::string, const ComponentIdentityCanonicalization*> canonicalization_by_component;
    for (const auto& canonicalization : model.component_identity_canonicalizations) {
        canonicalization_by_component[canonicalization.component_id] = &canonicalization;
    }
    std::unordered_map<std::string, const ComponentIdentityResolution*> resolution_by_id;
    for (const auto& resolution : model.component_identity_resolutions) {
        resolution_by_id[resolution.id] = &resolution;
    }
    std::map<std::string, std::vector<std::string>> connector_terminals_by_connector;
    for (const auto& terminal : model.connector_terminals) {
        connector_terminals_by_connector[terminal.connector_id].push_back(terminal.id);
    }
    std::unordered_map<std::string, const WireSemanticResolution*> semantics_by_wire;
    for (const auto& resolution : model.wire_semantics) {
        semantics_by_wire[resolution.wire_id] = &resolution;
    }

    // ---- components --------------------------------------------------
    diagram.components.reserve(model.component_candidates.size());
    for (const auto& component : model.component_candidates) {
        DiagramComponent dc;
        dc.component_id = component.id;
        dc.kind = component.kind;
        dc.bounds = component.bounds;
        dc.geometry_confidence = component.confidence;
        dc.semantic_labels = component.semantic_labels;

        if (const auto it = terminal_ids_by_component.find(component.id);
            it != terminal_ids_by_component.end()) {
            dc.terminal_candidate_ids = it->second;
            std::sort(dc.terminal_candidate_ids.begin(), dc.terminal_candidate_ids.end());
        }
        if (const auto it = endpoint_ids_by_component.find(component.id);
            it != endpoint_ids_by_component.end()) {
            dc.endpoint_ids = it->second;
            std::sort(dc.endpoint_ids.begin(), dc.endpoint_ids.end());
        }
        if (const auto it = symbol_geometry_by_component.find(component.id);
            it != symbol_geometry_by_component.end()) {
            dc.symbol_geometry_id = it->second;
        }
        if (const auto it = symbol_family_resolution_by_component.find(component.id);
            it != symbol_family_resolution_by_component.end()) {
            dc.symbol_family_resolution_id = it->second;
        }

        if (const auto it = canonicalization_by_component.find(component.id);
            it != canonicalization_by_component.end()) {
            const auto* canonicalization = it->second;
            dc.identity_status = from_canonicalization_status(canonicalization->status);
            if (canonicalization->status == ComponentIdentityCanonicalizationStatus::Resolved) {
                dc.canonical_name = canonicalization->canonical_name;
            }
            dc.identity_evidence_ids.push_back(canonicalization->id);
            if (const auto res_it = resolution_by_id.find(canonicalization->source_resolution_id);
                res_it != resolution_by_id.end()) {
                dc.identity_evidence_ids.push_back(res_it->second->id);
                for (const auto& evidence_id : res_it->second->evidence_ids) {
                    dc.identity_evidence_ids.push_back(evidence_id);
                }
            }
            std::sort(dc.identity_evidence_ids.begin(), dc.identity_evidence_ids.end());
            dc.identity_evidence_ids.erase(
                std::unique(dc.identity_evidence_ids.begin(), dc.identity_evidence_ids.end()),
                dc.identity_evidence_ids.end());
        }

        diagram.components.push_back(std::move(dc));
    }
    std::sort(
        diagram.components.begin(), diagram.components.end(),
        [](const DiagramComponent& a, const DiagramComponent& b) {
            return a.component_id < b.component_id;
        });

    // ---- connectors ----------------------------------------------------
    diagram.connectors.reserve(model.connector_candidates.size());
    for (const auto& connector : model.connector_candidates) {
        DiagramConnector conn;
        conn.connector_id = connector.id;
        conn.component_id = connector.component_candidate_id;
        conn.bounds = connector.bounds;
        conn.confidence = connector.confidence;
        conn.semantic_labels = connector.semantic_labels;
        if (const auto it = connector_terminals_by_connector.find(connector.id);
            it != connector_terminals_by_connector.end()) {
            conn.connector_terminal_ids = it->second;
            std::sort(conn.connector_terminal_ids.begin(), conn.connector_terminal_ids.end());
        }
        diagram.connectors.push_back(std::move(conn));
    }
    std::sort(
        diagram.connectors.begin(), diagram.connectors.end(),
        [](const DiagramConnector& a, const DiagramConnector& b) {
            return a.connector_id < b.connector_id;
        });

    // ---- wires -----------------------------------------------------
    diagram.wires.reserve(model.wires.size());
    for (const auto& wire : model.wires) {
        DiagramWire dw;
        dw.wire_id = wire.id;
        dw.start_endpoint_id = wire.start_endpoint;
        dw.end_endpoint_id = wire.end_endpoint;
        dw.topology_edge_ids = wire.topology_edges;
        dw.conductor_segment_ids = wire.conductor_segments;
        dw.geometry_confidence = wire.confidence;
        dw.heavy_cable = wire.heavy_cable;
        if (const auto it = semantics_by_wire.find(wire.id); it != semantics_by_wire.end()) {
            dw.wire_semantic_resolution_id = it->second->id;
        }
        diagram.wires.push_back(std::move(dw));
    }
    std::sort(
        diagram.wires.begin(), diagram.wires.end(),
        [](const DiagramWire& a, const DiagramWire& b) { return a.wire_id < b.wire_id; });

    // ---- splices / crossings ----------------------------------------
    std::map<std::string, std::vector<std::string>> wires_by_edge;
    for (const auto& wire : model.wires) {
        for (const auto& edge_id : wire.topology_edges) {
            wires_by_edge[edge_id].push_back(wire.id);
        }
    }
    std::map<std::string, std::vector<std::string>> edges_by_node;
    for (const auto& edge : model.edges) {
        edges_by_node[edge.from_node].push_back(edge.id);
        edges_by_node[edge.to_node].push_back(edge.id);
    }

    for (const auto& node : model.nodes) {
        if (node.type == TopologyNodeType::Splice || node.type == TopologyNodeType::Junction) {
            DiagramSplice splice;
            splice.node_id = node.id;
            splice.position = node.position;
            splice.type = node.type;

            std::set<std::string> incident_wires;
            if (const auto it = edges_by_node.find(node.id); it != edges_by_node.end()) {
                for (const auto& edge_id : it->second) {
                    if (const auto wire_it = wires_by_edge.find(edge_id);
                        wire_it != wires_by_edge.end()) {
                        for (const auto& wire_id : wire_it->second) {
                            incident_wires.insert(wire_id);
                        }
                    }
                }
            }
            splice.incident_wire_ids.assign(incident_wires.begin(), incident_wires.end());
            diagram.splices.push_back(std::move(splice));
        } else if (node.type == TopologyNodeType::Crossing) {
            diagram.crossing_node_ids.push_back(node.id);
        }
    }
    std::sort(
        diagram.splices.begin(), diagram.splices.end(),
        [](const DiagramSplice& a, const DiagramSplice& b) { return a.node_id < b.node_id; });
    std::sort(diagram.crossing_node_ids.begin(), diagram.crossing_node_ids.end());

    // ---- labels -------------------------------------------------------
    std::unordered_map<std::string, const TextRecognitionEvidence*> recognition_by_region;
    for (const auto& evidence : model.text_recognition_evidence) {
        if (!recognition_by_region.count(evidence.text_region_id)) {
            recognition_by_region.emplace(evidence.text_region_id, &evidence);
        }
    }
    std::unordered_map<std::string, const TextSemanticEvidence*> semantic_by_region;
    for (const auto& evidence : model.text_semantic_evidence) {
        if (!semantic_by_region.count(evidence.text_region_id)) {
            semantic_by_region.emplace(evidence.text_region_id, &evidence);
        }
    }
    std::unordered_map<std::string, const EngineeringObjectSemanticResolution*> resolution_by_region;
    for (const auto& resolution : model.engineering_object_semantics) {
        if (!resolution_by_region.count(resolution.text_region_id)) {
            resolution_by_region.emplace(resolution.text_region_id, &resolution);
        }
    }

    diagram.labels.reserve(model.text_regions.size());
    for (const auto& region : model.text_regions) {
        DiagramLabel label;
        label.text_region_id = region.id;

        if (const auto it = recognition_by_region.find(region.id);
            it != recognition_by_region.end()) {
            label.raw_text = it->second->raw_text;
            label.provider = it->second->provider;
        }
        if (const auto it = semantic_by_region.find(region.id); it != semantic_by_region.end()) {
            label.normalized_text = it->second->normalized_text;
            label.kind = it->second->kind;
            label.confidence = it->second->confidence;
        }
        if (const auto it = resolution_by_region.find(region.id);
            it != resolution_by_region.end() &&
            it->second->confidence != ConfidenceClass::Unresolved) {
            label.status = DiagramObjectStatus::Resolved;
            label.associated_object_id = it->second->target_id;
            label.associated_object_kind = it->second->target_kind;
        }

        diagram.labels.push_back(std::move(label));
    }
    std::sort(
        diagram.labels.begin(), diagram.labels.end(),
        [](const DiagramLabel& a, const DiagramLabel& b) {
            return a.text_region_id < b.text_region_id;
        });

    // ---- electrical nets ------------------------------------------
    diagram.electrical_nets.reserve(model.electrical_nets.size());
    for (const auto& net : model.electrical_nets) {
        DiagramElectricalNet den;
        den.net_id = net.id;
        den.endpoint_ids = net.endpoint_ids;
        den.splice_node_ids = net.splice_node_ids;
        den.topology_edge_ids = net.topology_edges;
        den.role = net.role;
        den.confidence = net.confidence;

        std::unordered_set<std::string> net_endpoints(
            net.endpoint_ids.begin(), net.endpoint_ids.end());
        std::set<std::string> wires_in_net;
        for (const auto& wire : model.wires) {
            if (net_endpoints.count(wire.start_endpoint) ||
                net_endpoints.count(wire.end_endpoint)) {
                wires_in_net.insert(wire.id);
            }
        }
        den.wire_ids.assign(wires_in_net.begin(), wires_in_net.end());

        diagram.electrical_nets.push_back(std::move(den));
    }
    std::sort(
        diagram.electrical_nets.begin(), diagram.electrical_nets.end(),
        [](const DiagramElectricalNet& a, const DiagramElectricalNet& b) {
            return a.net_id < b.net_id;
        });

    // ---- validation: reference integrity -----------------------------
    DiagramValidationReport& v = diagram.validation;

    for (const auto& wire : model.wires) {
        if (endpoint_ids_set.count(wire.start_endpoint)) ++v.valid_references;
        else {
            ++v.invalid_references;
            issue(v, "wire", "WIRE-START-ENDPOINT-MISSING", wire.id,
                  "start_endpoint does not reference an existing EndpointCandidate");
        }
        if (endpoint_ids_set.count(wire.end_endpoint)) ++v.valid_references;
        else {
            ++v.invalid_references;
            issue(v, "wire", "WIRE-END-ENDPOINT-MISSING", wire.id,
                  "end_endpoint does not reference an existing EndpointCandidate");
        }
        for (const auto& edge_id : wire.topology_edges) {
            if (edge_ids_set.count(edge_id)) ++v.valid_references;
            else {
                ++v.invalid_references;
                issue(v, "wire", "WIRE-TOPOLOGY-EDGE-MISSING", wire.id,
                      "topology_edges references a missing TopologyEdge: " + edge_id);
            }
        }
    }

    for (const auto& terminal : model.terminal_candidates) {
        if (endpoint_ids_set.count(terminal.endpoint_id)) ++v.valid_references;
        else {
            ++v.invalid_references;
            issue(v, "terminal", "TERMINAL-ENDPOINT-MISSING", terminal.id,
                  "endpoint_id does not reference an existing EndpointCandidate");
        }
        if (!terminal.component_candidate_id.empty()) {
            if (component_ids_set.count(terminal.component_candidate_id)) ++v.valid_references;
            else {
                ++v.invalid_references;
                issue(v, "terminal", "TERMINAL-COMPONENT-MISSING", terminal.id,
                      "component_candidate_id does not reference an existing ComponentCandidate");
            }
        }
    }

    for (const auto& connector_terminal : model.connector_terminals) {
        if (connector_ids_set.count(connector_terminal.connector_id)) ++v.valid_references;
        else {
            ++v.invalid_references;
            issue(v, "connector_terminal", "CONNECTOR-TERMINAL-CONNECTOR-MISSING",
                  connector_terminal.id,
                  "connector_id does not reference an existing ConnectorCandidate");
        }
    }

    for (const auto& geometry : model.component_symbol_geometries) {
        if (component_ids_set.count(geometry.component_id)) ++v.valid_references;
        else {
            ++v.invalid_references;
            issue(v, "symbol_geometry", "SYMBOL-GEOMETRY-COMPONENT-MISSING", geometry.id,
                  "component_id does not reference an existing ComponentCandidate");
        }
    }

    for (const auto& primitive : model.symbol_primitives) {
        if (component_ids_set.count(primitive.component_id)) ++v.valid_references;
        else {
            ++v.invalid_references;
            issue(v, "symbol_primitive", "PRIMITIVE-COMPONENT-MISSING", primitive.id,
                  "component_id does not reference an existing ComponentCandidate");
        }
    }

    for (const auto& net : model.electrical_nets) {
        for (const auto& endpoint_id : net.endpoint_ids) {
            if (endpoint_ids_set.count(endpoint_id)) ++v.valid_references;
            else {
                ++v.invalid_references;
                issue(v, "electrical_net", "NET-ENDPOINT-MISSING", net.id,
                      "endpoint_ids references a missing EndpointCandidate: " + endpoint_id);
            }
        }
    }

    for (const auto& resolution : model.wire_semantics) {
        if (wire_ids_set.count(resolution.wire_id)) ++v.valid_references;
        else {
            ++v.invalid_references;
            issue(v, "wire_semantic_resolution", "SEMANTIC-RESOLUTION-WIRE-MISSING",
                  resolution.id, "wire_id does not reference an existing Wire");
        }
    }

    for (const auto& resolution : model.symbol_family_resolutions) {
        if (component_ids_set.count(resolution.component_id)) ++v.valid_references;
        else {
            ++v.invalid_references;
            issue(v, "symbol_family_resolution", "SYMBOL-FAMILY-RESOLUTION-COMPONENT-MISSING",
                  resolution.id, "component_id does not reference an existing ComponentCandidate");
        }
    }
    for (const auto& evidence : model.symbol_family_evidence) {
        if (component_ids_set.count(evidence.component_id)) ++v.valid_references;
        else {
            ++v.invalid_references;
            issue(v, "symbol_family_evidence", "SYMBOL-FAMILY-EVIDENCE-COMPONENT-MISSING",
                  evidence.id, "component_id does not reference an existing ComponentCandidate");
        }
    }

    // ---- validation: duplicate relationships --------------------------
    {
        std::set<std::string> seen_terminal_pairs;
        for (const auto& terminal : model.terminal_candidates) {
            if (terminal.component_candidate_id.empty()) continue;
            const std::string pair = terminal.endpoint_id + ":" + terminal.component_candidate_id;
            if (!seen_terminal_pairs.insert(pair).second) {
                ++v.duplicate_relationships;
                issue(v, "terminal", "DUPLICATE-TERMINAL-COMPONENT-PAIR", terminal.id,
                      "endpoint/component pair already represented by another TerminalCandidate");
            }
        }

        std::set<std::string> seen_connector_pairs;
        for (const auto& terminal : model.connector_terminals) {
            const std::string pair = terminal.connector_id + ":" + terminal.endpoint_id;
            if (!seen_connector_pairs.insert(pair).second) {
                ++v.duplicate_relationships;
                issue(v, "connector_terminal", "DUPLICATE-CONNECTOR-TERMINAL-PAIR", terminal.id,
                      "connector/endpoint pair already represented by another ConnectorTerminal");
            }
        }

        std::set<std::string> seen_wire_semantic_ids;
        for (const auto& resolution : model.wire_semantics) {
            if (!seen_wire_semantic_ids.insert(resolution.wire_id).second) {
                ++v.duplicate_relationships;
                issue(v, "wire_semantic_resolution", "DUPLICATE-WIRE-SEMANTIC-RESOLUTION",
                      resolution.id, "more than one WireSemanticResolution for the same wire_id");
            }
        }

        std::set<std::string> seen_symbol_family_components;
        for (const auto& resolution : model.symbol_family_resolutions) {
            if (!seen_symbol_family_components.insert(resolution.component_id).second) {
                ++v.duplicate_relationships;
                issue(v, "symbol_family_resolution", "DUPLICATE-SYMBOL-FAMILY-RESOLUTION",
                      resolution.id, "more than one SymbolFamilyResolution for the same component_id");
            }
        }
    }

    // ---- validation: orphan objects -----------------------------------
    for (const auto& component : diagram.components) {
        if (component.kind == ComponentCandidateKind::DiagramFurniture) continue;
        if (component.terminal_candidate_ids.empty() &&
            component.endpoint_ids.empty() &&
            component.symbol_geometry_id.empty()) {
            ++v.orphan_objects;
            issue(v, "component", "COMPONENT-FULLY-DISCONNECTED", component.component_id,
                  "real component has no terminals, endpoints, or symbol geometry");
        }
    }

    return diagram;
}

} // namespace eke::dx::wire

#include "eke_dx_wire/export/topology_exporter.hpp"

#include <fstream>
#include <stdexcept>

namespace eke::dx::wire {
namespace {

const char* node_type_name(TopologyNodeType type) {
    switch (type) {
    case TopologyNodeType::ConductorEnd: return "conductor_end";
    case TopologyNodeType::Continuation: return "continuation";
    case TopologyNodeType::Junction: return "junction";
    case TopologyNodeType::Splice: return "splice";
    case TopologyNodeType::Crossing: return "crossing";
    case TopologyNodeType::ComponentBoundary: return "component_boundary";
    case TopologyNodeType::Unresolved: return "unresolved";
    }
    return "unresolved";
}

} // namespace

void TopologyExporter::export_json(
    const WireModel& model,
    const std::string& output_path) {

    std::ofstream out(output_path);
    if (!out) {
        throw std::runtime_error("Unable to create topology JSON: " + output_path);
    }

    auto json_escape = [](const std::string& value) {
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
    };

    auto confidence_name = [](ConfidenceClass confidence) {
        switch (confidence) {
        case ConfidenceClass::High: return "high";
        case ConfidenceClass::Medium: return "medium";
        case ConfidenceClass::Low: return "low";
        case ConfidenceClass::Unresolved: return "unresolved";
        }
        return "unresolved";
    };

    auto endpoint_kind_name = [](EndpointKind kind) {
        switch (kind) {
        case EndpointKind::GeometricConductorEnd: return "geometric";
        case EndpointKind::ComponentTerminal: return "component_terminal";
        case EndpointKind::ConnectorTerminal: return "connector_terminal";
        case EndpointKind::Splice: return "splice";
        case EndpointKind::Ground: return "ground";
        case EndpointKind::ExternalConnection: return "external_connection";
        case EndpointKind::Unresolved: return "unresolved";
        }
        return "unresolved";
    };

    auto terminal_role_name = [](TerminalRole role) {
        switch (role) {
        case TerminalRole::Unknown: return "unknown";
        case TerminalRole::ComponentTerminal: return "component_terminal";
        case TerminalRole::ConnectorTerminal: return "connector_terminal";
        case TerminalRole::GroundTerminal: return "ground_terminal";
        case TerminalRole::PowerSource: return "power_source";
        case TerminalRole::ExternalConnection: return "external_connection";
        }
        return "unknown";
    };

    auto rejected_geometry_class_name = [](RejectedGeometryClass classification) {
        switch (classification) {
        case RejectedGeometryClass::ComponentAssociated:
            return "component_associated";
        case RejectedGeometryClass::ConnectorAssociated:
            return "connector_associated";
        case RejectedGeometryClass::TextAssociated:
            return "text_associated";
        case RejectedGeometryClass::Unresolved:
            return "unresolved";
        }
        return "unresolved";
    };

    auto text_semantic_kind_name = [](TextSemanticKind kind) {
        switch (kind) {
        case TextSemanticKind::GroundLabel: return "ground_label";
        case TextSemanticKind::PowerFeedLabel: return "power_feed_label";
        case TextSemanticKind::SharedFunctionFeedLabel: return "shared_function_feed_label";
        case TextSemanticKind::ComponentLabel: return "component_label";
        case TextSemanticKind::ConnectorLabel: return "connector_label";
        case TextSemanticKind::TerminalLabel: return "terminal_label";
        case TextSemanticKind::WireColorLabel: return "wire_color_label";
        case TextSemanticKind::FunctionLabel: return "function_label";
        case TextSemanticKind::Unknown: return "unknown";
        }
        return "unknown";
    };

    auto association_target_kind_name = [](SemanticAssociationTargetKind kind) {
        switch (kind) {
        case SemanticAssociationTargetKind::Component:
            return "component";
        case SemanticAssociationTargetKind::Endpoint:
            return "endpoint";
        }
        return "endpoint";
    };

    auto association_relation_name = [](SemanticAssociationRelation relation) {
        switch (relation) {
        case SemanticAssociationRelation::LabelToComponent:
            return "label_to_component";
        case SemanticAssociationRelation::LabelToEndpoint:
            return "label_to_endpoint";
        }
        return "label_to_endpoint";
    };

    auto distribution_role_name = [](DistributionRole role) {
        switch (role) {
        case DistributionRole::Ground: return "ground";
        case DistributionRole::PowerFeed: return "power_feed";
        case DistributionRole::SharedFunctionFeed: return "shared_function_feed";
        case DistributionRole::Unknown: return "unknown";
        }
        return "unknown";
    };

    out << "{\n";
    out << "  \"source_id\": \"" << json_escape(model.source_id) << "\",\n";
    out << "  \"page\": " << model.page << ",\n";
    out << "  \"component_candidates\": [\n";
    for (std::size_t i = 0; i < model.component_candidates.size(); ++i) {
        const auto& component = model.component_candidates[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(component.id) << "\",\n"
            << "      \"semantic_labels\": [";
        for (std::size_t j = 0; j < component.semantic_labels.size(); ++j) {
            if (j) out << ", ";
            out << "\"" << json_escape(component.semantic_labels[j]) << "\"";
        }
        out << "],\n"
            << "      \"x\": " << component.bounds.x << ",\n"
            << "      \"y\": " << component.bounds.y << ",\n"
            << "      \"width\": " << component.bounds.width << ",\n"
            << "      \"height\": " << component.bounds.height << ",\n"
            << "      \"confidence\": \"" << confidence_name(component.confidence) << "\"\n"
            << "    }";
        if (i + 1 != model.component_candidates.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";
    out << "  \"nodes\": [\n";
    for (std::size_t i = 0; i < model.nodes.size(); ++i) {
        const auto& node = model.nodes[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(node.id) << "\",\n"
            << "      \"x\": " << node.position.x << ",\n"
            << "      \"y\": " << node.position.y << ",\n"
            << "      \"type\": \"" << node_type_name(node.type) << "\",\n"
            << "      \"electrically_connective\": "
            << (node.electrically_connective ? "true" : "false") << "\n"
            << "    }";
        if (i + 1 != model.nodes.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n  \"rejected_geometry\": [\n";
    for (std::size_t i = 0; i < model.rejected_geometry.size(); ++i) {
        const auto& evidence = model.rejected_geometry[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(evidence.id) << "\",\n"
            << "      \"x1\": " << evidence.geometry.a.x << ",\n"
            << "      \"y1\": " << evidence.geometry.a.y << ",\n"
            << "      \"x2\": " << evidence.geometry.b.x << ",\n"
            << "      \"y2\": " << evidence.geometry.b.y << ",\n"
            << "      \"reason\": \"" << json_escape(evidence.reason) << "\",\n"
            << "      \"measurement\": " << evidence.measurement << ",\n"
            << "      \"classification\": \"" << rejected_geometry_class_name(evidence.classification) << "\",\n"
            << "      \"associated_object_id\": \"" << json_escape(evidence.associated_object_id) << "\"\n"
            << "    }";
        if (i + 1 != model.rejected_geometry.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n  \"edges\": [\n";
    for (std::size_t i = 0; i < model.edges.size(); ++i) {
        const auto& edge = model.edges[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(edge.id) << "\",\n"
            << "      \"from_node\": \"" << json_escape(edge.from_node) << "\",\n"
            << "      \"to_node\": \"" << json_escape(edge.to_node) << "\",\n"
            << "      \"conductor_segment\": \"" << json_escape(edge.conductor_segment) << "\"\n"
            << "    }";
        if (i + 1 != model.edges.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n  \"endpoint_candidates\": [\n";
    for (std::size_t i = 0; i < model.endpoint_candidates.size(); ++i) {
        const auto& endpoint = model.endpoint_candidates[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(endpoint.id) << "\",\n"
            << "      \"node_id\": \"" << json_escape(endpoint.node_id) << "\",\n"
            << "      \"x\": " << endpoint.position.x << ",\n"
            << "      \"y\": " << endpoint.position.y << ",\n"
            << "      \"kind\": \"" << endpoint_kind_name(endpoint.kind) << "\",\n"
            << "      \"terminal_role\": \"" << terminal_role_name(endpoint.terminal_role) << "\",\n"
            << "      \"confidence\": \"" << confidence_name(endpoint.confidence) << "\",\n"
            << "      \"component_id\": \"" << json_escape(endpoint.component_id) << "\",\n"
            << "      \"terminal_name\": \"" << json_escape(endpoint.terminal_name) << "\",\n"
            << "      \"function_label\": \"" << json_escape(endpoint.function_label) << "\",\n"
            << "      \"wire_color\": \"" << json_escape(endpoint.wire_color) << "\"\n"
            << "    }";
        if (i + 1 != model.endpoint_candidates.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n  \"wires\": [\n";
    for (std::size_t i = 0; i < model.wires.size(); ++i) {
        const auto& wire = model.wires[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(wire.id) << "\",\n"
            << "      \"start_endpoint\": \"" << json_escape(wire.start_endpoint) << "\",\n"
            << "      \"end_endpoint\": \"" << json_escape(wire.end_endpoint) << "\",\n"
            << "      \"confidence\": \"" << confidence_name(wire.confidence) << "\",\n"
            << "      \"heavy_cable\": " << (wire.heavy_cable ? "true" : "false") << ",\n"
            << "      \"topology_edges\": [";
        for (std::size_t j = 0; j < wire.topology_edges.size(); ++j) {
            if (j) out << ", ";
            out << "\"" << json_escape(wire.topology_edges[j]) << "\"";
        }
        out << "],\n      \"conductor_segments\": [";
        for (std::size_t j = 0; j < wire.conductor_segments.size(); ++j) {
            if (j) out << ", ";
            out << "\"" << json_escape(wire.conductor_segments[j]) << "\"";
        }
        out << "]\n    }";
        if (i + 1 != model.wires.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n  \"text_recognition_evidence\": [\n";
    for (std::size_t i = 0; i < model.text_recognition_evidence.size(); ++i) {
        const auto& evidence = model.text_recognition_evidence[i];
        out << "    {\n"
            << "      \"text_region_id\": \"" << json_escape(evidence.text_region_id) << "\",\n"
            << "      \"raw_text\": \"" << json_escape(evidence.raw_text) << "\",\n"
            << "      \"confidence\": \"" << confidence_name(evidence.confidence) << "\",\n"
            << "      \"provider\": \"" << json_escape(evidence.provider) << "\"\n"
            << "    }";
        if (i + 1 != model.text_recognition_evidence.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n  \"semantic_associations\": [\n";
    for (std::size_t i = 0; i < model.semantic_associations.size(); ++i) {
        const auto& association = model.semantic_associations[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(association.id) << "\",\n"
            << "      \"text_region_id\": \"" << json_escape(association.text_region_id) << "\",\n"
            << "      \"target_id\": \"" << json_escape(association.target_id) << "\",\n"
            << "      \"target_kind\": \"" << association_target_kind_name(association.target_kind) << "\",\n"
            << "      \"relation\": \"" << association_relation_name(association.relation) << "\",\n"
            << "      \"distance\": " << association.distance << ",\n"
            << "      \"confidence\": \"" << confidence_name(association.confidence) << "\"\n"
            << "    }";
        if (i + 1 != model.semantic_associations.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n  \"text_semantic_evidence\": [\n";
    for (std::size_t i = 0; i < model.text_semantic_evidence.size(); ++i) {
        const auto& evidence = model.text_semantic_evidence[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(evidence.id) << "\",\n"
            << "      \"text_region_id\": \"" << json_escape(evidence.text_region_id) << "\",\n"
            << "      \"raw_text\": \"" << json_escape(evidence.raw_text) << "\",\n"
            << "      \"normalized_text\": \"" << json_escape(evidence.normalized_text) << "\",\n"
            << "      \"kind\": \"" << text_semantic_kind_name(evidence.kind) << "\",\n"
            << "      \"confidence\": \"" << confidence_name(evidence.confidence) << "\",\n"
            << "      \"source\": \"" << json_escape(evidence.source) << "\"\n"
            << "    }";
        if (i + 1 != model.text_semantic_evidence.size()) out << ",";
        out << "\n";
    }

    out << "  ],\n  \"engineering_object_semantics\": [\n";
    for (std::size_t i = 0; i < model.engineering_object_semantics.size(); ++i) {
        const auto& resolution = model.engineering_object_semantics[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(resolution.id) << "\",\n"
            << "      \"text_region_id\": \"" << json_escape(resolution.text_region_id) << "\",\n"
            << "      \"target_id\": \"" << json_escape(resolution.target_id) << "\",\n"
            << "      \"target_kind\": \"" << association_target_kind_name(resolution.target_kind) << "\",\n"
            << "      \"semantic_kind\": \"" << text_semantic_kind_name(resolution.semantic_kind) << "\",\n"
            << "      \"raw_text\": \"" << json_escape(resolution.raw_text) << "\",\n"
            << "      \"normalized_text\": \"" << json_escape(resolution.normalized_text) << "\",\n"
            << "      \"confidence\": \"" << confidence_name(resolution.confidence) << "\",\n"
            << "      \"distance\": " << resolution.distance << ",\n"
            << "      \"source\": \"" << json_escape(resolution.source) << "\"\n"
            << "    }";
        if (i + 1 != model.engineering_object_semantics.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n  \"component_identity_evidence\": [\n";
    for (std::size_t i = 0; i < model.component_identity_evidence.size(); ++i) {
        const auto& evidence = model.component_identity_evidence[i];
        const char* kind =
            evidence.kind == ComponentIdentityEvidenceKind::ConnectorLabel
                ? "connector_label"
                : "component_label";
        out << "    {\n"
            << "      \"id\": \"" << json_escape(evidence.id) << "\",\n"
            << "      \"component_id\": \"" << json_escape(evidence.component_id) << "\",\n"
            << "      \"kind\": \"" << kind << "\",\n"
            << "      \"raw_text\": \"" << json_escape(evidence.raw_text) << "\",\n"
            << "      \"normalized_text\": \"" << json_escape(evidence.normalized_text) << "\",\n"
            << "      \"confidence\": \"" << confidence_name(evidence.confidence) << "\",\n"
            << "      \"distance\": " << evidence.distance << ",\n"
            << "      \"source\": \"" << json_escape(evidence.source) << "\"\n"
            << "    }";
        if (i + 1 != model.component_identity_evidence.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n  \"component_identity_resolutions\": [\n";
    for (std::size_t i = 0; i < model.component_identity_resolutions.size(); ++i) {
        const auto& resolution = model.component_identity_resolutions[i];
        const char* status =
            resolution.status == ComponentIdentityResolutionStatus::Resolved
                ? "resolved"
                : resolution.status == ComponentIdentityResolutionStatus::Conflicted
                    ? "conflicted"
                    : "unresolved";
        out << "    {\n"
            << "      \"id\": \"" << json_escape(resolution.id) << "\",\n"
            << "      \"component_id\": \"" << json_escape(resolution.component_id) << "\",\n"
            << "      \"identity\": \"" << json_escape(resolution.identity) << "\",\n"
            << "      \"confidence\": \"" << confidence_name(resolution.confidence) << "\",\n"
            << "      \"status\": \"" << status << "\",\n"
            << "      \"evidence_ids\": [";
        for (std::size_t j = 0; j < resolution.evidence_ids.size(); ++j) {
            if (j) out << ", ";
            out << "\"" << json_escape(resolution.evidence_ids[j]) << "\"";
        }
        out << "]\n    }";
        if (i + 1 != model.component_identity_resolutions.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n  \"component_identity_canonicalizations\": [\n";
    for (std::size_t i = 0; i < model.component_identity_canonicalizations.size(); ++i) {
        const auto& canonicalization =
            model.component_identity_canonicalizations[i];
        const char* status =
            canonicalization.status ==
                ComponentIdentityCanonicalizationStatus::Resolved
                ? "resolved"
                : canonicalization.status ==
                    ComponentIdentityCanonicalizationStatus::Conflicted
                    ? "conflicted"
                    : "not_found";
        out << "    {\n"
            << "      \"id\": \"" << json_escape(canonicalization.id) << "\",\n"
            << "      \"component_id\": \"" << json_escape(canonicalization.component_id) << "\",\n"
            << "      \"source_resolution_id\": \"" << json_escape(canonicalization.source_resolution_id) << "\",\n"
            << "      \"source_identity\": \"" << json_escape(canonicalization.source_identity) << "\",\n"
            << "      \"canonical_id\": \"" << json_escape(canonicalization.canonical_id) << "\",\n"
            << "      \"canonical_name\": \"" << json_escape(canonicalization.canonical_name) << "\",\n"
            << "      \"confidence\": \"" << confidence_name(canonicalization.confidence) << "\",\n"
            << "      \"status\": \"" << status << "\"\n"
            << "    }";
        if (i + 1 != model.component_identity_canonicalizations.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n  \"electrical_nets\": [\n";
    for (std::size_t i = 0; i < model.electrical_nets.size(); ++i) {
        const auto& net = model.electrical_nets[i];
        out << "    {\n"
            << "      \"id\": \"" << json_escape(net.id) << "\",\n"
            << "      \"role\": \"" << distribution_role_name(net.role) << "\",\n"
            << "      \"confidence\": \"" << confidence_name(net.confidence) << "\",\n"
            << "      \"anchor_endpoint\": \"" << json_escape(net.anchor_endpoint) << "\",\n"
            << "      \"endpoint_ids\": [";
        for (std::size_t j = 0; j < net.endpoint_ids.size(); ++j) {
            if (j) out << ", ";
            out << "\"" << json_escape(net.endpoint_ids[j]) << "\"";
        }
        out << "],\n      \"splice_node_ids\": [";
        for (std::size_t j = 0; j < net.splice_node_ids.size(); ++j) {
            if (j) out << ", ";
            out << "\"" << json_escape(net.splice_node_ids[j]) << "\"";
        }
        out << "],\n      \"topology_edges\": [";
        for (std::size_t j = 0; j < net.topology_edges.size(); ++j) {
            if (j) out << ", ";
            out << "\"" << json_escape(net.topology_edges[j]) << "\"";
        }
        out << "]\n    }";
        if (i + 1 != model.electrical_nets.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n}\n";
}
} // namespace eke::dx::wire

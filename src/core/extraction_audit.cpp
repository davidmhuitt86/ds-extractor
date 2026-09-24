#include "eke_dx_wire/core/extraction_audit.hpp"

#include <map>

namespace eke::dx::wire {

ExtractionAudit build_extraction_audit(
    const WireModel& model,
    std::size_t gaps_bridged) {

    ExtractionAudit audit;
    audit.conductor_segments = model.conductor_segments.size();

    audit.topology_nodes = model.nodes.size();
    audit.topology_edges = model.edges.size();

    for (const auto& node : model.nodes) {
        switch (node.type) {
        case TopologyNodeType::ConductorEnd:
            ++audit.conductor_end_nodes;
            break;
        case TopologyNodeType::Continuation:
            ++audit.continuation_nodes;
            break;
        case TopologyNodeType::Junction:
            ++audit.junction_nodes;
            break;
        case TopologyNodeType::Splice:
            ++audit.splice_nodes;
            break;
        case TopologyNodeType::Crossing:
            ++audit.crossing_nodes;
            break;
        case TopologyNodeType::ComponentBoundary:
            ++audit.component_boundary_nodes;
            break;
        case TopologyNodeType::Unresolved:
            ++audit.unresolved_nodes;
            break;
        }
    }

    audit.endpoint_candidates = model.endpoint_candidates.size();
    for (const auto& endpoint : model.endpoint_candidates) {
        switch (endpoint.kind) {
        case EndpointKind::GeometricConductorEnd:
            ++audit.geometric_endpoints;
            break;
        case EndpointKind::ComponentTerminal:
            ++audit.component_terminals;
            break;
        case EndpointKind::ConnectorTerminal:
            ++audit.connector_terminals;
            break;
        case EndpointKind::Ground:
            ++audit.ground_endpoints;
            break;
        case EndpointKind::ExternalConnection:
            ++audit.external_connections;
            break;
        case EndpointKind::Splice:
            ++audit.splice_endpoints;
            break;
        case EndpointKind::Unresolved:
            ++audit.unresolved_endpoints;
            break;
        }
    }

    audit.shapes = model.component_candidates.size();
    audit.component_symbol_recognitions =
        model.component_symbol_recognitions.size();
    for (const auto& component : model.component_candidates) {
        switch (component.kind) {
        case ComponentCandidateKind::Enclosure:
            ++audit.enclosure_shapes;
            break;
        case ComponentCandidateKind::CircularSymbol:
            ++audit.circular_shapes;
            break;
        case ComponentCandidateKind::ChassisGround:
            ++audit.chassis_ground_shapes;
            break;
        case ComponentCandidateKind::PrimitiveSymbol:
            ++audit.primitive_shapes;
            break;
        case ComponentCandidateKind::DiagramFurniture:
            ++audit.diagram_furniture_shapes;
            break;
        case ComponentCandidateKind::Unknown:
            ++audit.unknown_shapes;
            break;
        }
    }

    audit.wires = model.wires.size();
    for (const auto& wire : model.wires) {
        if (wire.heavy_cable) {
            ++audit.heavy_cable_wires;
        }
        if (wire.confidence == ConfidenceClass::Unresolved) {
            ++audit.unresolved_wires;
        }
    }

    audit.electrical_nets = model.electrical_nets.size();
    for (const auto& net : model.electrical_nets) {
        switch (net.role) {
        case DistributionRole::Ground:
            ++audit.ground_nets;
            break;
        case DistributionRole::PowerFeed:
            ++audit.power_feed_nets;
            break;
        case DistributionRole::SharedFunctionFeed:
            ++audit.shared_function_feed_nets;
            break;
        case DistributionRole::Unknown:
            ++audit.unresolved_nets;
            break;
        }
    }

    audit.valid_wires = model.wire_validation.valid_wires;
    for (const auto& issue : model.wire_validation.issues) {
        if (issue.severity == WireValidationSeverity::Error) {
            ++audit.validation_errors;
        } else {
            ++audit.validation_warnings;
        }
    }

    std::map<std::string, std::size_t> warning_counts;
    for (const auto& item : model.wire_validation.issues) {
        if (item.severity == WireValidationSeverity::Warning) {
            ++warning_counts[item.code];
        }
    }

    audit.validation_warning_summaries.reserve(warning_counts.size());
    for (const auto& [code, count] : warning_counts) {
        audit.validation_warning_summaries.push_back({code, count});
    }

    audit.gaps_bridged = gaps_bridged;

    for (const auto& geometry : model.component_symbol_geometries) {
        if (geometry.primitive_ids.empty()) {
            ++audit.components_without_symbol_geometry;
        } else {
            ++audit.components_with_symbol_geometry;
        }
    }

    audit.symbol_primitives = model.symbol_primitives.size();
    for (const auto& primitive : model.symbol_primitives) {
        switch (primitive.kind) {
        case SymbolPrimitiveKind::Line:
            ++audit.symbol_primitive_lines;
            break;
        case SymbolPrimitiveKind::Circle:
            ++audit.symbol_primitive_circles;
            break;
        case SymbolPrimitiveKind::Rectangle:
            ++audit.symbol_primitive_rectangles;
            break;
        case SymbolPrimitiveKind::TerminalLead:
            ++audit.symbol_primitive_terminal_leads;
            break;
        case SymbolPrimitiveKind::Unknown:
            ++audit.symbol_primitive_unknown;
            break;
        }
    }

    WireSemanticCoverage& semantics = audit.wire_semantics;
    semantics.total = model.wire_semantics.size();
    for (const auto& resolution : model.wire_semantics) {
        switch (resolution.wire_color_status) {
        case WireSemanticStatus::Resolved: ++semantics.wire_color_resolved; break;
        case WireSemanticStatus::Conflicted: ++semantics.wire_color_conflicted; break;
        case WireSemanticStatus::Unresolved: ++semantics.wire_color_unresolved; break;
        }

        switch (resolution.function_status) {
        case WireSemanticStatus::Resolved: ++semantics.function_resolved; break;
        case WireSemanticStatus::Conflicted: ++semantics.function_conflicted; break;
        case WireSemanticStatus::Unresolved: ++semantics.function_unresolved; break;
        }

        const bool component_resolved =
            resolution.start_component_status == WireSemanticStatus::Resolved ||
            resolution.end_component_status == WireSemanticStatus::Resolved;
        const bool component_conflicted =
            resolution.start_component_status == WireSemanticStatus::Conflicted ||
            resolution.end_component_status == WireSemanticStatus::Conflicted;
        if (component_resolved) {
            ++semantics.component_association_resolved;
        } else if (component_conflicted) {
            ++semantics.component_association_conflicted;
        } else {
            ++semantics.component_association_unresolved;
        }

        if (resolution.start_connector_status == WireSemanticStatus::Resolved ||
            resolution.end_connector_status == WireSemanticStatus::Resolved) {
            ++semantics.connector_association_resolved;
        }

        switch (resolution.electrical_net_status) {
        case WireSemanticStatus::Resolved: ++semantics.electrical_net_resolved; break;
        case WireSemanticStatus::Conflicted: ++semantics.electrical_net_conflicted; break;
        case WireSemanticStatus::Unresolved: ++semantics.electrical_net_unresolved; break;
        }

        const bool any_resolved =
            resolution.wire_color_status == WireSemanticStatus::Resolved ||
            resolution.function_status == WireSemanticStatus::Resolved ||
            component_resolved ||
            resolution.start_connector_status == WireSemanticStatus::Resolved ||
            resolution.end_connector_status == WireSemanticStatus::Resolved ||
            resolution.electrical_net_status == WireSemanticStatus::Resolved;
        if (!any_resolved) {
            ++semantics.fully_unresolved;
        }
    }

    SymbolFamilyCoverage& families = audit.symbol_families;
    families.total = model.symbol_family_resolutions.size();
    for (const auto& resolution : model.symbol_family_resolutions) {
        switch (resolution.status) {
        case SymbolFamilyResolutionStatus::Resolved: {
            ++families.resolved;
            switch (resolution.family) {
            case SymbolFamily::Ground: ++families.ground_resolved; break;
            case SymbolFamily::Lamp: ++families.lamp_resolved; break;
            case SymbolFamily::Switch: ++families.switch_resolved; break;
            case SymbolFamily::Relay: ++families.relay_resolved; break;
            case SymbolFamily::Motor: ++families.motor_resolved; break;
            case SymbolFamily::Diode: ++families.diode_resolved; break;
            case SymbolFamily::Alternator: ++families.alternator_resolved; break;
            case SymbolFamily::Battery: ++families.battery_resolved; break;
            case SymbolFamily::Solenoid: ++families.solenoid_resolved; break;
            case SymbolFamily::Coil: ++families.coil_resolved; break;
            case SymbolFamily::Unknown: break;
            }
            break;
        }
        case SymbolFamilyResolutionStatus::Unresolved:
            ++families.unresolved;
            break;
        case SymbolFamilyResolutionStatus::Conflicted:
            ++families.conflicted;
            break;
        }
    }

    return audit;
}

} // namespace eke::dx::wire

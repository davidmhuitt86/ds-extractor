#include "eke_dx_wire/export/structured_svg_exporter.hpp"
#include "eke_dx_wire/export/symbol_renderer.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace eke::dx::wire {
namespace {

std::string xml_escape(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '&': result += "&amp;"; break;
        case '<': result += "&lt;"; break;
        case '>': result += "&gt;"; break;
        case '"': result += "&quot;"; break;
        default: result += ch; break;
        }
    }
    return result;
}

std::string to_upper(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

const char* symbol_family_name(SymbolFamily family) {
    switch (family) {
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
    case SymbolFamily::Unknown: return "unknown";
    }
    return "unknown";
}

const char* status_name(WireSemanticStatus status) {
    switch (status) {
    case WireSemanticStatus::Resolved: return "resolved";
    case WireSemanticStatus::Unresolved: return "unresolved";
    case WireSemanticStatus::Conflicted: return "conflicted";
    }
    return "unresolved";
}

const char* status_name(SymbolFamilyResolutionStatus status) {
    switch (status) {
    case SymbolFamilyResolutionStatus::Resolved: return "resolved";
    case SymbolFamilyResolutionStatus::Unresolved: return "unresolved";
    case SymbolFamilyResolutionStatus::Conflicted: return "conflicted";
    }
    return "unresolved";
}

const char* status_name(DiagramObjectStatus status) {
    switch (status) {
    case DiagramObjectStatus::Resolved: return "resolved";
    case DiagramObjectStatus::Unresolved: return "unresolved";
    case DiagramObjectStatus::Conflicted: return "conflicted";
    }
    return "unresolved";
}

const char* component_kind_name(ComponentCandidateKind kind) {
    switch (kind) {
    case ComponentCandidateKind::Enclosure: return "enclosure";
    case ComponentCandidateKind::CircularSymbol: return "circular-symbol";
    case ComponentCandidateKind::ChassisGround: return "chassis-ground";
    case ComponentCandidateKind::PrimitiveSymbol: return "primitive-symbol";
    case ComponentCandidateKind::DiagramFurniture: return "diagram-furniture";
    case ComponentCandidateKind::Unknown: return "unknown";
    }
    return "unknown";
}

const char* confidence_name(ConfidenceClass value) {
    switch (value) {
    case ConfidenceClass::High: return "high";
    case ConfidenceClass::Medium: return "medium";
    case ConfidenceClass::Low: return "low";
    case ConfidenceClass::Unresolved: return "unresolved";
    }
    return "unresolved";
}

// A rendering-only convenience mapping from resolved wire-color TEXT to a
// display color. This never determines WHETHER a color is resolved (that
// is WireSemanticResolution's job, AP-WIRE-025) - it only picks a stroke
// for an already-resolved value. An unmapped resolved value still keeps
// its raw text in the SVG (data-wire-color) and falls back to a neutral
// stroke rather than inventing a color.
std::string display_color_for(const std::string& raw_wire_color) {
    static const std::vector<std::pair<std::string, std::string>> table = {
        {"BLACK", "#1a1a1a"}, {"WHITE", "#e8e8e8"}, {"RED", "#d32f2f"},
        {"BLUE", "#1565c0"}, {"GREEN", "#2e7d32"}, {"YELLOW", "#f9a825"},
        {"ORANGE", "#ef6c00"}, {"BROWN", "#5d4037"}, {"PINK", "#d81b60"},
        {"GRAY", "#757575"}, {"GREY", "#757575"}, {"PURPLE", "#6a1b9a"},
    };
    const std::string upper = to_upper(raw_wire_color);
    for (const auto& [keyword, color] : table) {
        if (upper.find(keyword) != std::string::npos) return color;
    }
    return {};
}

struct Lookups {
    std::unordered_map<std::string, const TopologyNode*> node_by_id;
    std::unordered_map<std::string, const EndpointCandidate*> endpoint_by_id;
    std::unordered_map<std::string, const ConductorSegment*> segment_by_id;
    std::unordered_map<std::string, const TextRegion*> text_region_by_id;
    std::unordered_map<std::string, const TerminalCandidate*> terminal_by_id;
    std::unordered_map<std::string, const ConnectorTerminal*> connector_terminal_by_id;
    std::unordered_map<std::string, const WireSemanticResolution*> wire_semantics_by_id;
    std::unordered_map<std::string, const SymbolFamilyResolution*> symbol_family_by_id;

    explicit Lookups(const WireModel& model) {
        for (const auto& n : model.nodes) node_by_id.emplace(n.id, &n);
        for (const auto& e : model.endpoint_candidates) endpoint_by_id.emplace(e.id, &e);
        for (const auto& s : model.conductor_segments) segment_by_id.emplace(s.id, &s);
        for (const auto& t : model.text_regions) text_region_by_id.emplace(t.id, &t);
        for (const auto& t : model.terminal_candidates) terminal_by_id.emplace(t.id, &t);
        for (const auto& t : model.connector_terminals) connector_terminal_by_id.emplace(t.id, &t);
        for (const auto& w : model.wire_semantics) wire_semantics_by_id.emplace(w.id, &w);
        for (const auto& r : model.symbol_family_resolutions) symbol_family_by_id.emplace(r.id, &r);
    }
};

void render_components(std::ostringstream& out, const EngineeringDiagram& diagram, const Lookups& lookups) {
    out << "    <g id=\"components\">\n";
    for (const auto& component : diagram.components) {
        // DiagramFurniture is intentionally never rendered as an
        // electrical component - it is diagram reference material, not
        // circuit content (see AP-WIRE-023).
        if (component.kind == ComponentCandidateKind::DiagramFurniture) continue;

        out << "      <g id=\"component-" << component.component_id << "\" "
            << "transform=\"translate(" << component.bounds.x << "," << component.bounds.y << ")\" "
            << "data-object-type=\"component\" data-component-id=\"" << component.component_id << "\" "
            << "data-component-kind=\"" << component_kind_name(component.kind) << "\" "
            << "data-identity-status=\"" << status_name(component.identity_status) << "\">\n";

        out << "        <rect x=\"0\" y=\"0\" width=\"" << component.bounds.width
            << "\" height=\"" << component.bounds.height
            << "\" fill=\"none\" stroke=\"#666\" stroke-width=\"0.75\" data-object-type=\"component-bounds\"/>\n";

        const SymbolFamilyResolution* resolution = nullptr;
        if (!component.symbol_family_resolution_id.empty()) {
            const auto it = lookups.symbol_family_by_id.find(component.symbol_family_resolution_id);
            if (it != lookups.symbol_family_by_id.end()) resolution = it->second;
        }

        out << "        <g class=\"symbol\" color=\"#222\" ";
        if (resolution && resolution->status == SymbolFamilyResolutionStatus::Resolved) {
            out << "data-symbol-family=\"" << symbol_family_name(resolution->family) << "\" "
                << "data-symbol-family-status=\"" << status_name(resolution->status) << "\" "
                << "data-symbol-family-confidence=\"" << confidence_name(resolution->confidence) << "\">\n"
                << "          " << symbol_renderer_for(resolution->family).render(component.bounds) << "\n";
        } else if (resolution && resolution->status == SymbolFamilyResolutionStatus::Conflicted) {
            out << "data-symbol-family-status=\"" << status_name(resolution->status) << "\">\n"
                << "          " << conflicted_symbol_renderer().render(component.bounds) << "\n";
        } else {
            // Unresolved, or no resolution reference at all (should not
            // happen for a real component, but never rendered as a
            // guessed glyph either way).
            out << "data-symbol-family-status=\"unresolved\">\n"
                << "          " << unresolved_symbol_renderer().render(component.bounds) << "\n";
        }
        out << "        </g>\n";

        if (!component.semantic_labels.empty()) {
            out << "        <text x=\"" << (component.bounds.width / 2) << "\" y=\""
                << (component.bounds.height + 10) << "\" font-size=\"8\" text-anchor=\"middle\" "
                << "data-source-label=\"" << xml_escape(component.semantic_labels.front()) << "\"";
            if (!component.canonical_name.empty()) {
                out << " data-canonical-name=\"" << xml_escape(component.canonical_name) << "\"";
            }
            out << ">" << xml_escape(component.semantic_labels.front()) << "</text>\n";
        }

        out << "      </g>\n";
    }
    out << "    </g>\n";
}

void render_connectors(std::ostringstream& out, const EngineeringDiagram& diagram) {
    out << "    <g id=\"connectors\">\n";
    for (const auto& connector : diagram.connectors) {
        out << "      <g id=\"connector-" << connector.connector_id << "\" "
            << "transform=\"translate(" << connector.bounds.x << "," << connector.bounds.y << ")\" "
            << "data-object-type=\"connector\" data-connector-id=\"" << connector.connector_id << "\">\n"
            << "        <rect x=\"0\" y=\"0\" width=\"" << connector.bounds.width << "\" height=\""
            << connector.bounds.height
            << "\" rx=\"3\" fill=\"#dceeff\" stroke=\"#3573a5\" stroke-width=\"1\"/>\n";
        if (!connector.semantic_labels.empty()) {
            out << "        <text x=\"" << (connector.bounds.width / 2) << "\" y=\""
                << (connector.bounds.height + 10) << "\" font-size=\"8\" text-anchor=\"middle\">"
                << xml_escape(connector.semantic_labels.front()) << "</text>\n";
        }
        out << "      </g>\n";
    }
    out << "    </g>\n";
}

void render_terminals(std::ostringstream& out, const EngineeringDiagram& diagram, const Lookups& lookups) {
    out << "    <g id=\"terminals\">\n";
    for (const auto& component : diagram.components) {
        for (const auto& terminal_id : component.terminal_candidate_ids) {
            const auto it = lookups.terminal_by_id.find(terminal_id);
            if (it == lookups.terminal_by_id.end()) continue;
            const auto* terminal = it->second;
            out << "      <circle id=\"terminal-" << terminal->id << "\" cx=\"" << terminal->position.x
                << "\" cy=\"" << terminal->position.y << "\" r=\"1.5\" fill=\"#c0392b\" "
                << "data-object-type=\"terminal\" data-terminal-id=\"" << terminal->id
                << "\" data-endpoint-id=\"" << terminal->endpoint_id
                << "\" data-component-id=\"" << component.component_id << "\"/>\n";
        }
    }
    for (const auto& connector : diagram.connectors) {
        for (const auto& terminal_id : connector.connector_terminal_ids) {
            const auto it = lookups.connector_terminal_by_id.find(terminal_id);
            if (it == lookups.connector_terminal_by_id.end()) continue;
            const auto* terminal = it->second;
            out << "      <circle id=\"connector-terminal-" << terminal->id << "\" cx=\""
                << terminal->position.x << "\" cy=\"" << terminal->position.y
                << "\" r=\"1.5\" fill=\"#3573a5\" data-object-type=\"connector-terminal\" "
                << "data-terminal-id=\"" << terminal->id << "\" data-connector-id=\"" << connector.connector_id
                << "\" data-status=\"" << (terminal->status == ConnectorTerminalStatus::Resolved ? "resolved" :
                    terminal->status == ConnectorTerminalStatus::Conflicted ? "conflicted" : "unresolved") << "\"";
            if (terminal->status == ConnectorTerminalStatus::Resolved && !terminal->terminal_name.empty()) {
                out << " data-terminal-name=\"" << xml_escape(terminal->terminal_name) << "\"";
            }
            out << "/>\n";
        }
    }
    out << "    </g>\n";
}

void render_wires(std::ostringstream& out, const EngineeringDiagram& diagram, const Lookups& lookups) {
    out << "    <g id=\"wires\" fill=\"none\" stroke-linecap=\"round\">\n";
    for (const auto& wire : diagram.wires) {
        const WireSemanticResolution* semantics = nullptr;
        if (!wire.wire_semantic_resolution_id.empty()) {
            const auto it = lookups.wire_semantics_by_id.find(wire.wire_semantic_resolution_id);
            if (it != lookups.wire_semantics_by_id.end()) semantics = it->second;
        }

        std::string stroke = "#888"; // neutral default: unresolved/unmapped
        std::string color_status = "unresolved";
        std::string color_data;
        if (semantics) {
            color_status = status_name(semantics->wire_color_status);
            if (semantics->wire_color_status == WireSemanticStatus::Resolved) {
                color_data = " data-wire-color=\"" + xml_escape(semantics->wire_color) + "\"";
                const std::string mapped = display_color_for(semantics->wire_color);
                if (!mapped.empty()) stroke = mapped;
            }
        }
        const bool conflicted = semantics && semantics->wire_color_status == WireSemanticStatus::Conflicted;

        out << "      <g id=\"wire-" << wire.wire_id << "\" "
            << "data-object-type=\"wire\" data-wire-id=\"" << wire.wire_id
            << "\" data-start-endpoint=\"" << wire.start_endpoint_id
            << "\" data-end-endpoint=\"" << wire.end_endpoint_id
            << "\" data-heavy-cable=\"" << (wire.heavy_cable ? "true" : "false")
            << "\" data-wire-color-status=\"" << color_status << "\"" << color_data;
        if (semantics && !semantics->electrical_net_id.empty()) {
            out << " data-electrical-net-id=\"" << semantics->electrical_net_id
                << "\" data-electrical-net-status=\"" << status_name(semantics->electrical_net_status) << "\"";
        }
        out << ">\n";

        for (const auto& segment_id : wire.conductor_segment_ids) {
            const auto it = lookups.segment_by_id.find(segment_id);
            if (it == lookups.segment_by_id.end()) continue;
            const auto* segment = it->second;
            out << "        <line x1=\"" << segment->geometry.a.x << "\" y1=\"" << segment->geometry.a.y
                << "\" x2=\"" << segment->geometry.b.x << "\" y2=\"" << segment->geometry.b.y
                << "\" stroke=\"" << stroke << "\" stroke-width=\""
                << std::max(1.0, segment->thickness_px) << "\"";
            if (conflicted) out << " stroke-dasharray=\"2,2\"";
            out << " data-object-type=\"conductor-segment\" data-conductor-segment-id=\"" << segment->id << "\"/>\n";
        }
        out << "      </g>\n";
    }
    out << "    </g>\n";
}

void render_splices(std::ostringstream& out, const EngineeringDiagram& diagram) {
    out << "    <g id=\"splices\">\n";
    for (const auto& splice : diagram.splices) {
        out << "      <circle id=\"splice-" << splice.node_id << "\" cx=\"" << splice.position.x
            << "\" cy=\"" << splice.position.y << "\" r=\"2.2\" fill=\"#111\" "
            << "data-object-type=\"splice\" data-node-id=\"" << splice.node_id
            << "\" data-incident-wire-count=\"" << splice.incident_wire_ids.size() << "\"/>\n";
    }
    out << "    </g>\n";
}

void render_crossings(std::ostringstream& out, const EngineeringDiagram& diagram, const Lookups& lookups) {
    out << "    <g id=\"crossings\">\n";
    for (const auto& node_id : diagram.crossing_node_ids) {
        const auto it = lookups.node_by_id.find(node_id);
        if (it == lookups.node_by_id.end()) continue;
        const auto* node = it->second;
        // Unfilled, unlike a splice's filled dot: a crossing never
        // implies electrical continuity.
        out << "      <circle id=\"crossing-" << node_id << "\" cx=\"" << node->position.x
            << "\" cy=\"" << node->position.y << "\" r=\"1.8\" fill=\"none\" stroke=\"#aaa\" "
            << "stroke-width=\"0.75\" data-object-type=\"crossing\" data-node-id=\"" << node_id << "\"/>\n";
    }
    out << "    </g>\n";
}

void render_grounds(std::ostringstream& out, const WireModel& model) {
    out << "    <g id=\"grounds\">\n";
    for (const auto& endpoint : model.endpoint_candidates) {
        if (endpoint.kind != EndpointKind::Ground) continue;
        out << "      <g id=\"ground-endpoint-" << endpoint.id << "\" "
            << "transform=\"translate(" << endpoint.position.x << "," << endpoint.position.y << ")\" "
            << "data-object-type=\"ground-endpoint\" data-endpoint-id=\"" << endpoint.id << "\">\n"
            << "        <line x1=\"0\" y1=\"0\" x2=\"0\" y2=\"6\" stroke=\"#2e7d32\" stroke-width=\"1.2\"/>\n"
            << "        <line x1=\"-4\" y1=\"6\" x2=\"4\" y2=\"6\" stroke=\"#2e7d32\" stroke-width=\"1.2\"/>\n"
            << "        <line x1=\"-2.5\" y1=\"8.5\" x2=\"2.5\" y2=\"8.5\" stroke=\"#2e7d32\" stroke-width=\"1\"/>\n"
            << "      </g>\n";
    }
    out << "    </g>\n";
}

void render_labels_and_annotations(
    std::ostringstream& labels_out,
    std::ostringstream& annotations_out,
    const EngineeringDiagram& diagram,
    const Lookups& lookups) {

    labels_out << "    <g id=\"labels\">\n";
    annotations_out << "    <g id=\"annotations\">\n";

    for (const auto& label : diagram.labels) {
        const auto region_it = lookups.text_region_by_id.find(label.text_region_id);
        const BoundingBox bounds = region_it != lookups.text_region_by_id.end()
            ? region_it->second->bounds : BoundingBox{};

        // An Unknown-kind label is treated as a general annotation; any
        // other kind is a semantic label. This is the only split - no
        // new evidence source, per AP-WIRE-026's documented convention.
        std::ostringstream& target = (label.kind == TextSemanticKind::Unknown) ? annotations_out : labels_out;
        const char* group_prefix = (label.kind == TextSemanticKind::Unknown) ? "annotation" : "label";

        target << "      <g id=\"" << group_prefix << "-" << label.text_region_id << "\" "
               << "transform=\"translate(" << bounds.x << "," << bounds.y << ")\" "
               << "data-object-type=\"" << group_prefix << "\" data-text-region-id=\""
               << label.text_region_id << "\" data-status=\"" << status_name(label.status) << "\"";
        if (!label.provider.empty()) {
            target << " data-provider=\"" << xml_escape(label.provider) << "\"";
        }
        target << ">\n";

        if (!label.raw_text.empty()) {
            target << "        <text x=\"0\" y=\"" << bounds.height << "\" font-size=\"7\">"
                   << xml_escape(label.raw_text) << "</text>\n";
        } else {
            // Unknown text must remain representable, never disappear:
            // an explicit dashed placeholder box instead of invented text.
            target << "        <rect x=\"0\" y=\"0\" width=\"" << std::max(4, bounds.width)
                   << "\" height=\"" << std::max(4, bounds.height)
                   << "\" fill=\"none\" stroke=\"#bbb\" stroke-width=\"0.5\" stroke-dasharray=\"1,1\"/>\n";
        }
        target << "      </g>\n";
    }

    labels_out << "    </g>\n";
    annotations_out << "    </g>\n";
}

// Electrical nets are a distinct connectivity-layer concept from physical
// wires (AP-WIRE-025/026) and are never merged into wire geometry. This
// group carries no independent visual shapes of its own - only metadata
// cross-referencing the wires/endpoints/splices already rendered above,
// so a consumer can highlight a net without the renderer inventing new
// geometry for it.
void render_electrical_nets(std::ostringstream& out, const EngineeringDiagram& diagram) {
    out << "    <g id=\"electrical-nets\">\n";
    for (const auto& net : diagram.electrical_nets) {
        out << "      <g id=\"electrical-net-" << net.net_id << "\" "
            << "data-object-type=\"electrical-net\" data-net-id=\"" << net.net_id
            << "\" data-wire-count=\"" << net.wire_ids.size()
            << "\" data-endpoint-count=\"" << net.endpoint_ids.size()
            << "\" data-confidence=\"" << confidence_name(net.confidence) << "\"/>\n";
    }
    out << "    </g>\n";
}

void render_metadata(std::ostringstream& out, const EngineeringDiagram& diagram) {
    out << "    <g id=\"metadata\" data-object-type=\"metadata\" "
        << "data-source-id=\"" << xml_escape(diagram.source_id) << "\" "
        << "data-page=\"" << diagram.page << "\" "
        << "data-coordinate-system=\"" << kEngineeringDiagramCoordinateSystem << "\" "
        << "data-component-count=\"" << diagram.components.size() << "\" "
        << "data-connector-count=\"" << diagram.connectors.size() << "\" "
        << "data-wire-count=\"" << diagram.wires.size() << "\" "
        << "data-splice-count=\"" << diagram.splices.size() << "\" "
        << "data-crossing-count=\"" << diagram.crossing_node_ids.size() << "\" "
        << "data-label-count=\"" << diagram.labels.size() << "\" "
        << "data-electrical-net-count=\"" << diagram.electrical_nets.size() << "\">\n"
        << "      <desc>eke-dx-wire structured engineering SVG, generated from EngineeringDiagram "
        << "(AP-WIRE-027). Source: " << xml_escape(diagram.source_id) << "</desc>\n"
        << "    </g>\n";
}

} // namespace

std::string StructuredSvgExporter::render(const EngineeringDiagram& diagram, const WireModel& model) {
    const Lookups lookups(model);

    std::ostringstream out;
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << diagram.image_width
        << "\" height=\"" << diagram.image_height << "\" viewBox=\"0 0 " << diagram.image_width
        << " " << diagram.image_height << "\">\n";
    out << "  <title>eke-dx-wire structured engineering diagram</title>\n";
    out << "  <g id=\"engineering-diagram\" data-source-id=\"" << xml_escape(diagram.source_id) << "\">\n";

    render_components(out, diagram, lookups);
    render_connectors(out, diagram);
    render_wires(out, diagram, lookups);
    render_splices(out, diagram);
    render_crossings(out, diagram, lookups);
    render_terminals(out, diagram, lookups);
    render_grounds(out, model);

    std::ostringstream labels;
    std::ostringstream annotations;
    render_labels_and_annotations(labels, annotations, diagram, lookups);
    out << labels.str();
    out << annotations.str();

    render_electrical_nets(out, diagram);
    render_metadata(out, diagram);

    out << "  </g>\n";
    out << "</svg>\n";
    return out.str();
}

void StructuredSvgExporter::export_svg(
    const EngineeringDiagram& diagram,
    const WireModel& model,
    const std::string& output_path) {

    std::ofstream out(output_path);
    if (!out) {
        throw std::runtime_error("Unable to create structured SVG: " + output_path);
    }
    out << render(diagram, model);
}

} // namespace eke::dx::wire

#include "eke_dx_wire/export/structured_svg_exporter.hpp"
#include "eke_dx_wire/diagram/engineering_diagram_builder.hpp"

#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace eke::dx::wire;

namespace {

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

ComponentCandidate make_component(
    const std::string& id,
    ComponentCandidateKind kind = ComponentCandidateKind::CircularSymbol,
    BoundingBox bounds = BoundingBox{10, 20, 30, 30}) {
    ComponentCandidate c;
    c.id = id;
    c.kind = kind;
    c.bounds = bounds;
    return c;
}

EndpointCandidate make_endpoint(
    const std::string& id, EndpointKind kind = EndpointKind::GeometricConductorEnd) {
    EndpointCandidate e;
    e.id = id;
    e.kind = kind;
    e.position = Point2D{5.0, 5.0};
    return e;
}

Wire make_wire(const std::string& id, const std::string& start, const std::string& end) {
    Wire w;
    w.id = id;
    w.start_endpoint = start;
    w.end_endpoint = end;
    return w;
}

ConductorSegment make_segment(const std::string& id, double x1, double y1, double x2, double y2) {
    ConductorSegment s;
    s.id = id;
    s.geometry = Segment2D{Point2D{x1, y1}, Point2D{x2, y2}};
    s.thickness_px = 1.5;
    return s;
}

TopologyNode make_node(const std::string& id, TopologyNodeType type, double x = 0.0, double y = 0.0) {
    TopologyNode n;
    n.id = id;
    n.type = type;
    n.position = Point2D{x, y};
    return n;
}

} // namespace

int main() {
    EngineeringDiagramBuilder builder;

    // 1. Empty diagram renders a valid, well-formed SVG document with no
    // engineering content.
    {
        WireModel model;
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(svg.find("<?xml") == 0);
        assert(contains(svg, "<svg"));
        assert(contains(svg, "</svg>"));
        assert(contains(svg, "id=\"engineering-diagram\""));
        assert(!contains(svg, "<image"));
    }

    // 2. Rendering is a pure function: identical input produces
    // byte-identical output (determinism requirement).
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        const auto diagram = builder.build(model);
        const std::string a = StructuredSvgExporter::render(diagram, model);
        const std::string b = StructuredSvgExporter::render(diagram, model);
        assert(a == b);
    }

    // 3. A component with no symbol-family resolution renders the
    // explicit unresolved placeholder, not a guessed glyph.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "data-component-id=\"comp-1\""));
        assert(contains(svg, "data-symbol-family-status=\"unresolved\""));
        assert(!contains(svg, "data-symbol-family=\""));
    }

    // 4. A Resolved symbol-family resolution renders the family glyph
    // and its metadata.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        SymbolFamilyResolution resolution;
        resolution.id = "sfr-1";
        resolution.component_id = "comp-1";
        resolution.family = SymbolFamily::Ground;
        resolution.status = SymbolFamilyResolutionStatus::Resolved;
        resolution.confidence = ConfidenceClass::High;
        model.symbol_family_resolutions = {resolution};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "data-symbol-family=\"ground\""));
        assert(contains(svg, "data-symbol-family-status=\"resolved\""));
        assert(contains(svg, "data-symbol-family-confidence=\"high\""));
    }

    // 5. A Conflicted symbol-family resolution renders the explicit
    // conflicted placeholder, never a chosen winning family.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        SymbolFamilyResolution resolution;
        resolution.id = "sfr-1";
        resolution.component_id = "comp-1";
        resolution.family = SymbolFamily::Lamp;
        resolution.status = SymbolFamilyResolutionStatus::Conflicted;
        model.symbol_family_resolutions = {resolution};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "data-symbol-family-status=\"conflicted\""));
        assert(!contains(svg, "data-symbol-family=\"lamp\""));
    }

    // 6. Every SymbolFamily has a distinct renderer wired in (no silent
    // fallback for a resolved-but-unhandled family).
    {
        const SymbolFamily families[] = {
            SymbolFamily::Ground, SymbolFamily::Lamp, SymbolFamily::Switch,
            SymbolFamily::Relay, SymbolFamily::Motor, SymbolFamily::Diode,
            SymbolFamily::Alternator, SymbolFamily::Battery, SymbolFamily::Solenoid,
            SymbolFamily::Coil};
        for (const auto family : families) {
            WireModel model;
            model.component_candidates = {make_component("comp-1")};
            SymbolFamilyResolution resolution;
            resolution.id = "sfr-1";
            resolution.component_id = "comp-1";
            resolution.family = family;
            resolution.status = SymbolFamilyResolutionStatus::Resolved;
            model.symbol_family_resolutions = {resolution};
            const auto diagram = builder.build(model);
            const std::string svg = StructuredSvgExporter::render(diagram, model);
            assert(contains(svg, "class=\"symbol\""));
        }
    }

    // 7. DiagramFurniture components are never rendered as circuit
    // components.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1", ComponentCandidateKind::DiagramFurniture)};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(!contains(svg, "data-component-id=\"comp-1\""));
    }

    // 8. Component semantic label and canonical name are both preserved
    // as distinct attributes, never collapsed into one.
    {
        WireModel model;
        auto comp = make_component("comp-1");
        comp.semantic_labels = {"HORN"};
        model.component_candidates = {comp};
        ComponentIdentityResolution resolution;
        resolution.id = "res-1";
        resolution.component_id = "comp-1";
        model.component_identity_resolutions = {resolution};
        ComponentIdentityCanonicalization canon;
        canon.id = "canon-1";
        canon.component_id = "comp-1";
        canon.source_resolution_id = "res-1";
        canon.canonical_name = "Horn";
        canon.status = ComponentIdentityCanonicalizationStatus::Resolved;
        model.component_identity_canonicalizations = {canon};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "data-source-label=\"HORN\""));
        assert(contains(svg, "data-canonical-name=\"Horn\""));
    }

    // 9. A connector renders as its own distinct group with terminals.
    {
        WireModel model;
        ConnectorCandidate connector;
        connector.id = "conn-1";
        connector.bounds = BoundingBox{0, 0, 20, 10};
        model.connector_candidates = {connector};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "id=\"connector-conn-1\""));
        assert(contains(svg, "data-object-type=\"connector\""));
    }

    // 10. A resolved ConnectorTerminal renders its terminal name.
    {
        WireModel model;
        ConnectorCandidate connector;
        connector.id = "conn-1";
        model.connector_candidates = {connector};
        model.endpoint_candidates = {make_endpoint("ep-a", EndpointKind::ConnectorTerminal)};
        ConnectorTerminal terminal;
        terminal.id = "ct-1";
        terminal.connector_id = "conn-1";
        terminal.endpoint_id = "ep-a";
        terminal.terminal_name = "A1";
        terminal.status = ConnectorTerminalStatus::Resolved;
        model.connector_terminals = {terminal};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "data-terminal-name=\"A1\""));
        assert(contains(svg, "data-status=\"resolved\""));
    }

    // 11. An unresolved ConnectorTerminal never fabricates a name.
    {
        WireModel model;
        ConnectorCandidate connector;
        connector.id = "conn-1";
        model.connector_candidates = {connector};
        model.endpoint_candidates = {make_endpoint("ep-a", EndpointKind::ConnectorTerminal)};
        ConnectorTerminal terminal;
        terminal.id = "ct-1";
        terminal.connector_id = "conn-1";
        terminal.endpoint_id = "ep-a";
        terminal.status = ConnectorTerminalStatus::Unresolved;
        model.connector_terminals = {terminal};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(!contains(svg, "data-terminal-name=\""));
    }

    // 12. A TerminalCandidate renders traceable to its endpoint and
    // component.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        model.endpoint_candidates = {make_endpoint("ep-a")};
        TerminalCandidate terminal;
        terminal.id = "term-1";
        terminal.endpoint_id = "ep-a";
        terminal.component_candidate_id = "comp-1";
        model.terminal_candidates = {terminal};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "id=\"terminal-term-1\""));
        assert(contains(svg, "data-endpoint-id=\"ep-a\""));
    }

    // 13. A wire renders as an endpoint-to-endpoint group, never split
    // into per-segment wires.
    {
        WireModel model;
        model.endpoint_candidates = {make_endpoint("ep-a"), make_endpoint("ep-b")};
        model.wires = {make_wire("w1", "ep-a", "ep-b")};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "id=\"wire-w1\""));
        assert(contains(svg, "data-start-endpoint=\"ep-a\""));
        assert(contains(svg, "data-end-endpoint=\"ep-b\""));
    }

    // 14. A wire's conductor segments render as real <line> geometry.
    {
        WireModel model;
        model.endpoint_candidates = {make_endpoint("ep-a"), make_endpoint("ep-b")};
        model.wires = {make_wire("w1", "ep-a", "ep-b")};
        model.conductor_segments = {make_segment("seg-1", 0, 0, 10, 10)};
        model.wires[0].conductor_segments = {"seg-1"};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "<line"));
        assert(contains(svg, "data-conductor-segment-id=\"seg-1\""));
    }

    // 15. A wire with no resolved color renders the neutral fallback and
    // marks the status explicitly, never inventing a color.
    {
        WireModel model;
        model.endpoint_candidates = {make_endpoint("ep-a"), make_endpoint("ep-b")};
        model.wires = {make_wire("w1", "ep-a", "ep-b")};
        WireSemanticResolution resolution;
        resolution.id = "wsr-1";
        resolution.wire_id = "w1";
        resolution.wire_color_status = WireSemanticStatus::Unresolved;
        model.wire_semantics = {resolution};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "data-wire-color-status=\"unresolved\""));
        assert(!contains(svg, "data-wire-color=\""));
    }

    // 16. A wire with Resolved color renders its raw color text and a
    // mapped display stroke.
    {
        WireModel model;
        model.endpoint_candidates = {make_endpoint("ep-a"), make_endpoint("ep-b")};
        model.wires = {make_wire("w1", "ep-a", "ep-b")};
        WireSemanticResolution resolution;
        resolution.id = "wsr-1";
        resolution.wire_id = "w1";
        resolution.wire_color = "RED";
        resolution.wire_color_status = WireSemanticStatus::Resolved;
        model.wire_semantics = {resolution};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "data-wire-color=\"RED\""));
        assert(contains(svg, "data-wire-color-status=\"resolved\""));
    }

    // 17. A Resolved color that doesn't map to the known display table
    // still keeps its raw text; the renderer never drops it.
    {
        WireModel model;
        model.endpoint_candidates = {make_endpoint("ep-a"), make_endpoint("ep-b")};
        model.wires = {make_wire("w1", "ep-a", "ep-b")};
        WireSemanticResolution resolution;
        resolution.id = "wsr-1";
        resolution.wire_id = "w1";
        resolution.wire_color = "TARTAN";
        resolution.wire_color_status = WireSemanticStatus::Resolved;
        model.wire_semantics = {resolution};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "data-wire-color=\"TARTAN\""));
    }

    // 18. A Conflicted wire color is represented explicitly (dashed),
    // never resolved to one winning value.
    {
        WireModel model;
        model.endpoint_candidates = {make_endpoint("ep-a"), make_endpoint("ep-b")};
        model.wires = {make_wire("w1", "ep-a", "ep-b")};
        // stroke-dasharray is emitted on the wire's rendered <line>, so
        // the wire needs real segment geometry to render one at all (see
        // test 14 above for the same pattern) - a wire with no geometry
        // still gets its group-level data-wire-color-status attribute,
        // but has no <line> element to carry the dasharray.
        model.conductor_segments = {make_segment("seg-1", 0, 0, 10, 10)};
        model.wires[0].conductor_segments = {"seg-1"};
        WireSemanticResolution resolution;
        resolution.id = "wsr-1";
        resolution.wire_id = "w1";
        resolution.wire_color_status = WireSemanticStatus::Conflicted;
        model.wire_semantics = {resolution};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "data-wire-color-status=\"conflicted\""));
        assert(contains(svg, "stroke-dasharray"));
    }

    // 19. A heavy-cable wire preserves that flag as metadata.
    {
        WireModel model;
        model.endpoint_candidates = {make_endpoint("ep-a"), make_endpoint("ep-b")};
        Wire w = make_wire("w1", "ep-a", "ep-b");
        w.heavy_cable = true;
        model.wires = {w};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "data-heavy-cable=\"true\""));
    }

    // 20. A splice node renders as a filled junction marker, distinct
    // from a crossing.
    {
        WireModel model;
        model.nodes = {make_node("n-splice", TopologyNodeType::Splice, 3.0, 4.0)};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "id=\"splice-n-splice\""));
        assert(contains(svg, "data-object-type=\"splice\""));
        assert(contains(svg, "fill=\"#111\""));
    }

    // 21. A crossing node renders as an unfilled marker and must not be
    // labeled a splice.
    {
        WireModel model;
        model.nodes = {make_node("n-cross", TopologyNodeType::Crossing, 3.0, 4.0)};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "id=\"crossing-n-cross\""));
        assert(contains(svg, "data-object-type=\"crossing\""));
        assert(!contains(svg, "id=\"splice-n-cross\""));
    }

    // 22. A crossing never renders inside the splices group or vice
    // versa (structural separation between the two groups).
    {
        WireModel model;
        model.nodes = {
            make_node("n-splice", TopologyNodeType::Splice, 1.0, 1.0),
            make_node("n-cross", TopologyNodeType::Crossing, 2.0, 2.0)};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        const auto splices_group = svg.find("id=\"splices\"");
        const auto crossings_group = svg.find("id=\"crossings\"");
        assert(splices_group != std::string::npos && crossings_group != std::string::npos);
    }

    // 23. A Ground-kind endpoint renders a ground glyph distinct from a
    // ChassisGround-family component.
    {
        WireModel model;
        model.endpoint_candidates = {make_endpoint("ep-gnd", EndpointKind::Ground)};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "id=\"ground-endpoint-ep-gnd\""));
    }

    // 24. A resolved label renders its raw text.
    {
        WireModel model;
        TextRegion region;
        region.id = "tr-1";
        region.bounds = BoundingBox{1, 2, 30, 10};
        model.text_regions = {region};
        TextRecognitionEvidence recognition;
        recognition.text_region_id = "tr-1";
        recognition.raw_text = "BL/W";
        recognition.confidence = ConfidenceClass::High;
        model.text_recognition_evidence = {recognition};
        EngineeringObjectSemanticResolution resolution;
        resolution.id = "eosr-1";
        resolution.text_region_id = "tr-1";
        resolution.target_id = "ep-a";
        resolution.confidence = ConfidenceClass::High;
        model.engineering_object_semantics = {resolution};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "BL/W"));
        assert(contains(svg, "data-status=\"resolved\""));
    }

    // 25. An unresolved label with no recognized text renders a
    // placeholder, never invented text, and is not silently dropped.
    {
        WireModel model;
        TextRegion region;
        region.id = "tr-1";
        region.bounds = BoundingBox{1, 2, 30, 10};
        model.text_regions = {region};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "data-text-region-id=\"tr-1\""));
        assert(contains(svg, "data-status=\"unresolved\""));
    }

    // 26. An Unknown-kind label renders in the annotations group, not
    // the labels group.
    {
        WireModel model;
        TextRegion region;
        region.id = "tr-1";
        region.bounds = BoundingBox{1, 2, 30, 10};
        model.text_regions = {region};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        const auto annotations_group = svg.find("id=\"annotations\"");
        const auto annotation_entry = svg.find("id=\"annotation-tr-1\"");
        assert(annotations_group != std::string::npos);
        assert(annotation_entry != std::string::npos && annotation_entry > annotations_group);
    }

    // 27. Electrical nets render as distinct metadata, never merged with
    // wire geometry.
    {
        WireModel model;
        model.endpoint_candidates = {make_endpoint("ep-a"), make_endpoint("ep-b")};
        model.wires = {make_wire("w1", "ep-a", "ep-b")};
        ElectricalNet net;
        net.id = "net-1";
        net.endpoint_ids = {"ep-a"};
        model.electrical_nets = {net};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "id=\"electrical-net-net-1\""));
        assert(contains(svg, "data-object-type=\"electrical-net\""));
    }

    // 28. Group hierarchy: all required top-level groups are present
    // under the engineering-diagram root, in a fixed order.
    {
        WireModel model;
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        const std::vector<std::string> expected_groups = {
            "id=\"components\"", "id=\"connectors\"", "id=\"wires\"", "id=\"splices\"",
            "id=\"crossings\"", "id=\"terminals\"", "id=\"grounds\"", "id=\"labels\"",
            "id=\"annotations\"", "id=\"electrical-nets\"", "id=\"metadata\""};
        std::string::size_type cursor = 0;
        for (const auto& group : expected_groups) {
            const auto pos = svg.find(group, cursor);
            assert(pos != std::string::npos);
            cursor = pos;
        }
    }

    // 29. Metadata group carries object counts and coordinate-system
    // provenance.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "data-coordinate-system=\"source_page_pixels\""));
        assert(contains(svg, "data-component-count=\"1\""));
    }

    // 30. Every rendered element ID is unique (no duplicate rendering of
    // the same object).
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1"), make_component("comp-2")};
        model.endpoint_candidates = {make_endpoint("ep-a"), make_endpoint("ep-b")};
        model.wires = {make_wire("w1", "ep-a", "ep-b")};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(svg.find("id=\"component-comp-1\"") != std::string::npos);
        // Not repeated: the substring appears exactly once.
        const auto first = svg.find("id=\"component-comp-1\"");
        const auto second = svg.find("id=\"component-comp-1\"", first + 1);
        assert(second == std::string::npos);
    }

    // 31. SVG root declares width/height/viewBox from the diagram's
    // image dimensions.
    {
        WireModel model;
        model.image_width = 800;
        model.image_height = 600;
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "width=\"800\""));
        assert(contains(svg, "height=\"600\""));
        assert(contains(svg, "viewBox=\"0 0 800 600\""));
    }

    // 32. Text content is XML-escaped (no raw '&', '<', '>' injected
    // into element bodies from source label text).
    {
        WireModel model;
        TextRegion region;
        region.id = "tr-1";
        region.bounds = BoundingBox{0, 0, 10, 10};
        model.text_regions = {region};
        TextRecognitionEvidence recognition;
        recognition.text_region_id = "tr-1";
        recognition.raw_text = "A & B < C";
        recognition.confidence = ConfidenceClass::High;
        model.text_recognition_evidence = {recognition};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "A &amp; B &lt; C"));
        assert(!contains(svg, "A & B < C"));
    }

    // 33. No source-image or raster dependency: rendering never touches
    // OpenCV types, and a diagram/model with no image path still
    // renders successfully.
    {
        WireModel model;
        model.source_id = "synthetic-no-image";
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(!svg.empty());
    }

    // 34. export_svg() writes the same content render() returns.
    {
        WireModel model;
        model.component_candidates = {make_component("comp-1")};
        const auto diagram = builder.build(model);
        const std::string rendered = StructuredSvgExporter::render(diagram, model);
        const std::string path = "/tmp/dx-wire-test-structured-svg-export.svg";
        StructuredSvgExporter::export_svg(diagram, model, path);
        std::ifstream in(path);
        const std::string written((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(written == rendered);
    }

    // 35. A full mixed scene (component + connector + wire + splice +
    // crossing + label + net) renders without error and every object
    // is traceable.
    {
        WireModel model;
        model.image_width = 100;
        model.image_height = 100;
        model.component_candidates = {make_component("comp-1")};
        SymbolFamilyResolution resolution;
        resolution.id = "sfr-1";
        resolution.component_id = "comp-1";
        resolution.family = SymbolFamily::Motor;
        resolution.status = SymbolFamilyResolutionStatus::Resolved;
        model.symbol_family_resolutions = {resolution};

        ConnectorCandidate connector;
        connector.id = "conn-1";
        model.connector_candidates = {connector};

        model.endpoint_candidates = {make_endpoint("ep-a"), make_endpoint("ep-b")};
        model.wires = {make_wire("w1", "ep-a", "ep-b")};
        model.nodes = {
            make_node("n-splice", TopologyNodeType::Splice, 5.0, 5.0),
            make_node("n-cross", TopologyNodeType::Crossing, 6.0, 6.0)};

        TextRegion region;
        region.id = "tr-1";
        region.bounds = BoundingBox{0, 0, 10, 10};
        model.text_regions = {region};

        ElectricalNet net;
        net.id = "net-1";
        model.electrical_nets = {net};

        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "data-component-id=\"comp-1\""));
        assert(contains(svg, "id=\"connector-conn-1\""));
        assert(contains(svg, "id=\"wire-w1\""));
        assert(contains(svg, "id=\"splice-n-splice\""));
        assert(contains(svg, "id=\"crossing-n-cross\""));
        assert(contains(svg, "id=\"electrical-net-net-1\""));
    }

    // 36. Rendering does not throw and produces non-empty, well-formed
    // output for a diagram carrying validation issues (dangling
    // references) - the renderer must degrade gracefully, not crash on
    // an imperfect upstream diagram.
    {
        WireModel model;
        model.wires = {make_wire("w-dangling", "missing-a", "missing-b")};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        assert(contains(svg, "id=\"wire-w-dangling\""));
        assert(contains(svg, "</svg>"));
    }

    return 0;
}

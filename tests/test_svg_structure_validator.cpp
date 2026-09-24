#include "eke_dx_wire/export/svg_structure_validator.hpp"
#include "eke_dx_wire/export/structured_svg_exporter.hpp"
#include "eke_dx_wire/diagram/engineering_diagram_builder.hpp"

#include <cassert>
#include <algorithm>

using namespace eke::dx::wire;

namespace {

bool has_issue(const SvgValidationReport& report, const std::string& code) {
    return std::any_of(
        report.issues.begin(), report.issues.end(),
        [&](const SvgValidationIssue& issue) { return issue.code == code; });
}

} // namespace

int main() {
    EngineeringDiagramBuilder builder;

    // 1. A real renderer-produced empty diagram validates clean.
    {
        WireModel model;
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        const auto report = SvgStructureValidator::validate(svg, diagram, model);
        assert(report.ok());
    }

    // 2. A real mixed-scene diagram validates clean (no dangling
    // references, no duplicate ids, no guessed symbol families).
    {
        WireModel model;
        model.component_candidates = {ComponentCandidate{"comp-1", ComponentCandidateKind::CircularSymbol, {}, BoundingBox{0, 0, 20, 20}, ConfidenceClass::High, {}}};
        SymbolFamilyResolution resolution;
        resolution.id = "sfr-1";
        resolution.component_id = "comp-1";
        resolution.family = SymbolFamily::Lamp;
        resolution.status = SymbolFamilyResolutionStatus::Resolved;
        model.symbol_family_resolutions = {resolution};
        EndpointCandidate a;
        a.id = "ep-a";
        EndpointCandidate b;
        b.id = "ep-b";
        model.endpoint_candidates = {a, b};
        Wire w;
        w.id = "w1";
        w.start_endpoint = "ep-a";
        w.end_endpoint = "ep-b";
        model.wires = {w};
        const auto diagram = builder.build(model);
        const std::string svg = StructuredSvgExporter::render(diagram, model);
        const auto report = SvgStructureValidator::validate(svg, diagram, model);
        assert(report.ok());
        assert(report.element_id_count > 0);
    }

    // 3. A missing <svg> root is caught.
    {
        WireModel model;
        const auto diagram = builder.build(model);
        const auto report = SvgStructureValidator::validate("<g></g>", diagram, model);
        assert(has_issue(report, "SVG-ROOT-MISSING"));
    }

    // 4. A raster <image> fallback is caught.
    {
        WireModel model;
        const auto diagram = builder.build(model);
        const std::string svg = "<svg><image href=\"x.png\"/></svg>";
        const auto report = SvgStructureValidator::validate(svg, diagram, model);
        assert(has_issue(report, "SVG-RASTER-FALLBACK"));
    }

    // 5. A duplicate element id is caught.
    {
        WireModel model;
        const auto diagram = builder.build(model);
        const std::string svg = "<svg><g id=\"a\"/><g id=\"a\"/></svg>";
        const auto report = SvgStructureValidator::validate(svg, diagram, model);
        assert(has_issue(report, "SVG-DUPLICATE-ELEMENT-ID"));
    }

    // 6. A dangling data-component-id reference (no such component in
    // the diagram) is caught.
    {
        WireModel model;
        const auto diagram = builder.build(model);
        const std::string svg = "<svg><g id=\"c1\" data-component-id=\"ghost\"/></svg>";
        const auto report = SvgStructureValidator::validate(svg, diagram, model);
        assert(has_issue(report, "SVG-DANGLING-REFERENCE"));
    }

    // 7. A dangling wire endpoint reference is caught.
    {
        WireModel model;
        const auto diagram = builder.build(model);
        const std::string svg = "<svg><g id=\"w1\" data-start-endpoint=\"ghost\"/></svg>";
        const auto report = SvgStructureValidator::validate(svg, diagram, model);
        assert(has_issue(report, "SVG-DANGLING-REFERENCE"));
    }

    // 8. A resolved symbol family reference is never flagged.
    {
        WireModel model;
        const auto diagram = builder.build(model);
        const std::string svg =
            "<svg><g class=\"symbol\" data-symbol-family=\"lamp\" "
            "data-symbol-family-status=\"resolved\"></g></svg>";
        const auto report = SvgStructureValidator::validate(svg, diagram, model);
        assert(!has_issue(report, "SVG-UNGUESSED-SYMBOL-FAMILY-VIOLATION"));
    }

    // 9. A symbol-family value rendered for an unresolved status is
    // flagged as a guessing violation (should never happen from the real
    // exporter, but the validator must catch it if it did).
    {
        WireModel model;
        const auto diagram = builder.build(model);
        const std::string svg =
            "<svg><g class=\"symbol\" data-symbol-family=\"lamp\" "
            "data-symbol-family-status=\"unresolved\"></g></svg>";
        const auto report = SvgStructureValidator::validate(svg, diagram, model);
        assert(has_issue(report, "SVG-UNGUESSED-SYMBOL-FAMILY-VIOLATION"));
    }

    // 10. A well-formed document with a valid closing tag reports no
    // SVG-ROOT-UNCLOSED issue.
    {
        WireModel model;
        const auto diagram = builder.build(model);
        const auto report = SvgStructureValidator::validate("<svg></svg>", diagram, model);
        assert(!has_issue(report, "SVG-ROOT-UNCLOSED"));
    }

    return 0;
}

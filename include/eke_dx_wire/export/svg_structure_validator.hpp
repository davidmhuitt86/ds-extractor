#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/diagram/engineering_diagram.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

/**
 * AP-WIRE-027: a lightweight, string/attribute-based structural validator
 * for StructuredSvgExporter output. This deliberately does NOT pull in a
 * new XML/DOM library - the SVG grammar this exporter produces is simple
 * enough (attribute-quoted, one element per emitted line) that regex-based
 * scanning is sufficient to check the acceptance criteria the AP-WIRE-027
 * spec requires: a valid SVG root, no raster fallback, unique element ids,
 * every data-*-id reference resolving to a real EngineeringDiagram/
 * WireModel object, and no symbol-family glyph rendered for an unresolved
 * or conflicted SymbolFamilyResolution.
 *
 * This is presentation-output validation only - it never re-derives or
 * second-guesses an engineering fact; it only checks that the renderer's
 * OWN output is internally and referentially consistent.
 */
struct SvgValidationIssue {
    std::string code;
    std::string detail;
};

struct SvgValidationReport {
    std::size_t element_id_count = 0;
    std::vector<SvgValidationIssue> issues;

    [[nodiscard]] bool ok() const { return issues.empty(); }
};

class SvgStructureValidator {
public:
    [[nodiscard]] static SvgValidationReport validate(
        const std::string& svg,
        const EngineeringDiagram& diagram,
        const WireModel& model);
};

} // namespace eke::dx::wire

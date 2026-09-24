#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/diagram/engineering_diagram.hpp"

#include <string>

namespace eke::dx::wire {

/**
 * AP-WIRE-027: the structured engineering SVG rendering boundary. This
 * is a RENDERER, not an extractor - it consumes an already-built
 * EngineeringDiagram (plus the WireModel it references, for geometry and
 * resolved-object detail) and produces real structured SVG markup
 * (<g>/<rect>/<circle>/<line>/<path>/<text>), never a raster <image>
 * wrapper. It requires no source image, no OpenCV type, and performs no
 * recognition of its own: every rendering decision is driven by an
 * already-established engineering fact (a resolved/unresolved/conflicted
 * status), never by re-inspecting geometry to guess one.
 */
class StructuredSvgExporter {
public:
    static void export_svg(
        const EngineeringDiagram& diagram,
        const WireModel& model,
        const std::string& output_path);

    // Exposed for testing/validation without touching the filesystem.
    [[nodiscard]] static std::string render(
        const EngineeringDiagram& diagram,
        const WireModel& model);
};

} // namespace eke::dx::wire

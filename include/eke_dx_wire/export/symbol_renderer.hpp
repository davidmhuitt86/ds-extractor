#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>

namespace eke::dx::wire {

/**
 * AP-WIRE-027: renders a canonical presentation glyph for an already-
 * resolved SymbolFamily. This is a pure PRESENTATION concern - it must
 * never decide *whether* a component belongs to a family (that is
 * AP-WIRE-026A's SymbolFamilyRecognizer's job, upstream and already
 * done by the time a renderer runs). A SymbolRenderer receives only the
 * bounds to draw within; it has no access to - and must never need -
 * the source raster image, OCR text, or any recognition evidence.
 *
 * The returned fragment is local-coordinate SVG content (drawn as if the
 * component's bounds top-left were at the origin); the caller wraps it
 * in a <g transform="translate(x,y)"> using the component's actual
 * position, so the same glyph logic works regardless of where the
 * component sits on the page.
 */
class SymbolRenderer {
public:
    virtual ~SymbolRenderer() = default;
    [[nodiscard]] virtual std::string render(const BoundingBox& bounds) const = 0;
};

// Returns the canonical renderer for a Resolved SymbolFamily. Never
// returns null; SymbolFamily::Unknown (and any value not covered by a
// specific renderer) returns the generic/unresolved renderer - callers
// must still gate on SymbolFamilyResolutionStatus themselves (see
// StructuredSvgExporter) rather than relying on this function to decide
// resolved-vs-unresolved presentation.
[[nodiscard]] const SymbolRenderer& symbol_renderer_for(SymbolFamily family);

// The explicit generic presentation for Unresolved status - visually
// distinguishable from every resolved glyph (dashed outline + "?").
[[nodiscard]] const SymbolRenderer& unresolved_symbol_renderer();

// The explicit generic presentation for Conflicted status - visually
// distinguishable from both a resolved glyph and the unresolved
// placeholder (dashed outline + "!").
[[nodiscard]] const SymbolRenderer& conflicted_symbol_renderer();

} // namespace eke::dx::wire

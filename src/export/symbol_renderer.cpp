#include "eke_dx_wire/export/symbol_renderer.hpp"

#include <algorithm>
#include <sstream>

namespace eke::dx::wire {
namespace {

// All glyphs are drawn inside a local w x h box (the component's own
// bounds) so they scale with whatever size the extractor actually
// measured, rather than a fixed canonical size that would misrepresent
// the source geometry's extent.
struct Box {
    double w;
    double h;
    double cx;
    double cy;
    double r; // a reasonable "radius" for circular glyphs
};

Box box_for(const BoundingBox& bounds) {
    const double w = std::max(4.0, static_cast<double>(bounds.width));
    const double h = std::max(4.0, static_cast<double>(bounds.height));
    return Box{w, h, w / 2.0, h / 2.0, std::min(w, h) / 2.0 * 0.85};
}

class GroundRenderer final : public SymbolRenderer {
public:
    std::string render(const BoundingBox& bounds) const override {
        const Box b = box_for(bounds);
        std::ostringstream out;
        // Vertical stem to three horizontal bars of decreasing width -
        // the standard chassis-ground glyph, matching what
        // ShapeDetector's ground-bar detector actually looks for.
        const double stem_bottom = b.h * 0.45;
        out << "<line x1=\"" << b.cx << "\" y1=\"0\" x2=\"" << b.cx
            << "\" y2=\"" << stem_bottom << "\" stroke=\"currentColor\" stroke-width=\"1.5\"/>";
        const double bar_y[3] = {b.h * 0.45, b.h * 0.65, b.h * 0.82};
        const double bar_w[3] = {b.w * 0.8, b.w * 0.5, b.w * 0.22};
        for (int i = 0; i < 3; ++i) {
            out << "<line x1=\"" << (b.cx - bar_w[i] / 2.0) << "\" y1=\"" << bar_y[i]
                << "\" x2=\"" << (b.cx + bar_w[i] / 2.0) << "\" y2=\"" << bar_y[i]
                << "\" stroke=\"currentColor\" stroke-width=\"1.5\"/>";
        }
        return out.str();
    }
};

class LampRenderer final : public SymbolRenderer {
public:
    std::string render(const BoundingBox& bounds) const override {
        const Box b = box_for(bounds);
        std::ostringstream out;
        out << "<circle cx=\"" << b.cx << "\" cy=\"" << b.cy << "\" r=\"" << b.r
            << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.5\"/>";
        // Filament cross (X) inside the circle - conventional lamp glyph.
        const double d = b.r * 0.7;
        out << "<line x1=\"" << (b.cx - d) << "\" y1=\"" << (b.cy - d) << "\" x2=\""
            << (b.cx + d) << "\" y2=\"" << (b.cy + d) << "\" stroke=\"currentColor\" stroke-width=\"1\"/>";
        out << "<line x1=\"" << (b.cx - d) << "\" y1=\"" << (b.cy + d) << "\" x2=\""
            << (b.cx + d) << "\" y2=\"" << (b.cy - d) << "\" stroke=\"currentColor\" stroke-width=\"1\"/>";
        return out.str();
    }
};

class SwitchRenderer final : public SymbolRenderer {
public:
    std::string render(const BoundingBox& bounds) const override {
        const Box b = box_for(bounds);
        std::ostringstream out;
        // Two contact dots with an open lever between them.
        out << "<circle cx=\"" << (b.w * 0.15) << "\" cy=\"" << b.cy << "\" r=\"1.5\" fill=\"currentColor\"/>";
        out << "<circle cx=\"" << (b.w * 0.85) << "\" cy=\"" << b.cy << "\" r=\"1.5\" fill=\"currentColor\"/>";
        out << "<line x1=\"" << (b.w * 0.15) << "\" y1=\"" << b.cy << "\" x2=\""
            << (b.w * 0.75) << "\" y2=\"" << (b.cy - b.h * 0.35)
            << "\" stroke=\"currentColor\" stroke-width=\"1.5\"/>";
        return out.str();
    }
};

class RelayRenderer final : public SymbolRenderer {
public:
    std::string render(const BoundingBox& bounds) const override {
        const Box b = box_for(bounds);
        std::ostringstream out;
        out << "<rect x=\"" << (b.w * 0.05) << "\" y=\"" << (b.h * 0.1) << "\" width=\""
            << (b.w * 0.9) << "\" height=\"" << (b.h * 0.8)
            << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.5\"/>";
        // Coil zigzag inside.
        const double top = b.h * 0.3;
        const double bottom = b.h * 0.7;
        const int teeth = 4;
        out << "<polyline points=\"";
        for (int i = 0; i <= teeth; ++i) {
            const double x = b.w * 0.2 + (b.w * 0.6) * (static_cast<double>(i) / teeth);
            const double y = (i % 2 == 0) ? top : bottom;
            out << x << "," << y << (i == teeth ? "" : " ");
        }
        out << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1\"/>";
        return out.str();
    }
};

class MotorRenderer final : public SymbolRenderer {
public:
    std::string render(const BoundingBox& bounds) const override {
        const Box b = box_for(bounds);
        std::ostringstream out;
        out << "<circle cx=\"" << b.cx << "\" cy=\"" << b.cy << "\" r=\"" << b.r
            << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.5\"/>";
        out << "<text x=\"" << b.cx << "\" y=\"" << (b.cy + b.r * 0.35)
            << "\" font-size=\"" << (b.r * 0.9)
            << "\" text-anchor=\"middle\" fill=\"currentColor\">M</text>";
        return out.str();
    }
};

class DiodeRenderer final : public SymbolRenderer {
public:
    std::string render(const BoundingBox& bounds) const override {
        const Box b = box_for(bounds);
        std::ostringstream out;
        const double left = b.w * 0.2;
        const double right = b.w * 0.8;
        // Triangle pointing right, bar at the tip - standard diode glyph.
        out << "<polygon points=\"" << left << "," << (b.h * 0.2) << " " << left << ","
            << (b.h * 0.8) << " " << right << "," << b.cy
            << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.5\"/>";
        out << "<line x1=\"" << right << "\" y1=\"" << (b.h * 0.15) << "\" x2=\"" << right
            << "\" y2=\"" << (b.h * 0.85) << "\" stroke=\"currentColor\" stroke-width=\"1.5\"/>";
        return out.str();
    }
};

class AlternatorRenderer final : public SymbolRenderer {
public:
    std::string render(const BoundingBox& bounds) const override {
        const Box b = box_for(bounds);
        std::ostringstream out;
        out << "<circle cx=\"" << b.cx << "\" cy=\"" << b.cy << "\" r=\"" << b.r
            << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.5\"/>";
        // Simple sine-like squiggle to suggest AC winding output.
        std::ostringstream d;
        d << "M " << (b.cx - b.r * 0.6) << " " << b.cy
          << " Q " << (b.cx - b.r * 0.2) << " " << (b.cy - b.r * 0.6) << " "
          << b.cx << " " << b.cy
          << " Q " << (b.cx + b.r * 0.2) << " " << (b.cy + b.r * 0.6) << " "
          << (b.cx + b.r * 0.6) << " " << b.cy;
        out << "<path d=\"" << d.str() << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1\"/>";
        return out.str();
    }
};

class BatteryRenderer final : public SymbolRenderer {
public:
    std::string render(const BoundingBox& bounds) const override {
        const Box b = box_for(bounds);
        std::ostringstream out;
        // Two unequal parallel bars - the standard cell glyph.
        out << "<line x1=\"" << (b.w * 0.35) << "\" y1=\"" << (b.h * 0.2) << "\" x2=\""
            << (b.w * 0.35) << "\" y2=\"" << (b.h * 0.8)
            << "\" stroke=\"currentColor\" stroke-width=\"2.5\"/>";
        out << "<line x1=\"" << (b.w * 0.65) << "\" y1=\"" << (b.h * 0.35) << "\" x2=\""
            << (b.w * 0.65) << "\" y2=\"" << (b.h * 0.65)
            << "\" stroke=\"currentColor\" stroke-width=\"1\"/>";
        return out.str();
    }
};

class SolenoidRenderer final : public SymbolRenderer {
public:
    std::string render(const BoundingBox& bounds) const override {
        const Box b = box_for(bounds);
        std::ostringstream out;
        out << "<rect x=\"" << (b.w * 0.1) << "\" y=\"" << (b.h * 0.2) << "\" width=\""
            << (b.w * 0.8) << "\" height=\"" << (b.h * 0.6)
            << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.5\"/>";
        for (int i = 0; i < 3; ++i) {
            const double x = b.w * 0.25 + i * b.w * 0.25;
            out << "<line x1=\"" << x << "\" y1=\"" << (b.h * 0.2) << "\" x2=\"" << x
                << "\" y2=\"" << (b.h * 0.8) << "\" stroke=\"currentColor\" stroke-width=\"1\"/>";
        }
        return out.str();
    }
};

class CoilRenderer final : public SymbolRenderer {
public:
    std::string render(const BoundingBox& bounds) const override {
        const Box b = box_for(bounds);
        std::ostringstream out;
        const int loops = 3;
        std::ostringstream d;
        d << "M 0 " << b.cy;
        for (int i = 0; i < loops; ++i) {
            const double x0 = b.w * (static_cast<double>(i) / loops);
            const double x1 = b.w * (static_cast<double>(i + 1) / loops);
            const double mid = (x0 + x1) / 2.0;
            d << " Q " << mid << " 0 " << x1 << " " << b.cy;
        }
        out << "<path d=\"" << d.str() << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1.5\"/>";
        return out.str();
    }
};

// The explicit "no guess" presentation: a dashed generic outline plus a
// marker glyph, visually distinguishable from every resolved family
// glyph above (none of which use a dashed stroke).
class UnresolvedRenderer final : public SymbolRenderer {
public:
    std::string render(const BoundingBox& bounds) const override {
        const Box b = box_for(bounds);
        std::ostringstream out;
        out << "<rect x=\"1\" y=\"1\" width=\"" << (b.w - 2) << "\" height=\"" << (b.h - 2)
            << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1\" "
               "stroke-dasharray=\"3,2\"/>";
        out << "<text x=\"" << b.cx << "\" y=\"" << (b.cy + b.h * 0.15)
            << "\" font-size=\"" << (std::min(b.w, b.h) * 0.5)
            << "\" text-anchor=\"middle\" fill=\"currentColor\">?</text>";
        return out.str();
    }
};

// The explicit conflicted presentation: dashed outline (like Unresolved,
// so both are clearly "not a confident resolved glyph") but with a
// distinct "!" marker so Unresolved and Conflicted remain visually
// distinguishable from each other in SVG metadata/rendering alike.
class ConflictedRenderer final : public SymbolRenderer {
public:
    std::string render(const BoundingBox& bounds) const override {
        const Box b = box_for(bounds);
        std::ostringstream out;
        out << "<rect x=\"1\" y=\"1\" width=\"" << (b.w - 2) << "\" height=\"" << (b.h - 2)
            << "\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"1\" "
               "stroke-dasharray=\"1,2\"/>";
        out << "<text x=\"" << b.cx << "\" y=\"" << (b.cy + b.h * 0.15)
            << "\" font-size=\"" << (std::min(b.w, b.h) * 0.5)
            << "\" text-anchor=\"middle\" fill=\"currentColor\">!</text>";
        return out.str();
    }
};

} // namespace

const SymbolRenderer& symbol_renderer_for(SymbolFamily family) {
    static const GroundRenderer ground;
    static const LampRenderer lamp;
    static const SwitchRenderer switch_renderer;
    static const RelayRenderer relay;
    static const MotorRenderer motor;
    static const DiodeRenderer diode;
    static const AlternatorRenderer alternator;
    static const BatteryRenderer battery;
    static const SolenoidRenderer solenoid;
    static const CoilRenderer coil;
    static const UnresolvedRenderer fallback;

    switch (family) {
    case SymbolFamily::Ground: return ground;
    case SymbolFamily::Lamp: return lamp;
    case SymbolFamily::Switch: return switch_renderer;
    case SymbolFamily::Relay: return relay;
    case SymbolFamily::Motor: return motor;
    case SymbolFamily::Diode: return diode;
    case SymbolFamily::Alternator: return alternator;
    case SymbolFamily::Battery: return battery;
    case SymbolFamily::Solenoid: return solenoid;
    case SymbolFamily::Coil: return coil;
    case SymbolFamily::Unknown: return fallback;
    }
    return fallback;
}

const SymbolRenderer& unresolved_symbol_renderer() {
    static const UnresolvedRenderer renderer;
    return renderer;
}

const SymbolRenderer& conflicted_symbol_renderer() {
    static const ConflictedRenderer renderer;
    return renderer;
}

} // namespace eke::dx::wire

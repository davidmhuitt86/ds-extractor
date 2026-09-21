#include "eke_dx_wire/export/svg_exporter.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace eke::dx::wire {

void SvgExporter::export_segments(
    const WireModel& model,
    const std::string& output_path) {

    std::ofstream out(output_path);
    if (!out) {
        throw std::runtime_error("Unable to create SVG: " + output_path);
    }

    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        << "width=\"" << model.image_width << "\" "
        << "height=\"" << model.image_height << "\" "
        << "viewBox=\"0 0 " << model.image_width << " "
        << model.image_height << "\">\n";

    out << "  <g id=\"conductor-layer\" fill=\"none\" stroke=\"black\" "
        << "stroke-linecap=\"round\" stroke-linejoin=\"round\">\n";

    out << std::setprecision(4);

    for (const auto& segment : model.conductor_segments) {
        out << "    <line id=\"" << segment.id << "\" "
            << "x1=\"" << segment.geometry.a.x << "\" "
            << "y1=\"" << segment.geometry.a.y << "\" "
            << "x2=\"" << segment.geometry.b.x << "\" "
            << "y2=\"" << segment.geometry.b.y << "\" "
            << "stroke-width=\"" << std::max(1.0, segment.thickness_px)
            << "\" data-object-type=\"conductor-segment\"/>\n";
    }

    out << "  </g>\n";
    out << "</svg>\n";
}

} // namespace eke::dx::wire

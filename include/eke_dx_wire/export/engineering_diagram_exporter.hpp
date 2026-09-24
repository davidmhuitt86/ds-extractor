#pragma once

#include "eke_dx_wire/diagram/engineering_diagram.hpp"

#include <string>

namespace eke::dx::wire {

class EngineeringDiagramExporter {
public:
    static void export_json(
        const EngineeringDiagram& diagram,
        const std::string& output_path);
};

} // namespace eke::dx::wire

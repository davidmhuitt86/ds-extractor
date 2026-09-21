#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>

namespace eke::dx::wire {

class SvgExporter {
public:
    static void export_segments(
        const WireModel& model,
        const std::string& output_path);
};

} // namespace eke::dx::wire

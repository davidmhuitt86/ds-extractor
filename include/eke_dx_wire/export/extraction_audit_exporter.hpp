#pragma once

#include "eke_dx_wire/core/model.hpp"
#include <filesystem>

namespace eke::dx::wire {
class ExtractionAuditExporter {
public:
    static void export_json(const WireModel& model,
                            const std::filesystem::path& output_path);
};
} // namespace eke::dx::wire

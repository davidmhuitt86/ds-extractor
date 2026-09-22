#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <cstddef>

namespace eke::dx::wire {

[[nodiscard]] ExtractionAudit build_extraction_audit(
    const WireModel& model,
    std::size_t gaps_bridged = 0);

} // namespace eke::dx::wire

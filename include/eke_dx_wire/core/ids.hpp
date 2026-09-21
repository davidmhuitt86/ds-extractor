#pragma once

#include <string>
#include <string_view>

namespace eke::dx::wire {

[[nodiscard]] std::string stable_id(
    std::string_view namespace_name,
    std::string_view canonical_geometry);

} // namespace eke::dx::wire

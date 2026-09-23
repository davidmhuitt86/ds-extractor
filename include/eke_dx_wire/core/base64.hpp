#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace eke::dx::wire {

[[nodiscard]] std::string base64_encode(const std::vector<uint8_t>& data);

} // namespace eke::dx::wire

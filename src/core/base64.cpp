#include "eke_dx_wire/core/base64.hpp"

namespace eke::dx::wire {

std::string base64_encode(const std::vector<uint8_t>& data) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";

    std::string result;
    result.reserve(((data.size() + 2) / 3) * 4);

    std::size_t i = 0;
    while (i + 3 <= data.size()) {
        const uint32_t chunk =
            (static_cast<uint32_t>(data[i]) << 16) |
            (static_cast<uint32_t>(data[i + 1]) << 8) |
            static_cast<uint32_t>(data[i + 2]);
        result += table[(chunk >> 18) & 0x3F];
        result += table[(chunk >> 12) & 0x3F];
        result += table[(chunk >> 6) & 0x3F];
        result += table[chunk & 0x3F];
        i += 3;
    }

    const std::size_t remaining = data.size() - i;
    if (remaining == 1) {
        const uint32_t chunk = static_cast<uint32_t>(data[i]) << 16;
        result += table[(chunk >> 18) & 0x3F];
        result += table[(chunk >> 12) & 0x3F];
        result += "==";
    } else if (remaining == 2) {
        const uint32_t chunk =
            (static_cast<uint32_t>(data[i]) << 16) |
            (static_cast<uint32_t>(data[i + 1]) << 8);
        result += table[(chunk >> 18) & 0x3F];
        result += table[(chunk >> 12) & 0x3F];
        result += table[(chunk >> 6) & 0x3F];
        result += "=";
    }

    return result;
}

} // namespace eke::dx::wire

#include "eke_dx_wire/core/ids.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>

namespace eke::dx::wire {

// FNV-1a is intentionally used here only as a dependency-free deterministic
// bootstrap identifier. The production specification calls for a stronger
// content-addressed identifier implementation; that replacement belongs in
// the infrastructure/serialization layer without changing domain contracts.
std::string stable_id(
    std::string_view namespace_name,
    std::string_view canonical_geometry) {

    constexpr std::uint64_t offset = 14695981039346656037ull;
    constexpr std::uint64_t prime = 1099511628211ull;

    std::uint64_t hash = offset;

    for (const char c : namespace_name) {
        hash ^= static_cast<unsigned char>(c);
        hash *= prime;
    }

    hash ^= 0xff;
    hash *= prime;

    for (const char c : canonical_geometry) {
        hash ^= static_cast<unsigned char>(c);
        hash *= prime;
    }

    std::ostringstream out;
    out << namespace_name << "-" << std::hex
        << std::setw(16) << std::setfill('0') << hash;

    return out.str();
}

} // namespace eke::dx::wire

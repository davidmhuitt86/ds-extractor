#pragma once

namespace eke::dx::wire {

// A short conductor overlap may be treated as a connector termination only
// when the connector housing boundary was independently reconstructed from
// a complete rectangular frame. Without that evidence, preserve the legacy
// minimum-overlap gate to avoid promoting unrelated component terminals as
// connector pins.
[[nodiscard]] constexpr bool connector_observation_passes_overlap_gate(
    double overlap_px,
    bool termination_contact,
    bool boundary_reconstructed_from_rectangular_strokes,
    double minimum_overlap_px = 3.0) noexcept {
    return overlap_px >= minimum_overlap_px ||
           (termination_contact &&
            boundary_reconstructed_from_rectangular_strokes);
}

} // namespace eke::dx::wire

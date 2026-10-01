#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

struct CircleContextConfig {
    // A circle whose outward straight-line probe contains a run at or beyond
    // this fraction of the adaptive probe distance is treated as line/grid
    // geometry rather than a self-contained circular symbol.
    double max_run_fraction = 0.75;

    // A surviving circle must have an electrical-conductor approach that
    // either intersects its candidate bounds or terminates within this
    // distance of the bounds. This is intentionally measured against
    // normalized conductor geometry, not terminal recognition.
    double max_conductor_endpoint_distance_px = 5.0;
};

class CircleContextClassifier {
public:
    explicit CircleContextClassifier(CircleContextConfig config = {});

    [[nodiscard]] std::vector<ComponentCandidate> classify(
        const std::vector<ComponentCandidate>& candidates,
        const std::vector<ConductorSegment>& conductors) const;

private:
    CircleContextConfig config_;
};

} // namespace eke::dx::wire

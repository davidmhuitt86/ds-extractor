#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

struct GeometryNormalizationConfig {
    double collinear_tolerance = 0.75;
    double endpoint_tolerance = 0.75;
    double duplicate_tolerance = 0.75;
};

class ConductorNormalizer {
public:
    explicit ConductorNormalizer(GeometryNormalizationConfig config = {});

    [[nodiscard]] std::vector<ConductorSegment> normalize(
        const std::vector<ConductorSegment>& raw_segments) const;

private:
    GeometryNormalizationConfig config_;
};

} // namespace eke::dx::wire

#include "eke_dx_wire/image/conductor_normalizer.hpp"

#include <cassert>
#include <cmath>
#include <vector>

using namespace eke::dx::wire;

static ConductorSegment segment(
    const char* id, Point2D a, Point2D b) {
    ConductorSegment s;
    s.id = id;
    s.geometry = {a, b};
    s.provenance.source_id = "fixture";
    s.provenance.page = 0;
    s.provenance.source_region = {
        static_cast<int>(std::min(a.x, b.x)),
        static_cast<int>(std::min(a.y, b.y)),
        static_cast<int>(std::abs(b.x - a.x)) + 1,
        static_cast<int>(std::abs(b.y - a.y)) + 1};
    return s;
}

int main() {
    {
        const std::vector<ConductorSegment> raw{
            segment("a", {0, 10}, {10, 10}),
            segment("b", {10, 10}, {20, 10}),
            segment("duplicate", {0, 10}, {10, 10}),
        };

        const auto normalized =
            ConductorNormalizer().normalize(raw);

        assert(normalized.size() == 1);
        assert(normalized.front().geometry.a.x == 0.0);
        assert(normalized.front().geometry.b.x == 20.0);
        assert(normalized.front().provenance.stage == "geometry.normalized");
    }

    {
        const std::vector<ConductorSegment> raw{
            segment("upper", {0, 10}, {20, 10}),
            segment("lower", {0, 20}, {20, 20}),
        };

        const auto normalized =
            ConductorNormalizer().normalize(raw);

        assert(normalized.size() == 2);
    }

    {
        const std::vector<ConductorSegment> raw{
            segment("left", {0, 10}, {10, 10}),
            segment("right", {12, 10}, {20, 10}),
        };

        const auto normalized =
            ConductorNormalizer().normalize(raw);

        // A two-pixel gap is intentionally not bridged.
        assert(normalized.size() == 2);
    }

    return 0;
}

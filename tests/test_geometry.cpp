#include "eke_dx_wire/core/geometry.hpp"

#include <cassert>
#include <cmath>

using namespace eke::dx::wire;

int main() {
    Segment2D horizontal{{0, 10}, {30, 10}};
    Segment2D vertical{{5, 0}, {5, 20}};

    assert(std::abs(horizontal.length() - 30.0) < 1e-9);
    assert(horizontal.is_horizontal());
    assert(!horizontal.is_vertical());

    assert(std::abs(vertical.length() - 20.0) < 1e-9);
    assert(vertical.is_vertical());
    assert(!vertical.is_horizontal());

    const auto mid = midpoint(horizontal.a, horizontal.b);
    const Point2D expected_midpoint{15, 10};
    assert(mid == expected_midpoint);
    return 0;
}

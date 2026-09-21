#include "eke_dx_wire/core/geometry.hpp"

#include <algorithm>

namespace eke::dx::wire {

double Segment2D::length() const noexcept {
    return distance(a, b);
}

bool Segment2D::is_horizontal(double tolerance) const noexcept {
    return std::abs(a.y - b.y) <= tolerance;
}

bool Segment2D::is_vertical(double tolerance) const noexcept {
    return std::abs(a.x - b.x) <= tolerance;
}

double distance(Point2D a, Point2D b) noexcept {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

Point2D midpoint(Point2D a, Point2D b) noexcept {
    return {(a.x + b.x) * 0.5, (a.y + b.y) * 0.5};
}

} // namespace eke::dx::wire

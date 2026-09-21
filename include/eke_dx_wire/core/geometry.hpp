#pragma once

#include <cmath>
#include <cstdint>
#include <string>

namespace eke::dx::wire {

struct Point2D {
    double x {};
    double y {};

    [[nodiscard]] bool operator==(const Point2D&) const = default;
};

struct Segment2D {
    Point2D a {};
    Point2D b {};

    [[nodiscard]] double length() const noexcept;
    [[nodiscard]] bool is_horizontal(double tolerance = 0.5) const noexcept;
    [[nodiscard]] bool is_vertical(double tolerance = 0.5) const noexcept;
};

struct BoundingBox {
    int x {};
    int y {};
    int width {};
    int height {};
};

[[nodiscard]] double distance(Point2D a, Point2D b) noexcept;
[[nodiscard]] Point2D midpoint(Point2D a, Point2D b) noexcept;

} // namespace eke::dx::wire

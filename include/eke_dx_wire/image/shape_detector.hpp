#pragma once

#include "eke_dx_wire/core/geometry.hpp"

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace eke::dx::wire {

enum class ShapeKind {
    Rectangle,
    Circle,
    ChassisGround
};

enum class ShapeRole {
    Primitive,
    Enclosure,
    Exclusion
};

struct ShapeRegion {
    std::string id;
    ShapeKind kind = ShapeKind::Rectangle;
    ShapeRole role = ShapeRole::Primitive;
    BoundingBox bounds {};
    double confidence = 0.0;
};

struct ShapeDetectionArtifacts {
    cv::Mat exclusion_mask;
    std::vector<ShapeRegion> regions;
};

struct ShapeDetectorConfig {
    int contour_threshold = 180;
    int contour_close_kernel = 3;

    // Candidate generation.
    double rectangle_epsilon = 0.04;
    double rectangle_fill_ratio = 0.55;
    double rectangle_min_area = 40.0;
    double rectangle_max_area_ratio = 0.20;
    int rectangle_min_width = 15;
    int rectangle_min_height = 12;
    double rectangle_max_interior_ink_density = 0.22;
    double rectangle_min_interior_ink_density = 0.005;
    int rectangle_min_interior_components = 1;
    int rectangle_max_interior_component_aspect = 8;
    double rectangle_min_border_ink_density = 0.08;
    double rectangle_min_perimeter_ratio = 0.55;
    double rectangle_max_perimeter_ratio = 1.60;

    // A geometric rectangle is not automatically a wire-exclusion region.
    // Small rectangles are retained as primitives so connector contacts,
    // switch cells, and similar graphics remain available to later stages.
    double rectangle_min_exclusion_area = 600.0;
    int rectangle_min_exclusion_width = 25;
    int rectangle_min_exclusion_height = 20;

    // Circular candidate validation.
    int circle_dp = 1;
    double circle_min_dist = 12.0;
    double circle_param1 = 100.0;
    double circle_param2 = 14.0;
    int circle_min_radius = 3;
    int circle_max_radius = 60;
    double circle_min_edge_support = 0.65;
    double circle_max_interior_ink_density = 0.30;
    double circle_min_circularity = 0.65;
    double circle_max_aspect_ratio = 1.35;
    double circle_min_area = 25.0;

    // Chassis-ground candidate validation.
    int ground_min_bar_length = 5;
    int ground_max_bar_length = 35;
    int ground_max_height = 30;
    int ground_min_bar_spacing = 3;
    int ground_max_bar_spacing = 12;
    double ground_width_ratio_tolerance = 0.20;
    int ground_stem_search_height = 14;
    int ground_min_exclusion_height = 12;
};

class ShapeDetector {
public:
    explicit ShapeDetector(ShapeDetectorConfig config = {});

    [[nodiscard]] ShapeDetectionArtifacts detect(
        const cv::Mat& normalized,
        const std::string& source_id,
        int page = 0) const;

private:
    ShapeDetectorConfig config_;
};

} // namespace eke::dx::wire

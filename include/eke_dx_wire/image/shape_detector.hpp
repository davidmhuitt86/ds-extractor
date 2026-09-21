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

struct ShapeRegion {
    std::string id;
    ShapeKind kind = ShapeKind::Rectangle;
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
    double rectangle_epsilon = 0.04;
    double rectangle_fill_ratio = 0.55;
    double rectangle_min_area = 40.0;
    double rectangle_max_area_ratio = 0.20;
    int circle_dp = 1;
    double circle_min_dist = 12.0;
    double circle_param1 = 100.0;
    double circle_param2 = 14.0;
    int circle_min_radius = 3;
    int circle_max_radius = 60;
    int ground_min_bar_length = 5;
    int ground_max_bar_length = 35;
    int ground_max_height = 30;
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

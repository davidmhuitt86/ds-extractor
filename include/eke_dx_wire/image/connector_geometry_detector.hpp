#pragma once

#include "eke_dx_wire/core/geometry.hpp"
#include "eke_dx_wire/image/shape_detector.hpp"

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace eke::dx::wire {

struct ConnectorGeometryDetectionArtifacts {
    std::vector<ShapeRegion> regions;
};

struct ConnectorGeometryDetectorConfig {
    int threshold = 180;
    int close_kernel = 3;
    double min_area = 80.0;
    double max_area_ratio = 0.01;
    int min_width = 12;
    int min_height = 10;
    int max_width = 90;
    int max_height = 70;
    double min_fill_ratio = 0.20;
    double max_fill_ratio = 0.98;
    double min_aspect_ratio = 0.25;
    double max_aspect_ratio = 5.0;
    double polygon_epsilon = 0.04;
    int max_polygon_vertices = 14;
    double min_pass_through_continuity = 0.65;
    int pass_through_probe = 12;
    int pass_through_band = 2;
};

class ConnectorGeometryDetector {
public:
    explicit ConnectorGeometryDetector(
        ConnectorGeometryDetectorConfig config = {});

    [[nodiscard]] ConnectorGeometryDetectionArtifacts detect(
        const cv::Mat& normalized,
        const std::string& source_id,
        int page = 0) const;

private:
    ConnectorGeometryDetectorConfig config_;
};

} // namespace eke::dx::wire

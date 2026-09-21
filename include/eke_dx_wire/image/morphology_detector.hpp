#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/image/shape_detector.hpp"

#include <opencv2/core.hpp>
#include <vector>

namespace eke::dx::wire {

struct MorphologyConfig {
    int horizontal_kernel_length = 25;
    int vertical_kernel_length = 25;
    int adaptive_block_size = 31;
    int adaptive_c = 7;
    int minimum_segment_length = 12;
    int minimum_component_area = 8;
};

struct DetectionArtifacts {
    cv::Mat binary;
    cv::Mat horizontal_mask;
    cv::Mat vertical_mask;
    ShapeDetectionArtifacts shapes;
    std::vector<ConductorSegment> conductor_segments;
};

class MorphologyWireDetector {
public:
    explicit MorphologyWireDetector(MorphologyConfig config = {});

    [[nodiscard]] DetectionArtifacts detect(
        const cv::Mat& normalized,
        const std::string& source_id,
        int page = 0,
        const cv::Mat& exclusion_mask = {}) const;

private:
    MorphologyConfig config_;
};

} // namespace eke::dx::wire

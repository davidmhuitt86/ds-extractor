#pragma once

#include "eke_dx_wire/core/geometry.hpp"

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace eke::dx::wire {

enum class TextRegionKind {
    Label,
    Annotation,
    WireLabel,
    Unknown
};

struct TextRegion {
    std::string id;
    TextRegionKind kind = TextRegionKind::Unknown;
    BoundingBox bounds {};
    double confidence = 0.0;
    bool excluded_from_wire_detection = false;
};

struct TextDetectionArtifacts {
    cv::Mat exclusion_mask;
    std::vector<TextRegion> regions;
};

struct TextDetectorConfig {
    int threshold = 180;
    int minimum_component_area = 2;
    int maximum_component_area = 500;
    int maximum_component_width = 40;
    int maximum_component_height = 40;
    int grouping_gap = 12;
    int grouping_vertical_tolerance = 6;
    int minimum_group_width = 8;
    int minimum_group_height = 3;
    int maximum_group_height = 40;
    double minimum_confidence = 0.45;
};

class TextRegionDetector {
public:
    explicit TextRegionDetector(TextDetectorConfig config = {});

    [[nodiscard]] TextDetectionArtifacts detect(
        const cv::Mat& normalized,
        const std::string& source_id,
        int page = 0) const;

private:
    TextDetectorConfig config_;
};

} // namespace eke::dx::wire

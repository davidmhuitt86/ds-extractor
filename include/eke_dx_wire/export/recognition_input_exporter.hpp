#pragma once

#include "eke_dx_wire/core/model.hpp"
#include <opencv2/core.hpp>
#include <string>

namespace eke::dx::wire {

class RecognitionInputExporter {
public:
    static void export_package(
        const WireModel& model,
        const cv::Mat& normalized_image,
        const std::string& original_image_path,
        const std::string& output_directory);
};

} // namespace eke::dx::wire

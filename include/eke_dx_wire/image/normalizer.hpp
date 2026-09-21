#pragma once

#include <opencv2/core.hpp>

namespace eke::dx::wire {

class ImageNormalizer {
public:
    [[nodiscard]] static cv::Mat normalize(const cv::Mat& source);
};

} // namespace eke::dx::wire

#pragma once

#include <opencv2/core.hpp>
#include <string>

namespace eke::dx::wire {

class ImageLoader {
public:
    [[nodiscard]] static cv::Mat load(const std::string& path);
};

} // namespace eke::dx::wire

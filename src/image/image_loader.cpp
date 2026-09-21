#include "eke_dx_wire/image/image_loader.hpp"

#include <opencv2/imgcodecs.hpp>
#include <stdexcept>

namespace eke::dx::wire {

cv::Mat ImageLoader::load(const std::string& path) {
    cv::Mat image = cv::imread(path, cv::IMREAD_UNCHANGED);

    if (image.empty()) {
        throw std::runtime_error("Unable to load image: " + path);
    }

    return image;
}

} // namespace eke::dx::wire

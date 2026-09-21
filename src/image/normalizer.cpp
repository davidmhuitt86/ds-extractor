#include "eke_dx_wire/image/normalizer.hpp"

#include <opencv2/imgproc.hpp>

namespace eke::dx::wire {

cv::Mat ImageNormalizer::normalize(const cv::Mat& source) {
    cv::Mat gray;

    if (source.channels() == 1) {
        gray = source.clone();
    } else if (source.channels() == 3) {
        cv::cvtColor(source, gray, cv::COLOR_BGR2GRAY);
    } else if (source.channels() == 4) {
        cv::cvtColor(source, gray, cv::COLOR_BGRA2GRAY);
    } else {
        throw std::runtime_error("Unsupported source channel count");
    }

    cv::Mat normalized;
    gray.convertTo(normalized, CV_8U);

    return normalized;
}

} // namespace eke::dx::wire

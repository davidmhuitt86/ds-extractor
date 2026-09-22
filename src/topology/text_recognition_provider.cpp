#include "eke_dx_wire/topology/text_recognition_provider.hpp"

namespace eke::dx::wire {

std::vector<TextRecognitionEvidence>
NullTextRecognitionProvider::recognize(
    const cv::Mat&,
    const std::vector<TextRegion>&,
    const std::string&,
    int) const {
    return {};
}

std::string NullTextRecognitionProvider::provider_id() const {
    return "none";
}

} // namespace eke::dx::wire

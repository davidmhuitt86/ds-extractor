#include "eke_dx_wire/topology/json_text_recognition_provider.hpp"
#include "eke_dx_wire/topology/recognition_observation_parser.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eke::dx::wire {

JsonTextRecognitionProvider::JsonTextRecognitionProvider(
    std::string observation_path)
    : observation_path_(std::move(observation_path)) {}

std::vector<TextRecognitionEvidence>
JsonTextRecognitionProvider::recognize(
    const cv::Mat&,
    const std::vector<TextRegion>&,
    const std::string&,
    int) const {

    std::ifstream in(observation_path_);
    if (!in) {
        throw std::runtime_error(
            "Unable to open recognition observation JSON: " +
            observation_path_);
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();

    return parse_recognition_observations(buffer.str(), provider_id());
}

std::string JsonTextRecognitionProvider::provider_id() const {
    return "json-observation";
}

} // namespace eke::dx::wire

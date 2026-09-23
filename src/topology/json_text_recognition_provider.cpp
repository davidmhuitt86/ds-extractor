#include "eke_dx_wire/topology/json_text_recognition_provider.hpp"

#include <opencv2/core.hpp>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eke::dx::wire {
namespace {

ConfidenceClass parse_confidence(const std::string& value) {
    if (value == "high") return ConfidenceClass::High;
    if (value == "medium") return ConfidenceClass::Medium;
    if (value == "low") return ConfidenceClass::Low;
    return ConfidenceClass::Unresolved;
}

} // namespace

JsonTextRecognitionProvider::JsonTextRecognitionProvider(
    std::string observation_path)
    : observation_path_(std::move(observation_path)) {}

std::vector<TextRecognitionEvidence>
JsonTextRecognitionProvider::recognize(
    const cv::Mat&,
    const std::vector<TextRegion>&,
    const std::string&,
    int) const {

    cv::FileStorage storage(
        observation_path_, cv::FileStorage::READ | cv::FileStorage::FORMAT_JSON);

    if (!storage.isOpened()) {
        throw std::runtime_error(
            "Unable to open recognition observation JSON: " +
            observation_path_);
    }

    const cv::FileNode observations = storage["observations"];
    if (observations.empty() || !observations.isSeq()) {
        throw std::runtime_error(
            "Recognition observation JSON must contain an 'observations' array");
    }

    std::vector<TextRecognitionEvidence> result;

    for (const auto& observation : observations) {
        if (!observation.isMap()) {
            continue;
        }

        std::string region_id;
        std::string raw_text;
        std::string confidence;

        observation["text_region_id"] >> region_id;
        observation["raw_text"] >> raw_text;
        observation["confidence"] >> confidence;

        if (region_id.empty() || raw_text.empty()) {
            continue;
        }

        TextRecognitionEvidence evidence;
        evidence.text_region_id = region_id;
        evidence.raw_text = raw_text;
        evidence.confidence = parse_confidence(confidence);
        evidence.provider = provider_id();

        if (evidence.confidence != ConfidenceClass::Unresolved) {
            result.push_back(std::move(evidence));
        }
    }

    return result;
}

std::string JsonTextRecognitionProvider::provider_id() const {
    return "json-observation";
}

} // namespace eke::dx::wire

#include "eke_dx_wire/topology/recognition_observation_parser.hpp"

#include <opencv2/core.hpp>

#include <stdexcept>

namespace eke::dx::wire {
namespace {

ConfidenceClass parse_confidence(const std::string& value) {
    if (value == "high") return ConfidenceClass::High;
    if (value == "medium") return ConfidenceClass::Medium;
    if (value == "low") return ConfidenceClass::Low;
    return ConfidenceClass::Unresolved;
}

} // namespace

std::vector<TextRecognitionEvidence> parse_recognition_observations(
    const std::string& json_text,
    const std::string& provider_id) {

    cv::FileStorage storage(
        json_text,
        cv::FileStorage::READ | cv::FileStorage::MEMORY | cv::FileStorage::FORMAT_JSON);

    if (!storage.isOpened()) {
        throw std::runtime_error(
            "Unable to parse recognition observation JSON");
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
        evidence.provider = provider_id;

        if (evidence.confidence != ConfidenceClass::Unresolved) {
            result.push_back(std::move(evidence));
        }
    }

    return result;
}

} // namespace eke::dx::wire

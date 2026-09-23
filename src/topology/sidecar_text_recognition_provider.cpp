#include "eke_dx_wire/topology/sidecar_text_recognition_provider.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace eke::dx::wire {
namespace {

ConfidenceClass parse_confidence(const std::string& value) {
    if (value == "high") return ConfidenceClass::High;
    if (value == "medium") return ConfidenceClass::Medium;
    if (value == "low") return ConfidenceClass::Low;
    return ConfidenceClass::Unresolved;
}

std::string trim(const std::string& value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

} // namespace

SidecarTextRecognitionProvider::SidecarTextRecognitionProvider(
    std::string sidecar_path)
    : sidecar_path_(std::move(sidecar_path)) {}

std::vector<TextRecognitionEvidence>
SidecarTextRecognitionProvider::recognize(
    const cv::Mat&,
    const std::vector<TextRegion>&,
    const std::string&,
    int) const {

    std::ifstream input(sidecar_path_);
    if (!input) {
        throw std::runtime_error(
            "Unable to open text recognition sidecar: " + sidecar_path_);
    }

    std::vector<TextRecognitionEvidence> result;
    std::string line;

    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') continue;

        std::istringstream fields(line);
        std::string region_id;
        std::string confidence;
        std::string raw_text;

        if (!std::getline(fields, region_id, '\t') ||
            !std::getline(fields, confidence, '\t') ||
            !std::getline(fields, raw_text)) {
            continue;
        }

        region_id = trim(region_id);
        confidence = trim(confidence);
        raw_text = trim(raw_text);

        if (region_id.empty() || raw_text.empty()) continue;

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

std::string SidecarTextRecognitionProvider::provider_id() const {
    return "sidecar";
}

} // namespace eke::dx::wire

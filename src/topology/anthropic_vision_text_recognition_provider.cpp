#include "eke_dx_wire/topology/anthropic_vision_text_recognition_provider.hpp"
#include "eke_dx_wire/core/base64.hpp"
#include "eke_dx_wire/topology/recognition_observation_parser.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace eke::dx::wire {
namespace {

std::string json_escape(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += ch; break;
        }
    }
    return result;
}

constexpr const char* kSystemInstructions =
    "You are reading text regions extracted from a wiring-diagram raster "
    "image. For each region listed in the user message, read the text at "
    "that location in the supplied image exactly as drawn. Do not invent "
    "characters that are not visually supported. If a region is illegible "
    "or contains no readable text, omit it rather than guessing. Preserve "
    "uncertainty using the confidence field. Respond with ONLY a single "
    "JSON document matching this exact schema, no prose, no markdown code "
    "fence:\n"
    "{\"observations\": [{\"text_region_id\": string, \"raw_text\": string, "
    "\"confidence\": \"high\"|\"medium\"|\"low\"|\"unresolved\", "
    "\"notes\": string}]}\n"
    "One observation per region id you could read. Neighboring geometry is "
    "not part of this task; only report what is visually present in the "
    "region.";

std::string extract_response_text(const std::string& response_body) {
    cv::FileStorage storage(
        response_body,
        cv::FileStorage::READ | cv::FileStorage::MEMORY | cv::FileStorage::FORMAT_JSON);

    if (!storage.isOpened()) {
        throw std::runtime_error(
            "Anthropic API response was not valid JSON: " + response_body);
    }

    const cv::FileNode error_node = storage["error"];
    if (!error_node.empty()) {
        std::string message;
        error_node["message"] >> message;
        throw std::runtime_error("Anthropic API error: " + message);
    }

    const cv::FileNode content = storage["content"];
    if (content.empty() || !content.isSeq()) {
        throw std::runtime_error(
            "Anthropic API response had no content array: " + response_body);
    }

    std::string text;
    for (const auto& block : content) {
        std::string type;
        block["type"] >> type;
        if (type == "text") {
            std::string piece;
            block["text"] >> piece;
            text += piece;
        }
    }

    if (text.empty()) {
        throw std::runtime_error(
            "Anthropic API response contained no text content: " + response_body);
    }

    return text;
}

} // namespace

AnthropicVisionTextRecognitionProvider::AnthropicVisionTextRecognitionProvider(
    std::shared_ptr<const HttpTransport> transport,
    AnthropicVisionRecognitionConfig config)
    : transport_(std::move(transport)), config_(std::move(config)) {
    if (!transport_) {
        throw std::invalid_argument(
            "AnthropicVisionTextRecognitionProvider requires a transport");
    }
}

std::string AnthropicVisionTextRecognitionProvider::build_request_body(
    const std::vector<TextRegion>& region_batch,
    const std::vector<uint8_t>& image_png) const {

    const std::string image_base64 = base64_encode(image_png);

    std::ostringstream regions_json;
    regions_json << "[";
    for (std::size_t i = 0; i < region_batch.size(); ++i) {
        const auto& region = region_batch[i];
        if (i != 0) regions_json << ",";
        regions_json << "{\"id\":\"" << json_escape(region.id) << "\","
                     << "\"x\":" << region.bounds.x << ","
                     << "\"y\":" << region.bounds.y << ","
                     << "\"width\":" << region.bounds.width << ","
                     << "\"height\":" << region.bounds.height << "}";
    }
    regions_json << "]";

    std::ostringstream body;
    body << "{"
         << "\"model\":\"" << json_escape(config_.model) << "\","
         << "\"max_tokens\":" << config_.max_tokens << ","
         << "\"system\":\"" << json_escape(kSystemInstructions) << "\","
         << "\"output_config\":{\"effort\":\"" << json_escape(config_.effort) << "\"},"
         << "\"messages\":[{\"role\":\"user\",\"content\":["
         << "{\"type\":\"image\",\"source\":{\"type\":\"base64\","
         << "\"media_type\":\"image/png\",\"data\":\"" << image_base64 << "\"}},"
         << "{\"type\":\"text\",\"text\":\"Regions (pixel coordinates in the "
         << "supplied image): " << json_escape(regions_json.str()) << "\"}"
         << "]}]"
         << "}";

    return body.str();
}

std::vector<TextRecognitionEvidence> AnthropicVisionTextRecognitionProvider::recognize(
    const cv::Mat& normalized_image,
    const std::vector<TextRegion>& text_regions,
    const std::string&,
    int) const {

    if (text_regions.empty()) {
        return {};
    }

    std::vector<uint8_t> image_png;
    if (!cv::imencode(".png", normalized_image, image_png)) {
        throw std::runtime_error("Unable to PNG-encode normalized image for recognition request");
    }

    const std::vector<HttpHeader> headers = {
        {"Content-Type", "application/json"},
        {"x-api-key", config_.api_key},
        {"anthropic-version", config_.anthropic_version},
    };

    std::vector<TextRecognitionEvidence> result;

    for (std::size_t offset = 0; offset < text_regions.size(); offset += config_.regions_per_request) {
        const std::size_t end =
            (std::min)(offset + config_.regions_per_request, text_regions.size());
        const std::vector<TextRegion> batch(
            text_regions.begin() + static_cast<std::ptrdiff_t>(offset),
            text_regions.begin() + static_cast<std::ptrdiff_t>(end));

        const std::string body = build_request_body(batch, image_png);
        const HttpResponse response = transport_->post_json(config_.endpoint, headers, body);

        if (response.transport_error) {
            throw std::runtime_error(
                "Anthropic API request failed: " + response.transport_error_message);
        }
        if (response.status_code != 200) {
            throw std::runtime_error(
                "Anthropic API request returned status " +
                std::to_string(response.status_code) + ": " + response.body);
        }

        const std::string observations_json = extract_response_text(response.body);
        auto batch_evidence = parse_recognition_observations(observations_json, provider_id());
        result.insert(
            result.end(),
            std::make_move_iterator(batch_evidence.begin()),
            std::make_move_iterator(batch_evidence.end()));
    }

    return result;
}

std::string AnthropicVisionTextRecognitionProvider::provider_id() const {
    return "anthropic-vision";
}

std::optional<AnthropicVisionRecognitionConfig>
anthropic_vision_config_from_environment() {
    const char* key = std::getenv("EKE_DX_WIRE_ANTHROPIC_API_KEY");
    if (!key || std::string(key).empty()) {
        key = std::getenv("ANTHROPIC_API_KEY");
    }
    if (!key || std::string(key).empty()) {
        return std::nullopt;
    }

    AnthropicVisionRecognitionConfig config;
    config.api_key = key;

    if (const char* model = std::getenv("EKE_DX_WIRE_ANTHROPIC_MODEL")) {
        if (std::string(model).size() > 0) {
            config.model = model;
        }
    }

    return config;
}

} // namespace eke::dx::wire

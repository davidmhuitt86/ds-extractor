#pragma once

#include "eke_dx_wire/core/http_transport.hpp"
#include "eke_dx_wire/topology/text_recognition_provider.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace eke::dx::wire {

struct AnthropicVisionRecognitionConfig {
    std::string api_key;
    std::string model = "claude-sonnet-5";
    std::string endpoint = "https://api.anthropic.com/v1/messages";
    std::string anthropic_version = "2023-06-01";
    std::string effort = "low";
    std::size_t regions_per_request = 40;
    int max_tokens = 8000;
};

/**
 * Automates the recognition hand-off that RecognitionInputExporter
 * otherwise packages for a human (or an external process) to run by hand:
 * it sends the normalized page image plus each text region's id/bounds to
 * a vision-capable Claude model and parses the response with the exact
 * same eke-dx-wire-recognition-observation contract JsonTextRecognitionProvider
 * already consumes from disk. The model is instructed with the same
 * do-not-invent-text / preserve-uncertainty rules already written for a
 * human reviewer in RecognitionInputExporter's instructions.md.
 *
 * The full normalized page is sent (not isolated per-region crops) so the
 * model has the same surrounding context a person reading the diagram
 * would have, batched across requests to bound request/response size.
 *
 * Requires network access and an API key. Callers that do not want that
 * dependency should keep using NullTextRecognitionProvider (the pipeline
 * default) or JsonTextRecognitionProvider.
 */
class AnthropicVisionTextRecognitionProvider final : public TextRecognitionProvider {
public:
    AnthropicVisionTextRecognitionProvider(
        std::shared_ptr<const HttpTransport> transport,
        AnthropicVisionRecognitionConfig config);

    [[nodiscard]] std::vector<TextRecognitionEvidence> recognize(
        const cv::Mat& normalized_image,
        const std::vector<TextRegion>& text_regions,
        const std::string& source_id,
        int page) const override;

    [[nodiscard]] std::string provider_id() const override;

    // Exposed for testing: builds the request body for one batch of
    // regions against an already-encoded image (e.g. PNG bytes).
    [[nodiscard]] std::string build_request_body(
        const std::vector<TextRegion>& region_batch,
        const std::vector<uint8_t>& image_png) const;

private:
    std::shared_ptr<const HttpTransport> transport_;
    AnthropicVisionRecognitionConfig config_;
};

/**
 * Reads EKE_DX_WIRE_ANTHROPIC_API_KEY (falling back to ANTHROPIC_API_KEY)
 * and optional EKE_DX_WIRE_ANTHROPIC_MODEL from the environment. Returns
 * std::nullopt when no key is configured, so a caller can fall back to a
 * provider that requires no network access instead of failing extraction.
 */
[[nodiscard]] std::optional<AnthropicVisionRecognitionConfig>
anthropic_vision_config_from_environment();

} // namespace eke::dx::wire

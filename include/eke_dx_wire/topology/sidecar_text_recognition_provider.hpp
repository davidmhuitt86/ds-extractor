#pragma once

#include "eke_dx_wire/topology/text_recognition_provider.hpp"

#include <string>

namespace eke::dx::wire {

/**
 * AP-WIRE-009: recognition evidence sidecar provider.
 *
 * The sidecar is deliberately simple and deterministic:
 *
 *   text-region-id<TAB>confidence<TAB>recognized text
 *
 * Blank lines and lines beginning with '#' are ignored.
 *
 * This adapter is an integration boundary for external recognition systems
 * (OCR, vision LLMs, human review, etc.). It does not perform recognition.
 */
class SidecarTextRecognitionProvider final : public TextRecognitionProvider {
public:
    explicit SidecarTextRecognitionProvider(std::string sidecar_path);

    [[nodiscard]] std::vector<TextRecognitionEvidence> recognize(
        const cv::Mat& normalized_image,
        const std::vector<TextRegion>& text_regions,
        const std::string& source_id,
        int page) const override;

    [[nodiscard]] std::string provider_id() const override;

private:
    std::string sidecar_path_;
};

} // namespace eke::dx::wire

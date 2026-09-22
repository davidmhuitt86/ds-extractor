#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/image/text_region_detector.hpp"

#include <opencv2/core.hpp>
#include <string>
#include <vector>

namespace eke::dx::wire {

/**
 * AP-WIRE-008: pluggable boundary between detected text regions and
 * recognized text evidence.
 *
 * Providers may use OCR, a local vision model, an external model service,
 * human annotation, or another recognition mechanism. The extraction
 * topology remains independent of the provider implementation.
 */
class TextRecognitionProvider {
public:
    virtual ~TextRecognitionProvider() = default;

    [[nodiscard]] virtual std::vector<TextRecognitionEvidence> recognize(
        const cv::Mat& normalized_image,
        const std::vector<TextRegion>& text_regions,
        const std::string& source_id,
        int page) const = 0;

    [[nodiscard]] virtual std::string provider_id() const = 0;
};

/**
 * Explicit no-op provider used by the default standalone pipeline.
 * Detection and topology can run without pretending that text was read.
 */
class NullTextRecognitionProvider final : public TextRecognitionProvider {
public:
    [[nodiscard]] std::vector<TextRecognitionEvidence> recognize(
        const cv::Mat& normalized_image,
        const std::vector<TextRegion>& text_regions,
        const std::string& source_id,
        int page) const override;

    [[nodiscard]] std::string provider_id() const override;
};

} // namespace eke::dx::wire

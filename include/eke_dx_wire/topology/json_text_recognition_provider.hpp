#pragma once

#include "eke_dx_wire/topology/text_recognition_provider.hpp"

#include <string>

namespace eke::dx::wire {

/**
 * AP-WIRE-011: imports recognition observations from the JSON contract
 * emitted by an external vision/OCR/reasoning system.
 *
 * Expected document:
 * {
 *   "observations": [
 *     {
 *       "text_region_id": "...",
 *       "raw_text": "...",
 *       "confidence": "high|medium|low|unresolved",
 *       "notes": "..."
 *     }
 *   ]
 * }
 *
 * The importer performs no semantic interpretation and does not mutate
 * topology. Pipeline validation remains authoritative for region identity
 * and usable confidence.
 */
class JsonTextRecognitionProvider final : public TextRecognitionProvider {
public:
    explicit JsonTextRecognitionProvider(std::string observation_path);

    [[nodiscard]] std::vector<TextRecognitionEvidence> recognize(
        const cv::Mat& normalized_image,
        const std::vector<TextRegion>& text_regions,
        const std::string& source_id,
        int page) const override;

    [[nodiscard]] std::string provider_id() const override;

private:
    std::string observation_path_;
};

} // namespace eke::dx::wire

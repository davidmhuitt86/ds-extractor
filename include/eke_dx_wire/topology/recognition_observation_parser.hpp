#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

/**
 * Parses the eke-dx-wire-recognition-observation contract:
 *
 * {
 *   "observations": [
 *     {"text_region_id": "...", "raw_text": "...", "confidence": "high|medium|low|unresolved"}
 *   ]
 * }
 *
 * Shared by every recognition provider that receives observations already
 * in this shape, whether imported from a file (JsonTextRecognitionProvider)
 * or returned by a live recognition call (AnthropicVisionTextRecognitionProvider),
 * so the contract is interpreted identically regardless of how it arrived.
 * Entries with an empty region id, empty text, or unresolved confidence are
 * dropped rather than treated as evidence.
 */
[[nodiscard]] std::vector<TextRecognitionEvidence> parse_recognition_observations(
    const std::string& json_text,
    const std::string& provider_id);

} // namespace eke::dx::wire

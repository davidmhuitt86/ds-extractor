#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

struct TextRecognitionEvidence {
    std::string text_region_id;
    std::string raw_text;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
};

class TextEvidenceInterpreter {
public:
    [[nodiscard]] std::vector<TextSemanticEvidence> interpret(
        const std::vector<TextRecognitionEvidence>& recognized_text) const;
};

} // namespace eke::dx::wire

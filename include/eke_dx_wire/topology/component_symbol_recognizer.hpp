#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <vector>

namespace eke::dx::wire {

class ComponentSymbolRecognizer {
public:
    [[nodiscard]] std::vector<ComponentSymbolRecognition> recognize(
        const std::vector<ComponentCandidate>& components) const;
};

} // namespace eke::dx::wire

#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>

namespace eke::dx::wire {

struct WireModelValidationConfig {
    // Structural graph validation is always performed. Semantic endpoint
    // classification is not required because geometric candidates are valid
    // evidence at this stage.
    bool warn_on_nonsemantic_endpoints = true;
    bool warn_on_unresolved_nets = true;
};

class WireModelValidator {
public:
    explicit WireModelValidator(
        WireModelValidationConfig config = {});

    [[nodiscard]] WireValidationReport validate(
        const WireModel& model) const;

private:
    WireModelValidationConfig config_;
};

} // namespace eke::dx::wire

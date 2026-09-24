#pragma once

#include "eke_dx_wire/core/model.hpp"

#include <string>
#include <vector>

namespace eke::dx::wire {

// A single provider observation: "this component might be this symbol
// family, with this confidence, according to this provider." It is
// evidence only - see SymbolFamilyRecognizer for how (and whether) it
// contributes to a Resolved status.
struct SymbolRecognitionObservation {
    std::string component_id;
    SymbolFamily family = SymbolFamily::Unknown;
    ConfidenceClass confidence = ConfidenceClass::Unresolved;
    std::string detail;
};

/**
 * AP-WIRE-026A: pluggable boundary between a component's already-
 * established geometry/identity and an external symbol-family
 * observation (e.g. a future vision-based recognizer). Mirrors the
 * existing TextRecognitionProvider boundary (AP-WIRE-008): the
 * deterministic extraction/recognition topology is independent of any
 * particular provider implementation, and the default is an explicit
 * no-op so the pipeline never pretends a component was recognized.
 */
class SymbolRecognitionProvider {
public:
    virtual ~SymbolRecognitionProvider() = default;

    [[nodiscard]] virtual std::vector<SymbolRecognitionObservation> recognize(
        const std::vector<ComponentCandidate>& components,
        const std::vector<ComponentSymbolGeometry>& geometries,
        const std::vector<SymbolPrimitive>& primitives,
        const std::string& source_id,
        int page) const = 0;

    [[nodiscard]] virtual std::string provider_id() const = 0;
};

class NullSymbolRecognitionProvider final : public SymbolRecognitionProvider {
public:
    [[nodiscard]] std::vector<SymbolRecognitionObservation> recognize(
        const std::vector<ComponentCandidate>& components,
        const std::vector<ComponentSymbolGeometry>& geometries,
        const std::vector<SymbolPrimitive>& primitives,
        const std::string& source_id,
        int page) const override;

    [[nodiscard]] std::string provider_id() const override;
};

} // namespace eke::dx::wire

#pragma once

#include "eke_dx_wire/core/model.hpp"
#include "eke_dx_wire/topology/symbol_recognition_provider.hpp"

#include <vector>

namespace eke::dx::wire {

struct SymbolFamilyRecognitionArtifacts {
    std::vector<SymbolFamilyEvidence> evidence;
    std::vector<SymbolFamilyResolution> resolutions;
};

/**
 * AP-WIRE-026A: resolves engineering symbol-family identity from already-
 * established evidence. Pure, deterministic, read-only with respect to
 * every input - it never mutates a ComponentCandidate, creates a
 * terminal/connector/endpoint, or touches topology/wires/electrical nets.
 * See docs/AP-WIRE-026A_Symbol_Family_Recognition.md for the exact
 * evidence and confidence rules.
 *
 * DiagramFurniture components are never considered (mirrors the same
 * exclusion AP-WIRE-023/024 already apply).
 */
class SymbolFamilyRecognizer {
public:
    [[nodiscard]] SymbolFamilyRecognitionArtifacts recognize(
        const std::vector<ComponentCandidate>& components,
        const std::vector<ComponentSymbolGeometry>& geometries,
        const std::vector<ComponentIdentityCanonicalization>& canonicalizations,
        const std::vector<SymbolRecognitionObservation>& provider_observations) const;
};

} // namespace eke::dx::wire

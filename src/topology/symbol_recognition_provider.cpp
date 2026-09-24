#include "eke_dx_wire/topology/symbol_recognition_provider.hpp"

namespace eke::dx::wire {

std::vector<SymbolRecognitionObservation>
NullSymbolRecognitionProvider::recognize(
    const std::vector<ComponentCandidate>&,
    const std::vector<ComponentSymbolGeometry>&,
    const std::vector<SymbolPrimitive>&,
    const std::string&,
    int) const {
    return {};
}

std::string NullSymbolRecognitionProvider::provider_id() const {
    return "none";
}

} // namespace eke::dx::wire

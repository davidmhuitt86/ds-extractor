#include "eke_dx_wire/topology/component_symbol_recognizer.hpp"

#include <algorithm>

namespace eke::dx::wire {

namespace {

ComponentSymbolKind symbol_kind(ComponentCandidateKind kind) {
    switch (kind) {
    case ComponentCandidateKind::Enclosure:
        return ComponentSymbolKind::Enclosure;
    case ComponentCandidateKind::CircularSymbol:
        return ComponentSymbolKind::CircularSymbol;
    case ComponentCandidateKind::ChassisGround:
        return ComponentSymbolKind::ChassisGround;
    case ComponentCandidateKind::PrimitiveSymbol:
        return ComponentSymbolKind::PrimitiveSymbol;
    case ComponentCandidateKind::DiagramFurniture:
        return ComponentSymbolKind::DiagramFurniture;
    case ComponentCandidateKind::Unknown:
        return ComponentSymbolKind::Unknown;
    }
    return ComponentSymbolKind::Unknown;
}

// This recognizer only carries the geometric candidate bucket across the
// model boundary; it does not classify symbol identity, so a non-Unknown
// kind is reported as GeometricallyClassified rather than Recognized.
ComponentSymbolRecognitionStatus status_for(ComponentCandidateKind kind) {
    return kind == ComponentCandidateKind::Unknown
        ? ComponentSymbolRecognitionStatus::Unresolved
        : ComponentSymbolRecognitionStatus::GeometricallyClassified;
}

std::string make_id(const std::string& component_id) {
    return "component-symbol-recognition-" + component_id;
}

} // namespace

std::vector<ComponentSymbolRecognition> ComponentSymbolRecognizer::recognize(
    const std::vector<ComponentCandidate>& components) const {

    std::vector<ComponentSymbolRecognition> result;
    result.reserve(components.size());

    for (const auto& component : components) {
        if (component.id.empty())
            continue;

        ComponentSymbolRecognition recognition;
        recognition.id = make_id(component.id);
        recognition.component_id = component.id;
        recognition.symbol_kind = symbol_kind(component.kind);
        recognition.confidence = component.confidence;
        recognition.status = status_for(component.kind);
        recognition.shape_ids = component.shape_ids;
        result.push_back(std::move(recognition));
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const auto& a, const auto& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

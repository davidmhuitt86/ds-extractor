#include "eke_dx_wire/topology/component_symbol_recognizer.hpp"

#include <cassert>

using namespace eke::dx::wire;

int main() {
    ComponentCandidate enclosure;
    enclosure.id = "component-enclosure";
    enclosure.kind = ComponentCandidateKind::Enclosure;
    enclosure.confidence = ConfidenceClass::High;
    enclosure.shape_ids = {"shape-enclosure"};

    ComponentCandidate circular;
    circular.id = "component-circular";
    circular.kind = ComponentCandidateKind::CircularSymbol;
    circular.confidence = ConfidenceClass::Medium;
    circular.shape_ids = {"shape-circular"};

    ComponentCandidate ground;
    ground.id = "component-ground";
    ground.kind = ComponentCandidateKind::ChassisGround;
    ground.confidence = ConfidenceClass::High;
    ground.shape_ids = {"shape-ground"};

    ComponentCandidate primitive;
    primitive.id = "component-primitive";
    primitive.kind = ComponentCandidateKind::PrimitiveSymbol;
    primitive.confidence = ConfidenceClass::Low;
    primitive.shape_ids = {"shape-primitive"};

    ComponentCandidate unknown;
    unknown.id = "component-unknown";
    unknown.kind = ComponentCandidateKind::Unknown;
    unknown.confidence = ConfidenceClass::Unresolved;

    ComponentSymbolRecognizer recognizer;
    const auto result = recognizer.recognize(
        {unknown, primitive, ground, circular, enclosure});

    assert(result.size() == 5);

    const auto find = [&](const std::string& id)
        -> const ComponentSymbolRecognition* {
        for (const auto& item : result) {
            if (item.component_id == id)
                return &item;
        }
        return nullptr;
    };

    const auto* enclosure_result = find(enclosure.id);
    assert(enclosure_result != nullptr);
    assert(enclosure_result->symbol_kind == ComponentSymbolKind::Enclosure);
    assert(enclosure_result->status ==
           ComponentSymbolRecognitionStatus::GeometricallyClassified);
    assert(enclosure_result->confidence == ConfidenceClass::High);
    assert(enclosure_result->shape_ids.size() == 1);

    const auto* circular_result = find(circular.id);
    assert(circular_result != nullptr);
    assert(circular_result->symbol_kind == ComponentSymbolKind::CircularSymbol);
    assert(circular_result->status ==
           ComponentSymbolRecognitionStatus::GeometricallyClassified);

    const auto* ground_result = find(ground.id);
    assert(ground_result != nullptr);
    assert(ground_result->symbol_kind == ComponentSymbolKind::ChassisGround);

    const auto* primitive_result = find(primitive.id);
    assert(primitive_result != nullptr);
    assert(primitive_result->symbol_kind == ComponentSymbolKind::PrimitiveSymbol);
    assert(primitive_result->confidence == ConfidenceClass::Low);

    const auto* unknown_result = find(unknown.id);
    assert(unknown_result != nullptr);
    assert(unknown_result->symbol_kind == ComponentSymbolKind::Unknown);
    assert(unknown_result->status == ComponentSymbolRecognitionStatus::Unresolved);

    for (std::size_t i = 1; i < result.size(); ++i)
        assert(result[i - 1].id < result[i].id);

    return 0;
}

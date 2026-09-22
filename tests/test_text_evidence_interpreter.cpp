#include "eke_dx_wire/topology/text_evidence_interpreter.hpp"

#include <cassert>

using namespace eke::dx::wire;

int main() {
    const auto result = TextEvidenceInterpreter().interpret({
        {"text-1", "GND", ConfidenceClass::High},
        {"text-2", "B+", ConfidenceClass::Medium},
        {"text-3", "shared-function-feed", ConfidenceClass::High},
        {"text-4", "IGN", ConfidenceClass::High},
        {"text-5", "red", ConfidenceClass::High},
        {"text-6", "connector C1", ConfidenceClass::Medium},
        {"text-7", "pin 3", ConfidenceClass::Medium},
        {"text-8", "arbitrary label", ConfidenceClass::High},
        {"text-9", "GND", ConfidenceClass::Unresolved}
    });

    assert(result.size() == 8);

    assert(result[0].text_region_id == "text-1");
    assert(result[0].kind == TextSemanticKind::GroundLabel);
    assert(result[0].normalized_text == "GND");

    bool saw_power = false;
    bool saw_shared = false;
    bool saw_function = false;
    bool saw_color = false;
    bool saw_connector = false;
    bool saw_terminal = false;
    bool saw_unknown = false;

    for (const auto& evidence : result) {
        saw_power |= evidence.kind == TextSemanticKind::PowerFeedLabel;
        saw_shared |= evidence.kind == TextSemanticKind::SharedFunctionFeedLabel;
        saw_function |= evidence.kind == TextSemanticKind::FunctionLabel;
        saw_color |= evidence.kind == TextSemanticKind::WireColorLabel;
        saw_connector |= evidence.kind == TextSemanticKind::ConnectorLabel;
        saw_terminal |= evidence.kind == TextSemanticKind::TerminalLabel;
        saw_unknown |= evidence.kind == TextSemanticKind::Unknown;
    }

    assert(saw_power);
    assert(saw_shared);
    assert(saw_function);
    assert(saw_color);
    assert(saw_connector);
    assert(saw_terminal);
    assert(saw_unknown);

    return 0;
}

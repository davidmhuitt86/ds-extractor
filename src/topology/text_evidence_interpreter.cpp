#include "eke_dx_wire/topology/text_evidence_interpreter.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace eke::dx::wire {
namespace {

std::string normalize_text(const std::string& value) {
    std::string result;
    result.reserve(value.size());

    bool pending_space = false;
    for (const unsigned char ch : value) {
        if (std::isspace(ch) || ch == '-' || ch == '_') {
            if (!result.empty()) {
                pending_space = true;
            }
            continue;
        }

        if (pending_space) {
            result.push_back(' ');
            pending_space = false;
        }

        result.push_back(
            static_cast<char>(std::toupper(ch)));
    }

    while (!result.empty() &&
           (result.back() == '.' ||
            result.back() == ',' ||
            result.back() == ':' ||
            result.back() == ';')) {
        result.pop_back();
    }

    return result;
}

TextSemanticKind classify(const std::string& normalized) {
    if (normalized == "GND" ||
        normalized == "GROUND" ||
        normalized == "CHASSIS GROUND") {
        return TextSemanticKind::GroundLabel;
    }

    if (normalized == "B+" ||
        normalized == "BATTERY+" ||
        normalized == "BATTERY POSITIVE" ||
        normalized == "POWER FEED" ||
        normalized == "POWER SOURCE") {
        return TextSemanticKind::PowerFeedLabel;
    }

    if (normalized == "SHARED FUNCTION FEED") {
        return TextSemanticKind::SharedFunctionFeedLabel;
    }

    if (normalized == "RED" ||
        normalized == "BLACK" ||
        normalized == "WHITE" ||
        normalized == "GREEN" ||
        normalized == "BLUE" ||
        normalized == "YELLOW" ||
        normalized == "BROWN" ||
        normalized == "ORANGE" ||
        normalized == "PINK" ||
        normalized == "GRAY" ||
        normalized == "GREY" ||
        normalized == "PURPLE") {
        return TextSemanticKind::WireColorLabel;
    }

    if (normalized == "IGN" ||
        normalized == "IGNITION" ||
        normalized == "START" ||
        normalized == "STARTER" ||
        normalized == "LIGHT" ||
        normalized == "LIGHTING" ||
        normalized == "HEADLIGHT" ||
        normalized == "BRAKE" ||
        normalized == "TURN" ||
        normalized == "HORN" ||
        normalized == "NEUTRAL" ||
        normalized == "OIL" ||
        normalized == "CHARGE") {
        return TextSemanticKind::FunctionLabel;
    }

    // These are deliberately conservative lexical classes. They do not
    // assert that an arbitrary nearby text region is a particular object.
    if (normalized.rfind("CONNECTOR ", 0) == 0 ||
        normalized.rfind("CONN ", 0) == 0) {
        return TextSemanticKind::ConnectorLabel;
    }

    if (normalized.rfind("TERMINAL ", 0) == 0 ||
        normalized.rfind("PIN ", 0) == 0) {
        return TextSemanticKind::TerminalLabel;
    }

    return TextSemanticKind::Unknown;
}

} // namespace

std::vector<TextSemanticEvidence>
TextEvidenceInterpreter::interpret(
    const std::vector<TextRecognitionEvidence>& recognized_text) const {

    std::vector<TextSemanticEvidence> result;

    for (const auto& observation : recognized_text) {
        if (observation.text_region_id.empty() ||
            observation.raw_text.empty() ||
            observation.confidence == ConfidenceClass::Unresolved) {
            continue;
        }

        const std::string normalized =
            normalize_text(observation.raw_text);
        if (normalized.empty()) {
            continue;
        }

        const TextSemanticKind kind = classify(normalized);

        TextSemanticEvidence evidence;
        evidence.id =
            observation.text_region_id + ":semantic:" + normalized;
        evidence.text_region_id = observation.text_region_id;
        evidence.raw_text = observation.raw_text;
        evidence.normalized_text = normalized;
        evidence.kind = kind;
        evidence.confidence = observation.confidence;
        evidence.source =
            "lexical-text-interpretation:" +
            (observation.provider.empty()
                ? "unspecified"
                : observation.provider);

        result.push_back(std::move(evidence));
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const TextSemanticEvidence& a,
           const TextSemanticEvidence& b) {
            if (a.text_region_id != b.text_region_id) {
                return a.text_region_id < b.text_region_id;
            }
            return a.id < b.id;
        });

    return result;
}

} // namespace eke::dx::wire

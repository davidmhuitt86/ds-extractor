#pragma once

#include "eke_dx_wire/ingest/extraction_scope.hpp"

#include <string>

namespace eke::dx::wire {

// AP-INGEST-001: deterministic JSON (de)serialization for ExtractionScope.
// Reading follows the same cv::FileStorage(MEMORY | FORMAT_JSON) approach
// already used by RecognitionObservationParser; writing follows the
// hand-rolled, escaped-string convention already used throughout
// src/export/ - this project has no third-party JSON dependency and this
// stays consistent with that.
class ExtractionScopeIO {
public:
    // Throws std::runtime_error if the file cannot be opened or parsed.
    [[nodiscard]] static ExtractionScope load(const std::string& path);

    // Throws std::runtime_error if json_text is not valid JSON.
    [[nodiscard]] static ExtractionScope parse(const std::string& json_text);

    // Throws std::runtime_error if the file cannot be created.
    static void save(const ExtractionScope& scope, const std::string& path);

    [[nodiscard]] static std::string serialize(const ExtractionScope& scope);
};

} // namespace eke::dx::wire
